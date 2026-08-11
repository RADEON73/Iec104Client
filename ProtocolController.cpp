#include "ProtocolController.h"

#include <AppSettings.h>
#include <functional>
#include <qabstractsocket.h>
#include <qdatetime.h>
#include <qmetatype.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qregularexpression.h>
#include <qthread.h>
#include "iec104/iec104_class.h"
#include "QIec104.h"

ProtocolController::ProtocolController(QObject *parent) 
    : QObject(parent),
    m_i104(new QIec104())
{
    qRegisterMetaType<QAbstractSocket::SocketState>();

    connect(m_i104, &QIec104::signal_stateChanged, this, &ProtocolController::signal_stateChanged);
    connect(m_i104, &QIec104::signal_commandActRespIndication, this, &ProtocolController::signal_commandActRespIndication);
    connect(m_i104, &QIec104::signal_dataIndication, this, &ProtocolController::signal_dataIndication);

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
    updateSettings();

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

void ProtocolController::updateSettings()
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

void ProtocolController::queueProtocolCommand(const iec_obj& obj)
{
    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104, command = obj]() mutable { worker->sendCommand(&command); },
        Qt::QueuedConnection);
}

void ProtocolController::queueProtocolCall(const std::function<void(QIec104*)>& fn)
{
    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104, fn]() { fn(worker); },
        Qt::QueuedConnection);
}

void ProtocolController::slot_reloadProtocolSettings()
{
    updateSettings();
}

void ProtocolController::request_SendData(CommandData data)
{

    auto parseIoa = [](const QString& str) {
        if (str.contains('-') || str.contains('.')) {
            QStringList parts = str.split(QRegularExpression("[-.]"));
            if (parts.size() >= 3) {
                unsigned int low = parts[0].toUInt();
                unsigned int mid = parts[1].toUInt();
                unsigned int high = parts[2].toUInt();
                return low + (mid << 8) + (high << 16);
            }
        }
        return str.toUInt();
        };

    iec_obj obj = {};
    obj.type = static_cast<unsigned char>(data.cbCmdAsdu.left(data.cbCmdAsdu.indexOf(':')).toUInt());

    if (obj.type == iec104_class::C_RD_NA_1) {
        if (data.cb888Mode) {
            if (data.leCmdAddressLow.trimmed() == "" &&
                data.leCmdAddressMid.trimmed() == "" &&
                data.leCmdAddressHigh.trimmed() == "")
                return;
            obj.address = data.leCmdAddressLow.toUInt() +
                (data.leCmdAddressMid.toUInt() << 8) +
                (data.leCmdAddressHigh.toUInt() << 16);
        }
        else {
            if (data.leCmdAddress.trimmed() == "")
                return;
            obj.address = parseIoa(data.leCmdAddress);
        }
        obj.value = 0;
    }
    else
        // reset process and interrogation must have value set (qrp) or (qoi)
        if (obj.type == iec104_class::C_RP_NA_1 ||
            obj.type == iec104_class::C_IC_NA_1 ||
            obj.type == iec104_class::C_CI_NA_1) {
            if (data.leCmdValue.trimmed() == "")
                return;
            obj.address = 0;
            obj.value = data.leCmdValue.toDouble();
        }
        else
            // test command parameters if not sync command or test command (that don't
            // have parameters)
            if (obj.type != iec104_class::C_CS_NA_1 &&
                obj.type != iec104_class::C_TS_TA_1) {
                if (data.leCmdValue.trimmed() == "")
                    return;
                unsigned int parsedAddr = 0;
                if (data.cb888Mode) {
                    if (data.leCmdAddressLow.trimmed() == "" &&
                        data.leCmdAddressMid.trimmed() == "" &&
                        data.leCmdAddressHigh.trimmed() == "")
                        return;
                    parsedAddr = data.leCmdAddressLow.toUInt() +
                        (data.leCmdAddressMid.toUInt() << 8) +
                        (data.leCmdAddressHigh.toUInt() << 16);
                }
                else {
                    if (data.leCmdAddress.trimmed() == "")
                        return;
                    parsedAddr = parseIoa(data.leCmdAddress);
                }
                if (parsedAddr == 0) return;

                obj.address = parsedAddr;
                obj.value = data.leCmdValue.toDouble();
            }

    obj.ca = data.leASDUAddr.toUShort();

    queueProtocolCall([ca = obj.ca](QIec104* worker) {
        worker->setSecondaryASDUAddress(ca);
        });

    QDateTime current = QDateTime::currentDateTime();

    switch (obj.type) {
    case iec104_class::C_IC_NA_1: // Interrogation
        queueProtocolCall([group = data.leCmdValue.toInt()](QIec104* worker) {
            worker->solicitInterrogation(static_cast<char>(group));
            });
        return;
    case iec104_class::C_SC_NA_1:
    case iec104_class::C_SC_TA_1:
        obj.scs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_DC_NA_1:
    case iec104_class::C_DC_TA_1:
        obj.dcs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_RC_NA_1:
    case iec104_class::C_RC_TA_1:
        obj.rcs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_SE_NA_1:
    case iec104_class::C_SE_TA_1:
        obj.value = data.leCmdValue.toInt();
        break;
    case iec104_class::C_SE_NB_1:
    case iec104_class::C_SE_TB_1:
        obj.value = data.leCmdValue.toInt();
        break;
    case iec104_class::C_SE_NC_1:
    case iec104_class::C_SE_TC_1:
        obj.value = data.leCmdValue.toDouble();
        break;
    case iec104_class::C_CS_NA_1:
        obj.timetag.year = static_cast<uint8_t>(current.date().year() % 100);
        obj.timetag.month = static_cast<uint8_t>(current.date().month());
        obj.timetag.mday = static_cast<uint8_t>(current.date().day());
        obj.timetag.hour = static_cast<uint8_t>(current.time().hour());
        obj.timetag.min = static_cast<uint8_t>(current.time().minute());
        obj.timetag.msec = static_cast<uint16_t>(
            current.time().second() * 1000 + current.time().msec());
        obj.timetag.iv = 0;
        obj.timetag.su = static_cast<uint8_t>(current.isDaylightTime());
        obj.timetag.wday = static_cast<uint8_t>(current.date().dayOfWeek());;
        obj.timetag.res1 = 0;
        obj.timetag.res2 = 0;
        obj.timetag.res3 = 0;
        obj.timetag.res4 = 0;
        break;
    case iec104_class::C_TS_TA_1:
        obj.timetag.year = static_cast<uint8_t>(current.date().year() % 100);
        obj.timetag.month = static_cast<uint8_t>(current.date().month());
        obj.timetag.mday = static_cast<uint8_t>(current.date().day());
        obj.timetag.hour = static_cast<uint8_t>(current.time().hour());
        obj.timetag.min = static_cast<uint8_t>(current.time().minute());
        obj.timetag.msec = static_cast<uint16_t>(
            current.time().second() * 1000 + current.time().msec());
        obj.timetag.iv = 0;
        obj.timetag.su = static_cast<uint8_t>(current.isDaylightTime());
        obj.timetag.wday = static_cast<uint8_t>(current.date().dayOfWeek());;
        obj.timetag.res1 = 0;
        obj.timetag.res2 = 0;
        obj.timetag.res3 = 0;
        obj.timetag.res4 = 0;
        break;
    case iec104_class::C_RD_NA_1:
        break;
    case iec104_class::C_RP_NA_1:
        break;
    case iec104_class::P_ME_NA_1:
        obj.value = data.leCmdValue.toInt();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NB_1:
        obj.value = data.leCmdValue.toInt();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NC_1:
        obj.value = data.leCmdValue.toDouble();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_AC_NA_1:
        obj.value = data.leCmdValue.toInt();
        obj.qpa = data.leCmdValue.toInt();
        break;
    case iec104_class::C_CI_NA_1:
        obj.value = data.leCmdValue.toInt();
        obj.frz = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        break;
    }
    obj.qu = static_cast<unsigned char>(
        data.cbCmdDuration.left(1).toUInt());
    obj.se = static_cast<unsigned char>(data.cbSBO);

    queueProtocolCommand(obj);
}
