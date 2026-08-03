#pragma once
#include <list>
#include <qmutex.h>
#include <string>

class TLogMsg
{
public:
    TLogMsg();
    void pushMsg(const char * msg, unsigned int level=0); // level: 0=less important
    std::string pullMsg();
    void activateLog();
    void deactivateLog();
    void doLogTime();
    void dontLogTime();
    void setMaxMsg(unsigned int maxmsg);
    bool haveMsg();
    void setLevel(unsigned int nivel); // set exibition level
    bool isLogging();
    int count();

private:
    std::list <std::string> mLstLog;
    std::list <time_t> mLstTime;
    mutable QMutex mMutex;
    unsigned int mMaxMsg;
    bool mDoLog;
    bool mRegTime;
    time_t mLastTime; // time of the previously pulled message
    unsigned int mLevel; // exibition level 0=all, 1 an on, exibit more information progressively
};
