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
    enum class Server
    {
        Primary,
        Backup
    };

    explicit QIec104(QObject* parent = 0);
    ~QIec104();

    void connectTcp();
    void disconnectTcp();
    void terminate();

    QAbstractSocket::SocketState connectionState() const;

signals:
    void signal_dataIndication(const QVector<iec_obj>& objects);
    void signal_commandActRespIndication(const iec_obj& obj);
    void signal_stateChanged(QAbstractSocket::SocketState);

private slots:
    void slot_tcpconnect(); // tcp connect for iec104
    void slot_tcpdisconnect(); // tcp disconnect for iec104
    void slot_tcpReadyRead(); // ready to read data on iec104 tcp socket
    void slot_tcperror(QAbstractSocket::SocketError socketError); // show errors of tcp
    void slot_keep_alive();
    void slot_reconnect();

    void slot_checkPrimary();
    void slot_primaryProbeConnected();
    void slot_primaryProbeError(QAbstractSocket::SocketError socketError);

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

    const char* currentServerIP();
    bool hasBackupServer();

private:
    QTimer* m_tmKeepAlive = nullptr; // 1 second timer Iec-104

    Server m_currentServer = Server::Primary; //Тип текущего сервера

    bool m_shutdownRequested = false; //shutdown request
    bool m_reconnectEnabled = true; //reconnect enabled

    QTcpSocket* m_tcps = nullptr; // iec104 socket(tcp)
    QTcpSocket* m_primaryProbe = nullptr; //primary checkLife socket

    QTimer* m_tcps_reconnect = nullptr; //reconnect timer for iec104 socket
    QTimer* m_primaryCheckTimer = nullptr; //checker for primary server life
    bool m_switchToPrimary = false; //Флаг блокировки ложного переключения
};
