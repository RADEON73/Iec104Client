#pragma once
#include <cstdint>
#include <iec104/iec104_class.h>
#include <qabstractsocket.h>
#include <qiodevice.h>
#include <qobject.h>
#include <qtcpsocket.h>
#include <qtimer.h>
#include <qvector.h>
#include <string>
#include "iec104/iec104_log.h"
#include "iec104/iec104_types.h"

class Iec104Socket : public QTcpSocket, public iec104_class
{
    Q_OBJECT

private:
    using QTcpSocket::connectToHost;
    using QTcpSocket::disconnectFromHost;

public:
    Iec104Socket(QObject* parent = nullptr);
    ~Iec104Socket() = default;

    void abort();
    iec104_log* logQueue() { return &mLog; }

signals:
    void signal_dataIndication(const QVector<iec_obj>& objects);
    void signal_interrogationActConfIndication();
    void signal_interrogationActTermIndication();
    void signal_commandActRespIndication(const iec_obj& obj);
    void signal_userprocAPDU(iec_apdu* papdu, int sz);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);

public: //Переопределение iec104_class обязательные
    void connectTCP(const std::string& ip, uint16_t port) final;
    void disconnectTCP() final;

private: //Переопределение iec104_class обязательные
    int readTCP(char* buf, int szmax) final;
    void sendTCP(char* data, int sz) final;
    void waitBytes(int bytes, int msTout) final;
    int bytesAvailableTCP() final;

private: //Оповещения от iec104_class не обязательные к переопределению и реализации
    void dataIndication(iec_obj* obj, unsigned numpoints) final;
    void interrogationActConfIndication() final;
    void interrogationActTermIndication() final;
    void commandActRespIndication(iec_obj* obj) final;
    void userprocAPDU(iec_apdu* papdu, int sz) final;

private slots:
    void slot_keepAlive();

private:
    QTimer* m_tmKeepAlive = nullptr; //1 second keepAlive timer Iec-104 Socket
};