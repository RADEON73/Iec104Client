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

QIec104::QIec104(QObject* parent) : QObject(parent)
{
    mLog.activateLog();
    mLog.doLogTime();

    tcps = new QTcpSocket(this);
    tmKeepAlive = new QTimer(this);

    connect(tmKeepAlive, &QTimer::timeout, this, &QIec104::slot_keep_alive);
    connect(tcps, &QTcpSocket::readyRead, this, &QIec104::slot_tcpreadytoread);
    connect(tcps, &QTcpSocket::connected, this, &QIec104::slot_tcpconnect);
    connect(tcps, &QTcpSocket::disconnected, this, &QIec104::slot_tcpdisconnect);
    connect(tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_tcperror, Qt::DirectConnection);
    connect(tcps, &QTcpSocket::errorOccurred, this, &QIec104::slot_socketError);
}

QIec104::~QIec104() = default;

void QIec104::waitBytes(int bytes, int msTout)
{
    while (tcps->bytesAvailable() < bytes && msTout > 0) {
        tcps->waitForReadyRead(8);
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
    char buf[100];

    tcps->abort();

    if (!mEnding && mAllowConnect) {
        // alternate main and backup UTR IP address, if configured
        if ((++mConnectAttemptCounter) % 2 || strcmp(getSecondaryIP_backup(), "0.0.0.0") == 0) {
            tcps->connectToHost(getSecondaryIP(), quint16(getPortTCP()), QIODevice::ReadWrite);
            sprintf(buf, "Try to connect IP: %s", getSecondaryIP());
            mLog.pushMsg(const_cast<char*>(buf));
        }
        else {
            tcps->connectToHost(getSecondaryIP_backup(), quint16(getPortTCP()), QIODevice::ReadWrite);
            sprintf(buf, "Try to connect IP: %s", getSecondaryIP_backup());
            mLog.pushMsg(const_cast<char*>(buf));
        }
    }
}

void QIec104::disconnectTCP() { tcps->abort(); }

void QIec104::slot_tcperror(QAbstractSocket::SocketError socketError)
{
    if (socketError != QAbstractSocket::SocketTimeoutError) {
        char buf[100];
        sprintf(buf, "SocketError: %d", socketError);
        mLog.pushMsg(const_cast<char*>(buf));
    }
}

int QIec104::readTCP(char* buf, int szmax)
{
    int ret = int(tcps->read(buf, szmax));

    if (!mEnding && ret > 0)
        return ret;
    else
        return 0;
}

// send tcp data, user provided
void QIec104::sendTCP(char* data, int sz)
{
    if (tcps->state() == QAbstractSocket::ConnectedState)
        if (!mEnding) {
            tcps->write(data, sz);
            tcps->flush();
            if (mLog.isLogging())
                LogFrame(data, sz, true);
        }
}

void QIec104::slot_tcpconnect()
{
    tcps->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    mLog.pushMsg("Plain TCP Connection Established.");
    onConnectTCP();
    emit signal_tcp_connect(tcps->peerAddress().toString());
}

void QIec104::slot_tcpdisconnect()
{
    onDisconnectTCP();
    emit signal_tcp_disconnect();
}

void QIec104::slot_keep_alive()
{
    if (!mEnding) {
        mKeepAliveCounter++;

        if (!(mKeepAliveCounter % 5))
            if (tcps->state() != QAbstractSocket::ConnectedState && mAllowConnect) {
                mLog.pushMsg("!!!!!TRY TO CONNECT!");
                connectTCP();
            }

        onTimerSecond();
    }
}

void QIec104::interrogationActConfIndication()
{
    emit signal_interrogationActConfIndication();
}

void QIec104::interrogationActTermIndication()
{
    emit signal_interrogationActTermIndication();
}

void QIec104::commandActRespIndication(iec_obj* obj)
{
    emit signal_commandActRespIndication(*obj);
}

void QIec104::terminate()
{
    mEnding = true;
    tcps->abort();
}

void QIec104::slot_tcpreadytoread()
{
    if (tcps->bytesAvailable() < 6)
        tcps->waitForReadyRead(8);

    packetReadyTCP();
}

void QIec104::disable_connect()
{
    mAllowConnect = false;
    if (tcps->state() == QAbstractSocket::ConnectedState)
        disconnectTCP();
}

void QIec104::enable_connect() 
{
    mAllowConnect = true;
}

int QIec104::bytesAvailableTCP() 
{ 
    return int(tcps->bytesAvailable()); 
}

void QIec104::slot_socketError(QAbstractSocket::SocketError)
{
    mLog.pushMsg(tcps->errorString().toStdString().c_str());
}