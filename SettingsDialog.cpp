#include "SettingsDialog.h"
#include "ui_SettingsDialog.h"

#include <memory>
#include <qdialog.h>
#include <qwidget.h>
#include "AppSettings.h"
#include <qregularexpression.h>
#include <qvalidator.h>
#include <qdialogbuttonbox.h>
#include <qpushbutton.h>

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent),
	ui(std::make_unique<Ui::SettingsDialog>())
{
	ui->setupUi(this);

	ui->buttonBox->button(QDialogButtonBox::Save)->setText("Сохранить");
	ui->buttonBox->button(QDialogButtonBox::Cancel)->setText("Отмена");
	ui->buttonBox->button(QDialogButtonBox::Reset)->setText("Сброс");

	connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
	connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
	connect(ui->buttonBox->button(QDialogButtonBox::Reset), &QPushButton::clicked,
		this, &SettingsDialog::reload);

	QRegularExpression rx("\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}("
		"?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b");
	QValidator* valip = new QRegularExpressionValidator(rx, this);
	ui->IpAddress->setValidator(valip);
	ui->IpAddressReserve->setValidator(valip);

	load();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::accept()
{
	store();

	emit signal_settingsChanged();

	QDialog::accept();
}

void SettingsDialog::reject()
{
	QDialog::reject();
}

void SettingsDialog::load()
{
	auto& s = AppSettings::instance();

	s.load();

	ui->IpAddress->setText(s.IpAddress);
	ui->IpAddressReserve->setText(s.IpAddressReserve);
	ui->TcpPort->setValue(s.TcpPort);

	ui->CA->setValue(s.CA);
	ui->OA->setValue(s.OA);

	ui->GIperiod->setValue(s.GIperiod);
}

void SettingsDialog::store()
{
	auto& s = AppSettings::instance();

	s.IpAddress = ui->IpAddress->text();
	s.IpAddressReserve = ui->IpAddressReserve->text();
	s.TcpPort = ui->TcpPort->value();

	s.CA = ui->CA->value();
	s.OA = ui->OA->value();

	s.GIperiod = ui->GIperiod->value();

	s.store();
}

void SettingsDialog::reload()
{
	load();
}
