#include "Iec104TcpSocket.h"

#include <qabstractsocket.h>
#include <qdebug.h>
#include <qglobal.h>
#include <qiodevice.h>
#include <qobject.h>
#include <qstring.h>
#include <qtcpsocket.h>
#include <qtimer.h>
#include "iec104/iec104_class.h"
#include "iec104/iec104_types.h"
#include <string>
#include <cstdint>

Iec104Socket::Iec104Socket(QObject* parent) : QTcpSocket(parent),
    m_tmKeepAlive(new QTimer(this))
{
    mLog.activateLog();

    setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(m_tmKeepAlive, &QTimer::timeout, this, &Iec104Socket::slot_keepAlive);

    connect(this, &QTcpSocket::connected, this, &Iec104Socket::onSocketConnected);
    connect(this, &QTcpSocket::disconnected, this, &Iec104Socket::onSocketDisconnected);
    connect(this, &QTcpSocket::readyRead, this, &Iec104Socket::onSocketReadyRead);
    connect(this, &QTcpSocket::errorOccurred, this, &Iec104Socket::onSocketError);
}

void Iec104Socket::abort()
{
    mLog.pushMsg("Прерывание подключения", 2);
    QTcpSocket::abort();
}

void Iec104Socket::connectTCP(const std::string& ip, uint16_t port)
{
    auto qIp = QString::fromStdString(ip);
    QString msg = QString("Подключение к серверу (%1:%2)")
        .arg(qIp)
        .arg(port);
    mLog.pushMsg(msg.toStdString(), 2);
    QTcpSocket::connectToHost(qIp, port, QIODevice::ReadWrite);
}

void Iec104Socket::disconnectTCP()
{
    mLog.pushMsg("Отключение от сервера", 2);
    QTcpSocket::disconnectFromHost();
}

int Iec104Socket::readTCP(char* buf, int szmax)
{
    auto ret = read(buf, szmax);
    if (ret > 0)
        return ret;
    else
        return 0;
}

void Iec104Socket::sendTCP(char* data, int sz)
{
    if (state() == QAbstractSocket::ConnectedState) {
        write(data, sz);
        flush();
        LogFrame(data, sz, true);
    }
}

void Iec104Socket::waitBytes(int bytes, int msTout)
{
    while (bytesAvailable() < bytes && msTout > 0) {
        waitForReadyRead(8);
        msTout -= 8;
    }
}

int Iec104Socket::bytesAvailableTCP()
{
    return bytesAvailable();
}

void Iec104Socket::dataIndication(iec_obj* obj, unsigned numpoints)
{
    qDebug() << "Получен пакет данных";
    QVector<iec_obj> objects;
    objects.reserve(static_cast<qsizetype>(numpoints));
    for (unsigned i = 0; i < numpoints; ++i) {
        objects.append(obj[i]);
    }
    emit signal_dataIndication(objects);
}

void Iec104Socket::interrogationActConfIndication()
{
    qDebug() << "Получено подтверждение активации общего опроса";
    emit signal_interrogationActConfIndication();
}

void Iec104Socket::interrogationActTermIndication()
{
    qDebug() << "Получено завершение общего опроса";
    emit signal_interrogationActTermIndication();
}

void Iec104Socket::commandActRespIndication(iec_obj* obj)
{
    qDebug() << "Получен ответ на активацию команды";
    emit signal_commandActRespIndication(*obj);
}

void Iec104Socket::userprocAPDU(iec_apdu* papdu, int sz)
{
    qDebug() << "Перехват APDU";
    emit signal_userprocAPDU(papdu, sz);
}

void Iec104Socket::slot_keepAlive()
{
    onTimerSecond();
}

void Iec104Socket::onSocketConnected()
{
    m_tmKeepAlive->start(1000);
    onConnectTCP();
}

void Iec104Socket::onSocketDisconnected()
{
    m_tmKeepAlive->stop();
    onDisconnectTCP();
}

void Iec104Socket::onSocketReadyRead()
{
    if (bytesAvailable() < 6)
        waitForReadyRead(8);
    packetReadyTCP();
}

void Iec104Socket::onSocketError(QAbstractSocket::SocketError socketError)
{
    QString err = QString("Ошибка сокета: %1 [%2]")
        .arg(socketError)
        .arg(errorString());
    qDebug() << err;
    mLog.pushMsg(err.toStdString(), 2);
}