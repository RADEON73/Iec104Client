#pragma once
#include <memory>
#include <qsettings.h>
#include <qstring.h>

class AppSettings
{
public:
	static AppSettings& instance();

	void load();
	void store();

private:
	AppSettings();
	~AppSettings() = default;

	AppSettings(const AppSettings&) = delete;
	AppSettings& operator=(const AppSettings&) = delete;
	AppSettings(AppSettings&&) = delete;
	AppSettings& operator=(AppSettings&&) = delete;

private:
	std::unique_ptr<QSettings> m_regSettings;

public:
	QString VERSION = "v1.0.0";

	QString IpAddress;
	QString IpAddressReserve;
	unsigned int TcpPort;

	int CA; //Адрес удаленной станции (СA) - Common Address of ASDU
	int OA; //Адрес отправителя (OA) - Originator Address

	int GIperiod; //GI period in seconds, 0 = no GI

	int ForcePrimary; // 1 = force primary (cant't stay secondary) , 0 = can be secondary
	int SendCommands; // 1 = allow sending commands, 0 = don't send commands
};

