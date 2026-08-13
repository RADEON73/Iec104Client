#pragma once
#include <mutex>
#include <queue>
#include <string>

class iec104_log
{
public:
    struct LogMessage
    {
        std::string text;
        time_t time;
    };

    iec104_log();
    ~iec104_log();

    void pushMsg(const std::string& msg, size_t logLevel = 0);
    std::string pullMsg();

    void activateLog();
    void deactivateLog();

    void setLogLevel(size_t logLevel);

    bool isEmpty();
    bool isNotEmpty();

    bool isLogging();

    size_t size();

private:
    std::mutex m_mutex;
    std::queue<LogMessage> m_logQueue;

    bool m_logOn = true;
    size_t m_logLevel = 0;
    time_t m_lastTime = 0;
};
