#pragma once
#include <Iec104TcpSocket.h>
#include <qabstractsocket.h>
#include <qglobal.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qstring.h>
#include <qthread.h>
#include <qtimer.h>
#include "iec104/iec104_log.h"
#include "iec104/iec104_types.h"

class SocketManager : public QObject
{
	Q_OBJECT

public:
    struct CommandData
    {
        bool cb888Mode;
        QString leCmdAddressLow;
        QString leCmdAddressMid;
        QString leCmdAddressHigh;
        QString leCmdAddress;
        QString leASDUAddr;
        QString cbCmdAsdu;
        QString cbCmdDuration;
        QString leCmdValue;
        bool cbSBO;
    };

    enum class ServerType
    {
        Primary,
        Backup
    };

    struct ServerConfig
    {
        QString host;
        quint16 port;
    };

	SocketManager(QObject* parent = nullptr);
	~SocketManager();

    // Управление
    void start();  // Начинаем подключение к первичному серверу
    void stop();   // Полная остановка (явная команда отключиться)
    void reconnect(); // Принудительное переподключение

    iec104_log* logQueue();

    //Защищенные сокет операции (межпоточные)
    void requestUpdate();
    void requestConnect(QString ip, quint16 port);
    void requestDisconnect();
    void requestAbort();
    void requestGI();
    void requestSendData(CommandData data);

    // Статус
    bool isConnected() const;
    ServerType currentServer() const;

signals:
    void connected(ServerType server);      // Подключились к серверу
    void disconnected();                     // Отключились (по любой причине)
    void errorOccurred(const QString& error); // Ошибка с человеческим текстом
    void stopped();                             // Полная остановка по команде
    
    void signal_dataIndication(const QVector<iec_obj>& objects);
    void signal_interrogationActConfIndication();
    void signal_interrogationActTermIndication();
    void signal_commandActRespIndication(const iec_obj& obj);
    void signal_userprocAPDU(iec_apdu* papdu, int sz);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimer();

    void onPrimaryCheckerConnected();
    void onPrimaryCheckerDisconnected();
    void onPrimaryCheckerError(QAbstractSocket::SocketError error);
    void onPrimaryCheckTimeout();

private:
    enum class State
    {
        Idle,           // Неактивен (остановлен)
        Connecting,     // В процессе подключения
        Connected,      // Подключен и работает
        Reconnecting    // Переподключение (пауза)
    };

    // Внутренние методы
    void connectTo(ServerType server);
    void switchToNextServer();
    void scheduleReconnect(int delayMs = 1000);
    void resetConnectionAttempts();
    void shutdownProtocolThread();

    void startPrimaryChecker();
    void stopPrimaryChecker();
    void checkPrimaryServer();
    void switchFromBackupToPrimary();

private:
    // Данные
    Iec104Socket* m_socket = nullptr;
    QTimer* m_reconnectTimer = nullptr;

    QTcpSocket* m_primaryChecker = nullptr;
    QTimer* m_primaryCheckTimer = nullptr;

    bool m_switchingToPrimary = false;

    ServerConfig m_primaryServer;
    ServerConfig m_backupServer;
    bool m_hasBackupServer = false;

    State m_state = State::Idle;
    ServerType m_currentServer = ServerType::Primary;
    int m_connectionAttempts = 0;        // Попытки подряд к одному серверу
    bool m_stopRequested = false;        // Флаг явной остановки

    QThread m_protocolThread;
};
