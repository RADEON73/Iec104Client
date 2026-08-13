//Класс отвечает за организацию лога
#pragma once
#include <qobject.h>
#include <qobjectdefs.h>
#include <qplaintextedit.h>
#include <qtimer.h>
#include "iec104/iec104_log.h"

class LogController : public QObject
{
	Q_OBJECT

public:
	explicit LogController(iec104_log* logQueue, QPlainTextEdit* logUI, QObject* parent = nullptr);
	~LogController();

	void clear();

	void setLogState(bool state);
	void setLogLevel(bool state);
	void copyToClipboard();
	void setAutoScrollState(bool state);

signals:
	void signal_logUpdated();

private slots:
	void slot_timerLogmsg(); // timer for log messages

private:
	iec104_log* m_logQueue;
	QPlainTextEdit* m_logUI;

	bool m_autoScroll = true;
	size_t m_loglevel = 0;
	QTimer m_tmLogMsg; // timer to show log messages
};

