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
    m_tcps(new QTcpSocket(this))
{
    mLog.activateLog();
    mLog.doLogTime();

    connect(m_tcps, &QTcpSocket::stateChanged, this, [this](QAbstractSocket::SocketState state) { 
        emit stateChanged(state);
        });

    connect(m_tmKeepAlive, &QTimer::timeout, this, &QIec104::slot_keep_alive);
    connect(m_tcps, &QTcpSocket::readyRead, this, &QIec104::slot_tcpReadyRead);
    connect(m_tcps, &QTcpSocket::connected, this, &QIec104::slot_tcpconnect);
    connect(m_tcps, &QTcpSocket::disconnected, this, &QIec104::slot_tcpdisconnect);
    connect(m_tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_tcperror, Qt::DirectConnection);
    connect(m_tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_socketError);
}

QIec104::~QIec104() = default;

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
    if (m_tcps->state() != QAbstractSocket::UnconnectedState) {
        mLog.pushMsg("TCP соединение не отключено.");
        return;
    }

    char buf[100];

    m_tcps->abort();

    if (!mEnding) {
        mConnectAttemptCounter++;
        // alternate main and backup UTR IP address, if configured
        if (mConnectAttemptCounter % 2 || strcmp(getSecondaryIP_backup(), "0.0.0.0") == 0) {
            m_tcps->connectToHost(getSecondaryIP(), quint16(getPortTCP()), QIODevice::ReadWrite);
            sprintf(buf, "Попытка подключения к IP: %s", getSecondaryIP());
            mLog.pushMsg(const_cast<char*>(buf));
        }
        else {
            m_tcps->connectToHost(getSecondaryIP_backup(), quint16(getPortTCP()), QIODevice::ReadWrite);
            sprintf(buf, "Попытка подключения к IP: %s", getSecondaryIP_backup());
            mLog.pushMsg(const_cast<char*>(buf));
        }
    }

    m_tmKeepAlive->start(1000);

    m_tcps->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    onConnectTCP();

}

void QIec104::disconnectTCP() 
{ 
    if (m_tcps->state() != QAbstractSocket::ConnectedState) {
		mLog.pushMsg("TCP соединение уже разорвано.");
        return;
    }

    m_tmKeepAlive->stop();
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
        char buf[100];
        sprintf(buf, "Ошибка сокета: %d", socketError);
        mLog.pushMsg(const_cast<char*>(buf));
    }
}

int QIec104::readTCP(char* buf, int szmax)
{
    int ret = int(m_tcps->read(buf, szmax));

    if (!mEnding && ret > 0)
        return ret;
    else
        return 0;
}

// send tcp data, user provided
void QIec104::sendTCP(char* data, int sz)
{
    if (m_tcps->state() == QAbstractSocket::ConnectedState)
        if (!mEnding) {
            m_tcps->write(data, sz);
            m_tcps->flush();
            if (mLog.isLogging())
                LogFrame(data, sz, true);
        }
}

void QIec104::slot_tcpconnect()
{
    mLog.pushMsg("TCP соединение установлено.");
}

void QIec104::slot_tcpdisconnect()
{
    onDisconnectTCP();
}

void QIec104::slot_keep_alive()
{
    if (!mEnding) {
        mKeepAliveCounter++;
        if (!(mKeepAliveCounter % 5))
            if (m_tcps->state() == QAbstractSocket::UnconnectedState) {
                mLog.pushMsg("!!!!!ПОПЫТКА ПОДКЛЮЧЕНИЯ...!");
                connectTCP();
            }

        onTimerSecond();
    }
}

void QIec104::commandActRespIndication(iec_obj* obj)
{
    emit signal_commandActRespIndication(*obj);
}

void QIec104::terminate()
{
    mEnding = true;
    m_tcps->abort();
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

void QIec104::slot_socketError(QAbstractSocket::SocketError)
{
    mLog.pushMsg(m_tcps->errorString().toStdString().c_str());
}