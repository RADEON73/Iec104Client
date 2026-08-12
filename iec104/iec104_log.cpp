#include "iec104_log.h"

#include <ctime>
#include <qmutex.h>
#include <string>

iec104_log::iec104_log() = default;
iec104_log::~iec104_log() = default;

void iec104_log::setMaxMsg(unsigned int maxmsg)
{
    QMutexLocker locker(&mMutex);
    mMaxMsg = maxmsg;
}

void iec104_log::setLevel(unsigned int level)
{
    QMutexLocker locker(&mMutex);
    mLevel = level;
}

void iec104_log::activateLog()
{
    QMutexLocker locker(&mMutex);
    mDoLog = true;
}

void iec104_log::deactivateLog()
{
    QMutexLocker locker(&mMutex);
    mLstLog.clear(); // clean lists
    mLstTime.clear();
    mDoLog = false;
}

void iec104_log::doLogTime()
{
    QMutexLocker locker(&mMutex);
    mLstLog.clear(); // clean lists, sync
    mLstTime.clear();
    mRegTime = true;
}

void iec104_log::dontLogTime()
{
    QMutexLocker locker(&mMutex);
    mRegTime = false;
}

bool iec104_log::haveMsg()
{
    QMutexLocker locker(&mMutex);
    return !mLstLog.empty();
}

bool iec104_log::isLogging()
{
    QMutexLocker locker(&mMutex);
    return mDoLog;
}

// coloca a mensagem na fila
void iec104_log::pushMsg( const char * msg, unsigned int level )
{
    QMutexLocker locker(&mMutex);
    if ( mDoLog && ( mLstLog.size() < mMaxMsg ) && ( mLevel <= level ) ) {
        mLstLog.push_back( msg );
        if ( mRegTime ) { // coloca hora na fila, se for o caso
            mLstTime.push_back( time( NULL ) );
        }
    }
}

int iec104_log::count()
{
    QMutexLocker locker(&mMutex);
    return int(mLstLog.size());
}

// Tira mensagem da fila
std::string iec104_log::pullMsg()
{
    QMutexLocker locker(&mMutex);
    if ( mLstLog.empty() || !mDoLog )
        return "";

    std::string s = mLstLog.front();   // pega a primeira da fila
    mLstLog.pop_front();          // retira-a da fila

    // se tem registro de hora, pega a hora e formata para exibir antes da mensagem
    if (mRegTime){
        char buffer [201];
        time_t hora = mLstTime.front();
        mLstTime.pop_front();
        if (hora != mLastTime)
          {
          struct tm * timeinfo;
          timeinfo = localtime ( &hora );
          // strftime ( buffer,200,"%d/%m %H:%M:%S ",timeinfo );
          strftime ( buffer,200,"%H:%M:%S ",timeinfo );
          s = buffer + s;
          }
        else
          s = "         " + s;
        mLastTime = hora;
    }

    return s;
}

