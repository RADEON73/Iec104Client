#include "iec104_log.h"

#include <ctime>
#include <mutex>
#include <queue>
#include <string>

iec104_log::iec104_log() = default;
iec104_log::~iec104_log() = default;

void iec104_log::activateLog()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_logOn = true;
}

void iec104_log::deactivateLog()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    std::queue<LogMessage>().swap(m_logQueue);
    m_logOn = false;
}

void iec104_log::setLogLevel(size_t logLevel)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_logLevel = logLevel;
}

bool iec104_log::isEmpty()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return !m_logQueue.empty();
}

bool iec104_log::isNotEmpty()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return !m_logQueue.empty();
}

bool iec104_log::isLogging()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_logOn;
}

void iec104_log::pushMsg(const std::string& msg, size_t logLevel)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    if (m_logOn && logLevel >= m_logLevel) {
        LogMessage item{ msg, std::time(nullptr) };
        m_logQueue.push(item);
    }
}

size_t iec104_log::size()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_logQueue.size();
}

std::string iec104_log::pullMsg()
{
    std::lock_guard<std::mutex> locker(m_mutex);

    if (m_logQueue.empty() || !m_logOn)
        return "";

    LogMessage msg = m_logQueue.front();
    m_logQueue.pop();

    std::string buffer;

    time_t hora = msg.time;
    if (hora != m_lastTime) {
        struct tm* timeinfo = localtime(&hora);
        strftime(buffer.data(), 200, "%H:%M:%S ", timeinfo);
        msg.text = buffer + msg.text;
    }
    //else
        //msg.text = "         " + msg.text;

    m_lastTime = hora;

    return msg.text;
}

