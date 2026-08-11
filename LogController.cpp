#include "LogController.h"

#include <AppSettings.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qfont.h>
#include <qobject.h>
#include <qplaintextedit.h>
#include <qstring.h>
#include <qtimer.h>
#include "QIec104.h"

LogController::LogController(QPlainTextEdit* log, QIec104* i104, QObject* parent) : QObject(parent),
	m_log(log), 
    m_i104(i104)
{
	connect(&tmLogMsg, &QTimer::timeout, this, &LogController::slot_timerLogmsg);
	tmLogMsg.start(350);

    QFont font = QFont("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(8);
    font.setFixedPitch(true);
    m_log->setFont(font);
}

LogController::~LogController() = default;

void LogController::copyToClipboard()
{
    QApplication::clipboard()->setText(m_log->toPlainText());
}

void LogController::setLogState(bool state)
{
    if (state) {
        m_i104->mLog.activateLog();
        auto d = QDate::currentDate();
        QString str = d.toString("yyyy.MM.dd") + QString(" Версия программы - ") + AppSettings::instance().VERSION;
        m_i104->mLog.pushMsg(str.toStdString().c_str());
    }
    else
        m_i104->mLog.deactivateLog();
}

void LogController::clear()
{
	m_log->clear();
}

void LogController::slot_timerLogmsg()
{
    static const int maxLogMsgsPerTick = 250;

    if (m_i104->mLog.haveMsg()) {
        // append the whole batch inside a single edit block: one layout/paint pass;
        // the document's maximumBlockCount (set in the .ui) discards the oldest
        // lines automatically once the log is full
        int logMsgsProcessed = 0;
        QTextCursor cur(m_log->document());
        cur.movePosition(QTextCursor::End);
        cur.beginEditBlock();
        while (m_i104->mLog.haveMsg() && logMsgsProcessed < maxLogMsgsPerTick) {
            const QString msg = QString::fromStdString(m_i104->mLog.pullMsg());

            QTextCharFormat fmt;
            if (msg.contains(QLatin1String("I104M"))) {
                fmt.setForeground(Qt::darkGray);
            }
            else if (msg.contains(QLatin1String("COMMAND"))) {
                fmt.setForeground(Qt::darkGray);
                fmt.setBackground(Qt::lightGray);
            }
            else if (msg.contains(QLatin1Char('['))) {
                fmt.setForeground(Qt::red);
            }
            else {
                fmt.setForeground(Qt::darkGray);
            }

            if (!cur.atStart())
                cur.insertBlock();
            cur.insertText(msg, fmt);
            logMsgsProcessed++;
        }
        cur.endEditBlock();

        emit signal_logUpdated();
    }
}