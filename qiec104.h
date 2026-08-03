#pragma once

#include <iec104_class.h>
#include <qabstractsocket.h>
#include <qlist.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qsslerror.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtimer.h>
#include <qvector.h>

class QIec104 : public QObject, public iec104_class
{
    Q_OBJECT

public:
    explicit QIec104(QObject* parent = 0);
    ~QIec104();
    int SendCommands;    // 1 = allow sending commands, 0 = don't send commands
    int ForcePrimary;    // 1 = force primary (cant't stay secondary) , 0 = can be
    // secondary
    QTimer* tmKeepAlive; // 1 second timer
    QSslSocket* tcps;    // socket for iec104 (tcp)
    void terminate();
    void disable_connect();
    void enable_connect();
    // TLS Configuration Setters
    void setTlsEnabled(bool enabled);
    void setCaCertPath(const QString& path);
    void setLocalCertPath(const QString& path);
    void setPrivateKeyPath(const QString& path);
    void setPeerVerifyMode(QSslSocket::PeerVerifyMode mode);

signals:
    void signal_dataIndication(const QVector<iec_obj>& objects);
    void signal_interrogationActConfIndication();
    void signal_interrogationActTermIndication();
    void signal_tcp_connect(const QString& peerAddress);
    void signal_tcp_disconnect();
    void signal_commandActRespIndication(const iec_obj& obj);

public slots:
    void slot_tcpdisconnect(); // tcp disconnect for iec104
    void slot_modeChanged(QSslSocket::SslMode mode);

private slots:
    void slot_tcpconnect();     // tcp connect for iec104
    void slot_tcpreadytoread(); // ready to read data on iec104 tcp socket
    void slot_tcperror(QAbstractSocket::SocketError socketError); // show errors of tcp
    void slot_keep_alive();
    void slot_sslErrors(const QList<QSslError>& errors); // Slot for SSL errors
    void slot_socketEncrypted();
    void slot_socketError(QAbstractSocket::SocketError);

private:
    // QThread tcpThread;

    // redefine for iec104_class
    void waitBytes(int bytes, int msTout);
    int bytesAvailableTCP();
    void connectTCP();
    void disconnectTCP();
    int readTCP(char* buf, int szmax);
    void sendTCP(char* data, int sz);
    void interrogationActConfIndication();
    void interrogationActTermIndication();
    void commandActRespIndication(iec_obj* obj);
    void dataIndication(iec_obj* obj, unsigned numpoints);
    bool mEnding;
    bool mAllowConnect;
    int mConnectAttemptCounter;
    unsigned int mKeepAliveCounter;

    // TLS Configuration Members
    bool mUseTls = false;
    QString mCaCertPath;
    QString mLocalCertPath;
    QString mPrivateKeyPath;
    QSslSocket::PeerVerifyMode mVerifyMode = QSslSocket::VerifyNone; // Default to no verification for ease of testing initially
};
