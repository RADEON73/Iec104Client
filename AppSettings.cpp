#include <memory>
#include <qcoreapplication.h>
#include <qsettings.h>
#include "AppSettings.h"

AppSettings& AppSettings::instance()
{
    static AppSettings instance;
    return instance;
}

AppSettings::AppSettings()
{
    m_regSettings = std::make_unique<QSettings>(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat);
}

void AppSettings::load()
{
    m_regSettings->beginGroup("IEC104");

    IpAddress = m_regSettings->value("IP_ADDRESS", "127.0.0.1").toString();
    IpAddressReserve = m_regSettings->value("IP_ADDRESS_RESERVE", "0.0.0.0").toString();
    TcpPort = m_regSettings->value("TCP_PORT", 2404).toUInt();

    CA = m_regSettings->value("CA", 1).toInt();
    OA = m_regSettings->value("OA", 1).toInt();

    GIperiod = m_regSettings->value("GI_PERIOD", 330).toInt();

    m_regSettings->endGroup();
}

void AppSettings::store()
{
    m_regSettings->beginGroup("IEC104");

    m_regSettings->setValue("IP_ADDRESS", IpAddress);
    m_regSettings->setValue("IP_ADDRESS_RESERVE", IpAddressReserve);
    m_regSettings->setValue("TCP_PORT", TcpPort);

    m_regSettings->setValue("CA", CA);
    m_regSettings->setValue("OA", OA);

    m_regSettings->setValue("GI_PERIOD", GIperiod);

    m_regSettings->endGroup();
}