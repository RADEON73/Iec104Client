#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <memory>
#include <qabstractsocket.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qframe.h>
#include <qglobal.h>
#include <qgroupbox.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qpalette.h>
#include <qpushbutton.h>
#include <qsortfilterproxymodel.h>
#include <qstylefactory.h>
#include <qwidget.h>
#include "AppSettings.h"
#include "LogController.h"
#include "model/TableModel.h"
#include "model/TableProxyModel.h"
#include "ProtocolController.h"
#include "QIec104.h"
#include "SettingsDialog.h"
#include <qlabel.h>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>())
{
    ui->setupUi(this);

    auto& settings = AppSettings::instance();
    settings.load();

    //Создаем модель
    m_model = std::make_unique<TableModel>();
    m_model->setColumnCount(8);
    auto proxyModel = new TableProxyModel(ui->tv_Points);
    proxyModel->setSourceModel(m_model.get());
    ui->tv_Points->setModel(proxyModel);
    ui->tv_Points->horizontalHeader()->setSortIndicator(TableModel::PointColumn::Address, Qt::AscendingOrder);

    //Создаем протокол
    m_protocolController = std::make_unique<ProtocolController>();
    connect(m_protocolController.get(), &ProtocolController::signal_stateChanged, 
        this, &MainWindow::slot_stateChanged);
    connect(m_protocolController.get(), &ProtocolController::signal_dataIndication,
        m_model.get(), &TableModel::slot_dataIndication);
    connect(m_protocolController.get(), &ProtocolController::signal_commandActRespIndication, 
        m_model.get(), &TableModel::slot_commandActRespIndication);

    //Создаем лог
	m_logController = std::make_unique<LogController>(m_protocolController->logQueue(), ui->pte_LogData);

    auto separator = []() {
        auto f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
        };
    statusBar()->addWidget(&m_lbStatus, 1);
    statusBar()->addPermanentWidget(separator());
    statusBar()->addPermanentWidget(new QLabel(settings.VERSION));

    on_cb_888Mode_stateChanged(ui->cb_888Mode->isChecked());

    ui->cb_Theme->setCurrentIndex(0);
    on_cb_Theme_currentIndexChanged(0);
    connect(ui->cb_Theme, QOverload<int>::of(&QComboBox::currentIndexChanged), 
        this, &MainWindow::on_cb_Theme_currentIndexChanged);

    ui->splitter->setSizes( {1, 0} );
}

MainWindow::~MainWindow() = default;

void MainWindow::slot_stateChanged(QAbstractSocket::SocketState state)
{
	switch (state) {
    case QAbstractSocket::ConnectedState:
        m_model->clear();
        m_lbStatus.setText("<font color='green'> Соединение установлено </font>");
        ui->pb_PointsGI->setEnabled(true);
        ui->pb_SendCommand->setEnabled(true);
        ui->pb_Connect->setText("Отключить");
        ui->pb_SettingsDialog->setEnabled(false);
        break;
	case QAbstractSocket::UnconnectedState:
        m_lbStatus.setText("<font color='red'> Сокет не подключен </font>");
        ui->pb_PointsGI->setEnabled(false);
        ui->pb_SendCommand->setEnabled(false);
        ui->pb_Connect->setText("Подключить");
        ui->pb_SettingsDialog->setEnabled(true);
		break;
	case QAbstractSocket::HostLookupState:
        m_lbStatus.setText("<font color='blue'> Выполняется DNS-разрешение имени </font>");
        break;
	case QAbstractSocket::ConnectingState:
        m_lbStatus.setText("<font color='blue'> Идет установка TCP-соединения... </font>");
        ui->pb_PointsGI->setEnabled(false);
        ui->pb_SendCommand->setEnabled(false);
        ui->pb_Connect->setText("Прервать");
        ui->pb_SettingsDialog->setEnabled(false);
        break;
    case QAbstractSocket::BoundState:
        m_lbStatus.setText("<font color='blue'> Состояние BoundState </font>");
		break;
    case QAbstractSocket::ListeningState:
        m_lbStatus.setText("<font color='blue'> Соединение ListeningState </font>");
		break;
	case QAbstractSocket::ClosingState:
        m_lbStatus.setText("<font color='blue'> Выполняется отключение... </font>");
        ui->pb_PointsGI->setEnabled(false);
        ui->pb_SendCommand->setEnabled(false);
        ui->pb_Connect->setText("Закрытие сокета");
        ui->pb_SettingsDialog->setEnabled(false);
		break;
	default:
        m_lbStatus.setText("<font color='blue'> Соединение UNKNOWN </font>");
		break;
	}
}

void MainWindow::on_cb_888Mode_stateChanged(int state)
{
    ui->leCmdAddress->setVisible(!state);
    ui->leCmdAddressLow->setVisible(state);
    ui->leCmdAddressMid->setVisible(state);
    ui->leCmdAddressHigh->setVisible(state);

	m_model->set888Mode(state);
}

void MainWindow::on_pb_SettingsDialog_clicked()
{
    auto dlg = new SettingsDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::signal_settingsChanged, m_protocolController.get(), &ProtocolController::slot_reloadProtocolSettings);
    dlg->show();
}

void MainWindow::on_pb_Connect_clicked()
{
    if (m_protocolController)
        m_protocolController->requestConnect();
}

void MainWindow::on_cb_Theme_currentIndexChanged(int index)
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

void MainWindow::on_pb_PointsGI_clicked()
{
    if (m_protocolController)
        m_protocolController->requestGI();
}

void MainWindow::on_pb_PointsCopy_clicked()
{
    if (m_model)
        m_model->copyToClipboard();
}

void MainWindow::on_pb_SendCommand_clicked()
{
    ProtocolController::CommandData data{};
    data.cb888Mode = ui->cb_888Mode->isChecked();
    data.leCmdAddressLow = ui->leCmdAddressLow->text();
    data.leCmdAddressMid = ui->leCmdAddressMid->text();
    data.leCmdAddressHigh = ui->leCmdAddressHigh->text();
    data.leCmdAddress = ui->leCmdAddress->text();
    data.leASDUAddr = ui->leASDUAddr->text();
    data.cbCmdAsdu = ui->cbCmdAsdu->currentText();
    data.cbCmdDuration = ui->cbCmdDuration->currentText();
    data.leCmdValue = ui->leCmdValue->text();
    data.cbSBO = ui->cbSBO->isChecked();
    m_protocolController->requestSendData(data);
}

void MainWindow::on_gb_LogOn_toggled(bool on)
{
    if (m_logController)
        m_logController->setLogState(on);
}

void MainWindow::on_cb_LogAutoscroll_toggled(bool on)
{
    if (m_logController)
        m_logController->setAutoScrollState(on);
}

void MainWindow::on_pb_LogCopy_clicked()
{
    if (m_logController)
        m_logController->copyToClipboard();
}
