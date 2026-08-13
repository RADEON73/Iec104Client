#include "LogController.h"

#include <AppSettings.h>
#include <qapplication.h>
#include <qchar.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qfont.h>
#include <qglobal.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qplaintextedit.h>
#include <qscrollbar.h>
#include <qstring.h>
#include <qtextcursor.h>
#include <qtextformat.h>
#include <qtimer.h>
#include "iec104/iec104_log.h"

LogController::LogController(iec104_log* logQueue, QPlainTextEdit* logUI, QObject* parent) : QObject(parent),
    m_logQueue(logQueue),
    m_logUI(logUI)
{
    QFont font = QFont("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(8);
    font.setFixedPitch(true);
    m_logUI->setFont(font);

	connect(&m_tmLogMsg, &QTimer::timeout, this, &LogController::slot_timerLogmsg);
    m_tmLogMsg.start(350);

    setLogState(true);
}

LogController::~LogController() = default;

void LogController::copyToClipboard()
{
    QApplication::clipboard()->setText(m_logUI->toPlainText());
}

void LogController::setLogState(bool state)
{
    if (state) {
        m_logQueue->activateLog();
        auto d = QDate::currentDate();
        QString str = d.toString("yyyy.MM.dd") + QString(" Версия программы - ") + AppSettings::instance().VERSION;
        m_logQueue->pushMsg(str.toStdString());
    }
    else
        m_logQueue->deactivateLog();
}

void LogController::setLogLevel(bool state)
{
    m_logQueue->setLogLevel(state ? 0 : 2);
}

void LogController::setAutoScrollState(bool state)
{
    m_autoScroll = state;
}

void LogController::clear()
{
    m_logUI->clear();
}

void LogController::slot_timerLogmsg()
{
    static const int maxLogMsgsPerTick = 250;

    if (m_logQueue->isNotEmpty()) {
        int logMsgsProcessed = 0;
        QTextCursor cur(m_logUI->document());
        cur.movePosition(QTextCursor::End);
        cur.beginEditBlock();
        while (m_logQueue->isNotEmpty() && logMsgsProcessed < maxLogMsgsPerTick) {
            const QString msg = QString::fromStdString(m_logQueue->pullMsg());

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

        if (m_autoScroll) {
           QScrollBar* vbar = m_logUI->verticalScrollBar();
           vbar->setValue(vbar->maximum());
        }

        emit signal_logUpdated();
    }
}