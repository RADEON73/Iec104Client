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
    m_primaryCheckTimer(new QTimer(this)),
    m_tcps(new QTcpSocket(this)),
    m_primaryProbe(new QTcpSocket(this))
{
    mLog.activateLog();
    mLog.doLogTime();

    qRegisterMetaType<QAbstractSocket::SocketState>();
    qRegisterMetaType<iec_obj>("iec_obj");
    qRegisterMetaType<QVector<iec_obj>>("QVector<iec_obj>");

    connect(m_tmKeepAlive, &QTimer::timeout, this, &QIec104::slot_keep_alive);

    m_tcps->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(m_tcps, &QTcpSocket::stateChanged, this, &QIec104::signal_stateChanged);
    connect(m_tcps, &QTcpSocket::readyRead, this, &QIec104::slot_tcpReadyRead);
    connect(m_tcps, &QTcpSocket::connected, this, &QIec104::slot_tcpconnect);
    connect(m_tcps, &QTcpSocket::disconnected, this, &QIec104::slot_tcpdisconnect);
    connect(m_tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_tcperror);
    m_primaryCheckTimer->setInterval(5000);
    m_primaryCheckTimer->setSingleShot(false);
    connect(m_primaryCheckTimer, &QTimer::timeout, this, &QIec104::slot_checkPrimary);

    connect(m_primaryProbe, &QTcpSocket::connected, this, &QIec104::slot_primaryProbeConnected);
    connect(m_primaryProbe, &QTcpSocket::errorOccurred, this, &QIec104::slot_primaryProbeError);
    m_tcps_reconnect->setInterval(2000);
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
    mLog.pushMsg(
        QString("DATA INDICATION: server=%1, points=%2")
        .arg(m_currentServer == Server::Primary ? "PRIMARY" : "BACKUP")
        .arg(numpoints)
        .toUtf8()
        .constData());

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
        const char* ipAddr = nullptr;
        if (m_currentServer == Server::Primary)
            ipAddr = getSecondaryIP();
        else
            ipAddr = getSecondaryIP_backup();
        m_tcps->connectToHost(ipAddr, quint16(getPortTCP()), QIODevice::ReadWrite);
        auto msg = QString("Попытка подключения к IP: %1").arg(ipAddr);
        mLog.pushMsg(msg.toUtf8().constData());
    }
}

void QIec104::disconnectTCP() 
{ 
    auto state = m_tcps->state();

    m_reconnectEnabled = false;
    m_tcps_reconnect->stop();
    m_tcps->close();
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
        if (m_currentServer == Server::Primary && hasBackupServer()) {
            m_currentServer = Server::Backup;
            mLog.pushMsg("Основной сервер недоступен. Переключение на резервный сервер.");
            m_primaryCheckTimer->start();
        }
        m_tcps_reconnect->start();
        mLog.pushMsg("!!!!!Переподключение...");
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

    onConnectTCP();
}

void QIec104::slot_tcpdisconnect()
{
    mLog.pushMsg("TCP соединение разорвано.");

    m_tmKeepAlive->stop();

    if (m_shutdownRequested)
        return;

    // Сначала обязательно закрываем старую IEC-104 сессию.
    onDisconnectTCP();

    if (!m_reconnectEnabled)
        return;

    // Backup -> Primary
    if (m_switchToPrimary) {
        m_switchToPrimary = false;

        mLog.pushMsg(
            "Старое соединение закрыто. "
            "Подключение к основному серверу.");

        connectTCP();
        return;
    }

    if (m_currentServer == Server::Primary && hasBackupServer()) {
        m_currentServer = Server::Backup;
        mLog.pushMsg("Основной сервер недоступен. Переключение на резервный сервер.");
        m_primaryCheckTimer->start();
    }

    m_tcps_reconnect->start();

    mLog.pushMsg("!!!!!Переподключение...");
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
    m_primaryCheckTimer->stop();
    m_tmKeepAlive->stop();
    m_primaryProbe->abort();
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

const char* QIec104::currentServerIP()
{
    if (m_currentServer == Server::Primary)
        return getSecondaryIP();

    return getSecondaryIP_backup();
}

bool QIec104::hasBackupServer()
{
    return strcmp(getSecondaryIP_backup(), "0.0.0.0") != 0;
}

void QIec104::slot_checkPrimary()
{
    if (m_shutdownRequested)
        return;

    if (m_currentServer != Server::Backup)
        return;

    if (!hasBackupServer())
        return;

    if (m_primaryProbe->state() != QAbstractSocket::UnconnectedState)
        return;

    const char* primaryIP = getSecondaryIP();

    if (primaryIP == nullptr || primaryIP[0] == '\0')
        return;

    mLog.pushMsg(QString("Проверка доступности основного сервера: %1")
        .arg(primaryIP)
        .toUtf8()
        .constData());

    m_primaryProbe->abort();

    m_primaryProbe->connectToHost(primaryIP, quint16(getPortTCP()), QIODevice::ReadWrite);
}


void QIec104::slot_primaryProbeConnected()
{
    mLog.pushMsg("Основной сервер снова доступен.");

    m_primaryProbe->abort();

    if (m_shutdownRequested)
        return;

    if (m_currentServer != Server::Backup)
        return;

    m_switchToPrimary = true;

    m_currentServer = Server::Primary;

    m_primaryCheckTimer->stop();
    m_tcps_reconnect->stop();

    mLog.pushMsg("Переключение рабочего соединения на основной сервер.");

    m_tcps->abort();
}

void QIec104::slot_primaryProbeError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    m_primaryProbe->abort();
    mLog.pushMsg("Основной сервер не доступен...");
}
