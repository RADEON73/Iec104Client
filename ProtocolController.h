#pragma once
#include <functional>
#include <qabstractsocket.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qthread.h>
#include <qvector.h>
#include "iec104/iec104_class.h"
#include "iec104/iec104_log.h"

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

    void requestConnect();
    void requestGI();
    void requestSendData(CommandData data);

    iec104_log* logQueue();

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

