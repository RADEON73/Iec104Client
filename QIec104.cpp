#include "QIec104.h"

#include <cstdio>
#include <qabstractsocket.h>
#include <qglobal.h>
#include <qhostaddress.h>
#include <qiodevice.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qstring.h>
#include <qtcpsocket.h>
#include <qtimer.h>
#include <qvector.h>
#include <string>
#include "iec104/iec104_class.h"
#include <qthread.h>

QIec104::QIec104(QObject* parent) 
    : QObject(parent),
    m_tmKeepAlive(new QTimer(this)),
    m_tcps_reconnect(new QTimer(this)),
    m_tcps(new QTcpSocket(this))
{
    mLog.activateLog();
    mLog.doLogTime();

    m_tcps->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(m_tcps, &QTcpSocket::stateChanged, this, &QIec104::signal_stateChanged);
    connect(m_tcps, &QTcpSocket::readyRead, this, &QIec104::slot_tcpReadyRead);
    connect(m_tcps, &QTcpSocket::connected, this, &QIec104::slot_tcpconnect);
    connect(m_tcps, &QTcpSocket::disconnected, this, &QIec104::slot_tcpdisconnect);
    connect(m_tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_tcperror, Qt::DirectConnection);

    connect(m_tmKeepAlive, &QTimer::timeout, this, &QIec104::slot_keep_alive);

    m_tcps_reconnect->setSingleShot(true);
    connect(m_tcps_reconnect, &QTimer::timeout, this, &QIec104::slot_reconnect);
}

QIec104::~QIec104() = default;

void QIec104::connectTcp()
{
    connectTCP();
}

void QIec104::disconnectTcp()
{
    disconnectTCP();
}

void QIec104::waitBytes(int bytes, int msTout)
{
    while (m_tcps->bytesAvailable() < bytes && msTout > 0) {
        m_tcps->waitForReadyRead(8);
        msTout -= 8;
    }
}

void QIec104::dataIndication(iec_obj* obj, unsigned numpoints)
{
    QVector<iec_obj> objects;
    objects.reserve(static_cast<qsizetype>(numpoints));
    for (unsigned i = 0; i < numpoints; ++i) {
        objects.append(obj[i]);
    }
    emit signal_dataIndication(objects);
}

void QIec104::connectTCP()
{
    auto state = m_tcps->state();

    if (state != QAbstractSocket::UnconnectedState) {
        mLog.pushMsg("TCP соединение не отключено.");
        return;
    }

    m_reconnectEnabled = true;

    m_tcps->abort();

    if (!m_shutdownRequested) {
        mConnectAttemptCounter++;
        char* ipAddr = nullptr;
        if (mConnectAttemptCounter % 2 || strcmp(getSecondaryIP_backup(), "0.0.0.0") == 0)
            ipAddr = getSecondaryIP();
        else
            ipAddr = getSecondaryIP_backup();
        m_tcps->connectToHost(ipAddr, quint16(getPortTCP()), QIODevice::ReadWrite);
        auto msg = QString("Попытка подключения к IP: %1").arg(ipAddr);
        mLog.pushMsg(msg.toUtf8().constData());
    }

    onConnectTCP();
}

void QIec104::disconnectTCP() 
{ 
    auto state = m_tcps->state();

    m_reconnectEnabled = false;
    m_tcps_reconnect->stop();
    m_tcps->close();
    onDisconnectTCP();
}

QAbstractSocket::SocketState QIec104::connectionState() const
{
    return m_tcps->state();
}

void QIec104::slot_tcperror(QAbstractSocket::SocketError socketError)
{
    if (socketError != QAbstractSocket::SocketTimeoutError) {
        auto msg = QString("Ошибка сокета: %1").arg(socketError);
        mLog.pushMsg(msg.toUtf8().constData());
    }
    else {
        mLog.pushMsg(m_tcps->errorString().toStdString().c_str());
    }
    m_tmKeepAlive->stop();
    if (!m_shutdownRequested && m_reconnectEnabled) {
        m_tcps_reconnect->start(2000);
        mLog.pushMsg("!!!!!Переподключение через 2 сек.!");
    }
}

void QIec104::slot_keep_alive()
{
    onTimerSecond();
}

int QIec104::readTCP(char* buf, int szmax)
{
    int ret = int(m_tcps->read(buf, szmax));

    if (!m_shutdownRequested && ret > 0)
        return ret;
    else
        return 0;
}

// send tcp data, user provided
void QIec104::sendTCP(char* data, int sz)
{
    if (m_tcps->state() == QAbstractSocket::ConnectedState)
        if (!m_shutdownRequested) {
            m_tcps->write(data, sz);
            m_tcps->flush();
            if (mLog.isLogging())
                LogFrame(data, sz, true);
        }
}

void QIec104::slot_tcpconnect()
{
    m_tmKeepAlive->start(1000);
    m_tcps_reconnect->stop();
    mLog.pushMsg("TCP соединение установлено.");
}

void QIec104::slot_tcpdisconnect()
{
    mLog.pushMsg("TCP соединение разорвано.");

    m_tmKeepAlive->stop();
    if (!m_shutdownRequested && m_reconnectEnabled) {
        m_tcps_reconnect->start(2000);
        mLog.pushMsg("!!!!!Переподключение через 2 сек.!");
    }
}

void QIec104::slot_reconnect()
{
    if (m_shutdownRequested)
        return;

    if (m_tcps->state() == QAbstractSocket::UnconnectedState) {
        mLog.pushMsg("!!!!!Выполняется попытка переподключения...!");
        connectTCP();
    }
}

void QIec104::commandActRespIndication(iec_obj* obj)
{
    emit signal_commandActRespIndication(*obj);
}

void QIec104::terminate()
{
    m_shutdownRequested = true;
    m_tcps_reconnect->stop();
    m_tmKeepAlive->stop();
    m_tcps->abort();
    mLog.pushMsg("!!!!!Работа прервана...!");
}

void QIec104::slot_tcpReadyRead()
{
    if (m_tcps->bytesAvailable() < 6)
        m_tcps->waitForReadyRead(8);

    packetReadyTCP();
}

int QIec104::bytesAvailableTCP() 
{ 
    return int(m_tcps->bytesAvailable());
}