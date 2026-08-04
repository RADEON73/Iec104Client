#include <memory>
#include <qcoreapplication.h>
#include <qsettings.h>
#include "application.h"

Application& Application::instance()
{
    static Application instance;
    return instance;
}

Application::Application()
{
    m_regSettings = std::make_unique<QSettings>(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat);
}

void Application::load()
{
    m_regSettings->beginGroup("IEC104");
    m_settings.OA = m_regSettings->value("OA", 1).toInt();
    m_settings.CA = m_regSettings->value("CA", 1).toInt();
    m_settings.ForcePrimary = m_regSettings->value("FORCE_PRIMARY", 0).toInt();
    m_settings.SendCommands = m_regSettings->value("ALLOW_COMMANDS", 0).toInt();
    m_settings.IpAddress = m_regSettings->value("IP_ADDRESS", "127.0.0.1").toString();
    m_settings.IpAddressReserve = m_regSettings->value("IP_ADDRESS_RESERVE", "0.0.0.0").toString();
    m_settings.TcpPort = m_regSettings->value("TCP_PORT", 2404).toUInt();
	m_settings.GIperiod = m_regSettings->value("GI_PERIOD", 330).toInt();
    m_regSettings->endGroup();
}

void Application::save()
{
    m_regSettings->beginGroup("IEC104");
    m_regSettings->setValue("OA", m_settings.OA);
    m_regSettings->setValue("CA", m_settings.CA);
    m_regSettings->setValue("FORCE_PRIMARY", m_settings.ForcePrimary);
    m_regSettings->setValue("ALLOW_COMMANDS", m_settings.SendCommands);
    m_regSettings->setValue("IP_ADDRESS", m_settings.IpAddress);
    m_regSettings->setValue("IP_ADDRESS_RESERVE", m_settings.IpAddressReserve);
    m_regSettings->setValue("TCP_PORT", m_settings.TcpPort);
    m_regSettings->setValue("GI_PERIOD", m_settings.GIperiod);
    m_regSettings->endGroup();
}
