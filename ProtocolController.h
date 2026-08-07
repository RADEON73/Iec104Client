#pragma once
#include <functional>
#include <qabstractsocket.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qthread.h>
#include "iec104/iec104_class.h"

class QIec104;

class ProtocolController  : public QObject
{
	Q_OBJECT

public:
	explicit ProtocolController(QObject *parent = nullptr);
	~ProtocolController();

    QIec104* i104();

	void request_GI();
	void request_Connect();

signals:
    void stateChanged(QAbstractSocket::SocketState);
    void signal_commandActRespIndication(const iec_obj& obj);

public slots:
    void reloadProtocolSettings();

private:
    void queueProtocolCall(const std::function<void(QIec104*)>& fn);
    void shutdownProtocolThread();

private:
    QIec104* m_i104 = nullptr;
    QThread protocolThread;


    bool ProtocolShutdown = false;
};

