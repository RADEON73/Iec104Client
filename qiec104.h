#pragma once

#include <iec104/iec104_class.h>
#include <qabstractsocket.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qtcpsocket.h>
#include <qtimer.h>
#include <qvector.h>
#include "iec104/iec104_types.h"

class QIec104 : public QObject, public iec104_class
{
    Q_OBJECT

public:
    explicit QIec104(QObject* parent = 0);
    ~QIec104();

    void connectTcp();
    void disconnectTcp();
    void terminate();

    QAbstractSocket::SocketState connectionState() const;

signals:
    void signal_dataIndication(const QVector<iec_obj>& objects);
    void signal_commandActRespIndication(const iec_obj& obj);

    void stateChanged(QAbstractSocket::SocketState);

private slots:
    void slot_tcpconnect(); // tcp connect for iec104
    void slot_tcpdisconnect(); // tcp disconnect for iec104
    void slot_tcpReadyRead(); // ready to read data on iec104 tcp socket
    void slot_tcperror(QAbstractSocket::SocketError socketError); // show errors of tcp
    void slot_keep_alive();
    void slot_reconnect();

private: // redefine for iec104_class
    void waitBytes(int bytes, int msTout) final;
    void connectTCP() final;
    void disconnectTCP() final;
    int readTCP(char* buf, int szmax) final;
    void sendTCP(char* data, int sz) final;
    int bytesAvailableTCP() final;

    void dataIndication(iec_obj* obj, unsigned numpoints) final;
    void interrogationActConfIndication() final {}
    void interrogationActTermIndication() final {}
    void commandActRespIndication(iec_obj* obj) final;
    void userprocAPDU(iec_apdu* papdu, int sz) final {}

private:
    QTimer* m_tmKeepAlive = nullptr; // 1 second timer Iec-104

    bool m_shutdownRequested = false; //shutdown request
    bool m_reconnectEnabled = true; //reconnect enabled

    QTcpSocket* m_tcps = nullptr; // iec104 socket(tcp)
    QTimer* m_tcps_reconnect; //reconnect timer for iec104 socket


    unsigned int mConnectAttemptCounter = 0;
};
