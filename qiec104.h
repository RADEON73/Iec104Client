#pragma once

#include <iec104/iec104_class.h>
#include <qabstractsocket.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qstring.h>
#include <qtcpsocket.h>
#include <qtimer.h>
#include <qvector.h>

class QIec104 : public QObject, public iec104_class
{
    Q_OBJECT

public:
    explicit QIec104(QObject* parent = 0);
    ~QIec104();

    void terminate();

    void connectTCP();
    void disconnectTCP();
    QAbstractSocket::SocketState connectionState() const;

public:
    QTimer* m_tmKeepAlive = nullptr; // 1 second timer
    QTcpSocket* m_tcps = nullptr;    // socket for iec104 (tcp)

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
    void slot_socketError(QAbstractSocket::SocketError);

private:
    // redefine for iec104_class
    void waitBytes(int bytes, int msTout);
    int bytesAvailableTCP();

    int readTCP(char* buf, int szmax);
    void sendTCP(char* data, int sz);
    void commandActRespIndication(iec_obj* obj);
    void dataIndication(iec_obj* obj, unsigned numpoints);

private:
    bool mEnding = false;

    unsigned int mConnectAttemptCounter = 0;
    unsigned int mKeepAliveCounter = 0;
};
