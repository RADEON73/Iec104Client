#pragma once
#include <functional>
#include <qabstractsocket.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qthread.h>
#include "iec104/iec104_class.h"
#include <qvector.h>

class QIec104;

class ProtocolController  : public QObject
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

	explicit ProtocolController(QObject *parent = nullptr);
	~ProtocolController();

    QIec104* i104();

	void request_GI();
	void request_Connect();
    void request_SendData(CommandData data);

    void shutdown();

signals:
    void signal_stateChanged(QAbstractSocket::SocketState);
    void signal_commandActRespIndication(const iec_obj& obj);
    void signal_dataIndication(const QVector<iec_obj>& objects);

public slots:
    void slot_reloadProtocolSettings();

private:
    void updateSettings();
    void queueProtocolCommand(const iec_obj& obj);
    void queueProtocolCall(const std::function<void(QIec104*)>& fn);

private:
    QIec104* m_i104 = nullptr;
    QThread m_protocolThread;
};

