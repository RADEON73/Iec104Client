#pragma once
#include <memory>
#include <qsettings.h>
#include <qstring.h>

class Application
{
	struct Settings
	{
		QString IpAddress;
		QString IpAddressReserve;
		unsigned int TcpPort;

		int CA; //Адрес удаленной станции (СA) - Common Address of ASDU
		int OA; //Адрес отправителя (OA) - Originator Address

		int GIperiod; //GI period in seconds, 0 = no GI

		int ForcePrimary; // 1 = force primary (cant't stay secondary) , 0 = can be secondary
		int SendCommands; // 1 = allow sending commands, 0 = don't send commands
	};

public:
	static Application& instance();

	Settings& settings() { return m_settings; }
	const Settings& settings() const { return m_settings; }

	void load();
	void save();

private:
	Application();
	~Application() = default;

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;
	Application(Application&&) = delete;
	Application& operator=(Application&&) = delete;

private:
	std::unique_ptr<QSettings> m_regSettings;
	Settings m_settings;
};

