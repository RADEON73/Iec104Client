#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <memory>
#include <qabstractsocket.h>
#include <qapplication.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qcoreevent.h>
#include <qevent.h>
#include <qframe.h>
#include <qglobal.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qpalette.h>
#include <qscrollbar.h>
#include <qsortfilterproxymodel.h>
#include <qstring.h>
#include <qstylefactory.h>
#include <qwidget.h>
#include "AppSettings.h"
#include "LogController.h"
#include "model/TableModel.h"
#include "ProtocolController.h"
#include "SettingsDialog.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>()),
    m_model(new TableModel(this))
{
    ui->setupUi(this);

    auto& settings = AppSettings::instance();
    settings.load();

    m_model->setColumnCount(8);

    auto proxy = new QSortFilterProxyModel(this);
    proxy->setSortRole(QtSortRole);
    proxy->setSourceModel(m_model);
    proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy->setDynamicSortFilter(true);
    ui->twPontos->setModel(proxy);
    ui->twPontos->setSortingEnabled(true);
    proxy->sort(TableModel::PointColumn::Address, Qt::AscendingOrder);

    m_protocolController = std::make_unique<ProtocolController>();
    connect(m_protocolController.get(), &ProtocolController::signal_stateChanged, this, &MainWindow::slot_stateChanged);
    connect(m_protocolController.get(), &ProtocolController::signal_dataIndication, m_model, &TableModel::slot_dataIndication);
    connect(m_protocolController.get(), &ProtocolController::signal_commandActRespIndication, m_model, &TableModel::slot_commandActRespIndication);

	m_logController = std::make_unique<LogController>(ui->lwLog, m_protocolController->i104());
	connect(m_logController.get(), &LogController::signal_logUpdated, this, [this]() {
		if (ui->cbAutoScroll->isChecked()) {
            QScrollBar* vbar = ui->lwLog->verticalScrollBar();
            vbar->setValue(vbar->maximum());
		}
		});
    m_logController->setLogState(ui->cbLog->isChecked());

    auto separator = []() {
        auto f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
        };
    statusBar()->addWidget(&m_lbStatus, 1);
    statusBar()->addPermanentWidget(separator());
    statusBar()->addPermanentWidget(new QLabel(settings.VERSION));

    on_cb888Mode_stateChanged(ui->cb888Mode->isChecked());

    ui->cbTheme->setCurrentIndex(0);
    on_cbTheme_currentIndexChanged(0);
    connect(ui->cbTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), 
        this, &MainWindow::on_cbTheme_currentIndexChanged);

    QList<int> sizes;
    sizes << ui->splitter->height() << 0; // Первый занимает всё, второй скрыт
    ui->splitter->setSizes(sizes);
}

MainWindow::~MainWindow() = default;

void MainWindow::on_pbGI_clicked()
{
    m_protocolController->request_GI();
}

void MainWindow::on_pbConnect_clicked()
{
    m_protocolController->request_Connect();
}

void MainWindow::on_pbSettingsDialog_clicked()
{
	auto dlg = new SettingsDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::settingsChanged, m_protocolController.get(), &ProtocolController::slot_reloadProtocolSettings);
    dlg->show();
}

void MainWindow::on_pbSendCommandsButton_clicked()
{
    ProtocolController::CommandData data{};
	data.cb888Mode = ui->cb888Mode->isChecked();
    data.leCmdAddressLow = ui->leCmdAddressLow->text();
    data.leCmdAddressMid = ui->leCmdAddressMid->text();
	data.leCmdAddressHigh = ui->leCmdAddressHigh->text();
	data.leCmdAddress = ui->leCmdAddress->text();
	data.leASDUAddr = ui->leASDUAddr->text();
	data.cbCmdAsdu = ui->cbCmdAsdu->currentText();
	data.cbCmdDuration = ui->cbCmdDuration->currentText();
	data.leCmdValue = ui->leCmdValue->text();
	data.cbSBO = ui->cbSBO->isChecked();
	m_protocolController->request_SendData(data);
}

void MainWindow::slot_stateChanged(QAbstractSocket::SocketState state)
{
	switch (state) {
    case QAbstractSocket::ConnectedState:
        m_model->clear();
        m_lbStatus.setText("<font color='green'> Соединение установлено </font>");
        ui->pbGI->setEnabled(true);
        ui->sendCommand->setEnabled(true);
        ui->pbConnect->setText("Отключить");
        ui->pbSettingsDialog->setEnabled(false);
        break;
	case QAbstractSocket::UnconnectedState:
        m_lbStatus.setText("<font color='red'> Сокет не подключен </font>");
        ui->pbGI->setEnabled(false);
        ui->sendCommand->setEnabled(false);
        ui->pbConnect->setText("Подключить");
        ui->pbSettingsDialog->setEnabled(true);
		break;
	case QAbstractSocket::HostLookupState:
        m_lbStatus.setText("<font color='blue'> Выполняется DNS-разрешение имени </font>");
        break;
	case QAbstractSocket::ConnectingState:
        m_lbStatus.setText("<font color='blue'> Идет установка TCP-соединения... </font>");
        ui->pbGI->setEnabled(false);
        ui->sendCommand->setEnabled(false);
        ui->pbConnect->setText("Прервать");
        ui->pbSettingsDialog->setEnabled(false);
        break;
    case QAbstractSocket::BoundState:
        m_lbStatus.setText("<font color='blue'> Состояние BoundState </font>");
		break;
    case QAbstractSocket::ListeningState:
        m_lbStatus.setText("<font color='blue'> Соединение ListeningState </font>");
		break;
	case QAbstractSocket::ClosingState:
        m_lbStatus.setText("<font color='blue'> Выполняется отключение... </font>");
        ui->pbGI->setEnabled(false);
        ui->sendCommand->setEnabled(false);
        ui->pbConnect->setText("Закрытие сокета");
        ui->pbSettingsDialog->setEnabled(false);
		break;
	default:
        m_lbStatus.setText("<font color='blue'> Соединение UNKNOWN </font>");
		break;
	}
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    event->accept();
}

void MainWindow::on_cbLog_clicked()
{
    m_logController->setLogState(ui->cbLog->isChecked());
}

void MainWindow::on_pbCopyClipb_clicked()
{
    m_logController->copyToClipboard();
}

void MainWindow::on_pbCopyVals_clicked()
{
    m_model->copyToClipboard();
}

void MainWindow::on_cbTheme_currentIndexChanged(int index)
{
    if (index == 1) { // Dark
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(45, 45, 45));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
    }
    else { // Light
        qApp->setStyle(QStyleFactory::create("Fusion"));
        QPalette sp = QApplication::style()->standardPalette();
        sp.setColor(QPalette::Text, Qt::black);
        qApp->setPalette(sp);
    }
}

void MainWindow::on_cb888Mode_stateChanged(int arg1)
{
    ui->leCmdAddress->setVisible(!arg1);
    ui->leCmdAddressLow->setVisible(arg1);
    ui->leCmdAddressMid->setVisible(arg1);
    ui->leCmdAddressHigh->setVisible(arg1);

	m_model->set888Mode(arg1);
}
