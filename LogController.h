//Класс отвечает за организацию лога
#pragma once
#include <qobject.h>
#include <qobjectdefs.h>
#include <qplaintextedit.h>
#include <qtimer.h>
#include "QIec104.h"

class LogController : public QObject
{
	Q_OBJECT

public:
	explicit LogController(QPlainTextEdit* log, QIec104* i104, QObject* parent = nullptr);
	~LogController();

	void copyToClipboard();
	void setLogState(bool state);

	void clear();

signals:
	void logUpdated();
	void resizeTableRequested();

private slots:
	void slot_timer_logmsg(); // timer for log messages

private:
	QPlainTextEdit* m_log;
	QIec104* m_i104;

	QTimer tmLogMsg; // timer to show log messages
	bool autoScrollEnabled = true;
	int logTickCount = 0;
};

