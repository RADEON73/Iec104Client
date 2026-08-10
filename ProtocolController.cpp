#include "ProtocolController.h"

#include <AppSettings.h>
#include <functional>
#include <qabstractsocket.h>
#include <qmetatype.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include "qiec104.h"

ProtocolController::ProtocolController(QObject *parent) 
    : QObject(parent),
    m_i104(new QIec104())
{
    qRegisterMetaType<QAbstractSocket::SocketState>();

    connect(m_i104, &QIec104::stateChanged, this, &ProtocolController::stateChanged);
    connect(m_i104, &QIec104::signal_commandActRespIndication, this, &ProtocolController::signal_commandActRespIndication);

    m_i104->moveToThread(&m_protocolThread);
    connect(m_i104, &QObject::destroyed, &m_protocolThread, &QThread::quit, Qt::DirectConnection);
    m_protocolThread.start();
}

ProtocolController::~ProtocolController()
{
    shutdown();
}

QIec104* ProtocolController::i104()
{
    return m_i104;
}

void ProtocolController::request_GI()
{
    queueProtocolCall([](QIec104* worker) {
        worker->solicitGI();
        });
}

void ProtocolController::request_Connect()
{
    reloadProtocolSettings();

    queueProtocolCall([](QIec104* worker) {
        switch (worker->connectionState()) {
        case QAbstractSocket::UnconnectedState:
            worker->connectTcp();
            return;
        default:
            worker->disconnectTcp();
            break;
        }
        });
}

void ProtocolController::shutdown()
{
    if (!m_protocolThread.isRunning())
        return;

    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104]() {
            worker->terminate();
            worker->deleteLater();
        },
        Qt::BlockingQueuedConnection
    );

    m_protocolThread.wait();

    m_i104 = nullptr;

}

void ProtocolController::queueProtocolCall(const std::function<void(QIec104*)>& fn)
{
    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104, fn]() { fn(worker); },
        Qt::QueuedConnection);
}

void ProtocolController::reloadProtocolSettings()
{
    auto& s = AppSettings::instance();

    queueProtocolCall([&s](QIec104* worker) {
        worker->setSecondaryIP(const_cast<char*>(s.IpAddress.toStdString().c_str()));
        worker->setSecondaryIP_backup(const_cast<char*>(s.IpAddressReserve.toStdString().c_str()));
        worker->setPortTCP(s.TcpPort);

        worker->setSecondaryAddress(s.CA);
        worker->setPrimaryAddress(s.OA);

        worker->setGIPeriod(s.GIperiod);
        });
}