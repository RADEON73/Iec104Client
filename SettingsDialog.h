//Класс диалог параметров подключения
#pragma once
#include <memory>
#include <qdialog.h>
#include <qglobal.h>
#include <qwidget.h>

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsDialog; };
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
	Q_OBJECT

public:
	SettingsDialog(QWidget *parent = nullptr);
	~SettingsDialog();

signals:
	void signal_settingsChanged();

public slots:
	void accept() override;
	void reject() override;

private slots:
	void reload();

private:
	void load();
	void store();

private:
	std::unique_ptr<Ui::SettingsDialog> ui;
};
