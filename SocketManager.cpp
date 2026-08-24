#include "SocketManager.h"

#include <AppSettings.h>
#include <cstdint>
#include <functional>
#include <qabstractsocket.h>
#include <qdatetime.h>
#include <qlogging.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qregularexpression.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtimer.h>
#include "iec104/iec104_class.h"
#include "Iec104TcpSocket.h"
#include <qthread.h>
#include "iec104/iec104_log.h"

SocketManager::SocketManager(QObject* parent)
    : QObject(parent)
    , m_socket(new Iec104Socket())
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);

    // Подключаем сигналы сокета
    connect(m_socket, &Iec104Socket::connected, this, &SocketManager::onSocketConnected);
    connect(m_socket, &Iec104Socket::disconnected, this, &SocketManager::onSocketDisconnected);
    connect(m_socket, &Iec104Socket::errorOccurred, this, &SocketManager::onSocketError);

    connect(m_socket, &Iec104Socket::signal_dataIndication, this, &SocketManager::signal_dataIndication);
    connect(m_socket, &Iec104Socket::signal_interrogationActConfIndication, this, &SocketManager::signal_interrogationActConfIndication);
    connect(m_socket, &Iec104Socket::signal_interrogationActTermIndication, this, &SocketManager::signal_interrogationActTermIndication);
    connect(m_socket, &Iec104Socket::signal_commandActRespIndication, this, &SocketManager::signal_commandActRespIndication);
    connect(m_socket, &Iec104Socket::signal_userprocAPDU, this, &SocketManager::signal_userprocAPDU);

    // Таймер для переподключения
    connect(m_reconnectTimer, &QTimer::timeout, this, &SocketManager::onReconnectTimer);

    m_socket->moveToThread(&m_protocolThread);
    connect(&m_protocolThread, &QThread::finished, m_socket, &QObject::deleteLater);
    m_protocolThread.start();
}

SocketManager::~SocketManager()
{
    m_stopRequested = true;
    m_reconnectTimer->stop();

    shutdownProtocolThread();
}

void SocketManager::start()
{
    auto& s = AppSettings::instance();

    m_primaryServer = ServerConfig{s.IpAddress, s.TcpPort};
    m_backupServer = ServerConfig{ s.IpAddressReserve, s.TcpPortReserve };
    m_hasBackupServer = (s.IpAddressReserve != "0.0.0.0");

    if (m_state != State::Idle) {
        qDebug() << "SocketManager: Already running";
        return;
    }

    if (m_primaryServer.host.isEmpty()) {
        qDebug() << "SocketManager: Primary server not configured!";
        return;
    }

    m_stopRequested = false;
    m_connectionAttempts = 0;
    m_currentServer = ServerType::Primary;
    m_state = State::Connecting;

    requestUpdate();

    qDebug() << "SocketManager: Starting connection to primary server";

    connectTo(ServerType::Primary);
}

void SocketManager::stop()
{
    if (m_stopRequested && m_state == State::Idle)
        return;

    qDebug() << "SocketManager: Stop requested";

    m_state = State::Idle;
    m_stopRequested = true;
    m_reconnectTimer->stop();

    requestAbort();
}

void SocketManager::reconnect()
{
    if (m_state == State::Idle) {
        start();
        return;
    }
    qDebug() << "SocketManager: Manual reconnect requested";
    m_reconnectTimer->stop();
    requestAbort();
}

iec104_log* SocketManager::logQueue()
{ 
    return m_socket->logQueue(); 
}

bool SocketManager::isConnected() const
{
    return m_state == State::Connected;
}

SocketManager::ServerType SocketManager::currentServer() const
{
    return m_currentServer;
}

void SocketManager::connectTo(ServerType server)
{
    if (m_stopRequested) {
        qDebug() << "SocketManager: Stop requested, aborting connection";
        return;
    }

    ServerConfig config;
    QString serverName;

    if (server == ServerType::Primary) {
        config = m_primaryServer;
        serverName = "Primary";
    }
    else {
        if (!m_hasBackupServer) {
            qDebug() << "SocketManager: No backup server configured!";
            emit errorOccurred("No backup server configured");
            return;
        }
        config = m_backupServer;
        serverName = "Backup";
    }

    m_state = State::Connecting;

    qDebug() << QString("SocketManager: Connecting to %1 server %2:%3")
        .arg(serverName, config.host)
        .arg(config.port);

    requestConnect(config.host, config.port);
}

void SocketManager::switchToNextServer()
{
    if (m_stopRequested)
        return;

    // Увеличиваем счетчик попыток
    m_connectionAttempts++;

    // Если слишком много попыток к одному серверу - переключаемся
    const int MAX_ATTEMPTS = 3;

    if (m_currentServer == ServerType::Primary && m_connectionAttempts >= MAX_ATTEMPTS) {
        if (m_hasBackupServer) {
            qDebug() << "SocketManager: Switching to backup server";
            m_currentServer = ServerType::Backup;
            m_connectionAttempts = 0;
            scheduleReconnect(500); // Пауза перед подключением к бэкапу
            return;
        }
    }

    if (m_currentServer == ServerType::Backup && m_connectionAttempts >= MAX_ATTEMPTS) {
        qDebug() << "SocketManager: Switching back to primary server";
        m_currentServer = ServerType::Primary;
        m_connectionAttempts = 0;
        scheduleReconnect(500); // Пауза перед подключением к primary
        return;
    }

    // Пробуем подключиться к текущему серверу снова
    scheduleReconnect(1000);
}

void SocketManager::scheduleReconnect(int delayMs)
{
    if (m_stopRequested)
        return;

    m_state = State::Reconnecting;
    qDebug() << QString("SocketManager: Scheduling reconnect in %1 ms").arg(delayMs);
    m_reconnectTimer->start(delayMs);
}

void SocketManager::resetConnectionAttempts()
{
    m_connectionAttempts = 0;
}

void SocketManager::shutdownProtocolThread()
{
    if (!m_protocolThread.isRunning())
        return;

    QMetaObject::invokeMethod(
        m_socket,
        [socket = m_socket]() {
            socket->abort();
        },
        Qt::BlockingQueuedConnection
    );

    m_protocolThread.quit();
    m_protocolThread.wait();
}

void SocketManager::onSocketConnected()
{
    if (m_stopRequested) {
        qDebug() << "SocketManager: Connected but stop requested";
        requestAbort();
        return;
    }

    m_state = State::Connected;
    m_connectionAttempts = 0;

    const QString serverName =
        (m_currentServer == ServerType::Primary)
        ? "Primary"
        : "Backup";

    qDebug()
        << QString("SocketManager: Connected to %1 server")
        .arg(serverName);

    emit connected(m_currentServer);
}

void SocketManager::onSocketDisconnected()
{
    qDebug() << "SocketManager: Disconnected";

    if (m_state == State::Connected)
        emit disconnected();

    if (m_stopRequested) {
        m_state = State::Idle;
        emit stopped();
        return;
    }

    switchToNextServer();
}

void SocketManager::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);

    if (m_stopRequested)
        return;

    if (m_state == State::Connecting) {
        qDebug() << "SocketManager: Connection error";
        switchToNextServer();
        return;
    }

    if (m_state == State::Connected) {
        qDebug() << "SocketManager: Socket error while connected";
        requestAbort();
    }
}

void SocketManager::onReconnectTimer()
{
    if (m_stopRequested)
        return;
    if (m_state != State::Reconnecting)
        return;
    connectTo(m_currentServer);
}

void SocketManager::requestUpdate()
{
    auto& s = AppSettings::instance();

    const auto ip = s.IpAddress.toStdString();
    const auto port = s.TcpPort;
    const auto backupIp = s.IpAddressReserve.toStdString();
    const auto backupPort = s.TcpPortReserve;
    const auto ca = s.CA;
    const auto oa = s.OA;
    const auto giPeriod = s.GIperiod;

    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket,
        ip,
        port,
        backupIp,
        backupPort,
        ca,
        oa,
        giPeriod]() {
            socket->setIP(ip);
            socket->setPort(port);
            socket->setIP_backup(backupIp);
            socket->setPort_backup(backupPort);
            socket->setSecondaryAddress(ca);
            socket->setPrimaryAddress(oa);
            socket->setGIPeriod(giPeriod);
        },
        Qt::QueuedConnection);
}

void SocketManager::requestConnect(QString ip, quint16 port)
{
    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket, ip = std::move(ip), port]() {
            socket->connectTCP(ip.toStdString(), port);
        },
        Qt::QueuedConnection);
}

void SocketManager::requestDisconnect()
{
    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket]() {
            socket->disconnectTCP();
        },
        Qt::QueuedConnection);
}

void SocketManager::requestAbort()
{
    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket]() {
            socket->abort();
        },
        Qt::BlockingQueuedConnection);
}

void SocketManager::requestGI()
{
    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket]() {
            socket->solicitGI();
        },
        Qt::QueuedConnection);
}

void SocketManager::requestSendData(CommandData data)
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

    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket, qoi = obj.ca]() {
            socket->setSecondaryASDUAddress(qoi);
        },
        Qt::QueuedConnection);

    QDateTime current = QDateTime::currentDateTime();

    switch (obj.type) {
    case iec104_class::C_IC_NA_1: // Interrogation
        QMetaObject::invokeMethod(m_socket,
            [=]() { 
                m_socket->solicitInterrogation(data.leCmdValue.toInt());
            },
            Qt::QueuedConnection);
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

    QMetaObject::invokeMethod(m_socket,
        [socket = m_socket, obj]() mutable {
            auto command = obj;
            socket->sendCommand(&command);
        },
        Qt::QueuedConnection);
}