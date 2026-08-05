#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <functional>
#include <memory>
#include <qapplication.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qcoreevent.h>
#include <qelapsedtimer.h>
#include <qevent.h>
#include <qfont.h>
#include <qglobal.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qpalette.h>
#include <qscrollbar.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qstylefactory.h>
#include <qtablewidget.h>
#include <qtimer.h>
#include <qvector.h>
#include <qwidget.h>
#include <string>
#include "AppSettings.h"
#include "iec104/iec104_class.h"
#include "LogController.h"
#include "QIec104.h"
#include "SettingsDialog.h"
#include "PointController.h"

void MainWindow::queueProtocolCall(const std::function<void(QIec104*)>& fn)
{
    QMetaObject::invokeMethod(
        i104,
        [worker = i104, fn]() { fn(worker); },
        Qt::QueuedConnection);
}

void MainWindow::shutdownProtocolThread()
{
    if (ProtocolShutdown)
        return;

    ProtocolShutdown = true;

    if (!i104)
        return;

    QMetaObject::invokeMethod(i104,
        [worker = i104]() {
            worker->terminate();
            worker->deleteLater();
        },
        Qt::BlockingQueuedConnection);

    protocolThread.quit();
    protocolThread.wait();

    i104 = nullptr;
}

//-------------------------------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    ui(std::make_unique<Ui::MainWindow>()),
    i104(new QIec104())
{
    ui->setupUi(this);

    auto& settings = AppSettings::instance();
    settings.load();

	m_logController = std::make_unique<LogController>(ui->lwLog, i104);
	connect(m_logController.get(), &LogController::logUpdated, this, [this]() {
		if (ui->cbAutoScroll->isChecked()) {
            QScrollBar* vbar = ui->lwLog->verticalScrollBar();
            vbar->setValue(vbar->maximum());
		}
		});
    connect(m_logController.get(), &LogController::resizeTableRequested, this, [this]() {
        if (pointTableResizePending && pendingDataIndications.isEmpty()) {
            ui->twPontos->resizeRowsToContents();
            ui->twPontos->resizeColumnsToContents();
            pointTableResizePending = false;
        }
        });
    m_logController->setLogState(ui->cbLog->isChecked());

    m_pointController = std::make_unique<PointController>(ui->twPontos, i104);

    auto separator = []() {
        auto f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
        };
    statusBar()->addWidget(&m_lbStatus, 1);          // занимает все свободное место
    statusBar()->addPermanentWidget(&m_lbMode);
    statusBar()->addPermanentWidget(separator());
    statusBar()->addPermanentWidget(new QLabel(settings.VERSION));

    on_cb888Mode_stateChanged(ui->cb888Mode->isChecked());

    i104->moveToThread(&protocolThread);
    protocolThread.start();

    tmUiDataPump = new QTimer(this);
    connect(tmUiDataPump, &QTimer::timeout, this, &MainWindow::slot_processPendingUiData);

    connect(i104, &QIec104::signal_dataIndication, this, &MainWindow::slot_dataIndication);
    connect(i104, &QIec104::signal_tcp_connect, this, &MainWindow::slot_tcpconnect);
    connect(i104, &QIec104::signal_tcp_disconnect, this, &MainWindow::slot_tcpdisconnect);
    connect(i104, &QIec104::signal_commandActRespIndication, this, [this](const iec_obj& obj) {
        CommandData data{};
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
        m_pointController->commandActRespIndication1(data, obj);
        }
    );
        
    reloadProtocolSettings();

    ui->pbGI->setEnabled(false);
    ui->pbCopyVals->setEnabled(false);
	ui->sendCommand->setEnabled(false);

    ui->twPontos->sortByColumn(0, Qt::AscendingOrder);
    ui->twPontos->setColumnCount(8);
    QStringList colunas;
    colunas << "Адрес"
        << "АСДУ"
        << "Значение"
        << "Тип"
        << "Причина"
        << "Флаги"
        << "Счетчик"
        << "Временная метка";
    ui->twPontos->setHorizontalHeaderLabels(colunas);

    QFont font = QFont("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(8);
    font.setFixedPitch(true);
    ui->lwLog->setFont(font);

    // Set default theme to Light
    ui->cbTheme->setCurrentIndex(0);
    on_cbTheme_currentIndexChanged(0);
    connect(ui->cbTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_cbTheme_currentIndexChanged);
}

MainWindow::~MainWindow()
{
    AppSettings::instance().store();
    shutdownProtocolThread();
}

void MainWindow::on_pbGI_clicked()
{
    queueProtocolCall([](QIec104* worker) { 
        worker->solicitGI(); 
        });
}

void MainWindow::on_pbConnect_clicked()
{
    if (ProtocolKeepAliveActive) {
        ProtocolKeepAliveActive = false;
        queueProtocolCall([](QIec104* worker) {
            worker->tmKeepAlive->stop();
            worker->tcps->close();
            worker->slot_tcpdisconnect();
            });
    }
    else {
        reloadProtocolSettings();
		ui->pbSettingsDialog->setEnabled(false);
        ui->pbConnect->setText("Прервать");
        m_lbStatus.setText("<font color='green'>Попытка подключения...</font>");
        m_pointController->clear();
        ProtocolKeepAliveActive = true;
        queueProtocolCall([](QIec104* worker) { 
            worker->tmKeepAlive->start(1000); 
            });
    }
}

void MainWindow::reloadProtocolSettings()
{
    auto& s = AppSettings::instance();

    queueProtocolCall([&s](QIec104* worker) {
        worker->setSecondaryIP(const_cast<char*>(s.IpAddress.toStdString().c_str()));
        worker->setSecondaryIP_backup(const_cast<char*>(s.IpAddressReserve.toStdString().c_str()));
        worker->setPortTCP(s.TcpPort);

        worker->setSecondaryAddress(s.CA);
        worker->setPrimaryAddress(s.OA);

        worker->setGIPeriod(s.GIperiod);

        worker->ForcePrimary = s.ForcePrimary;
        worker->SendCommands = s.SendCommands;
        });
}

void MainWindow::on_pbSettingsDialog_clicked()
{
	auto dlg = new SettingsDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg, &SettingsDialog::settingsChanged, this, &MainWindow::reloadProtocolSettings);
    dlg->show();
}

void MainWindow::on_pbSendCommandsButton_clicked()
{
    CommandData data{};
	data.cb888Mode = ui->cb888Mode->isChecked();
    data.leCmdAddressLow = ui->leCmdAddressLow->text();
    data.leCmdAddressMid = ui->leCmdAddressMid->text();
	data.leCmdAddressHigh = ui->leCmdAddressHigh->text();
	data.leCmdAddress = ui->leCmdAddress->text();
	data.leASDUAddr = ui->leASDUAddr->text().toInt();
	data.cbCmdAsdu = ui->cbCmdAsdu->currentText();
	data.cbCmdDuration = ui->cbCmdDuration->currentText();
	data.leCmdValue = ui->leCmdValue->text().toInt();
	data.cbSBO = ui->cbSBO->isChecked();
	m_pointController->sendCommand(data);
}

void MainWindow::slot_dataIndication(const QVector<iec_obj>& objects)
{
    if (objects.isEmpty()) {
        return;
    }

    pendingDataPointCount += objects.size();
    pendingDataIndications.enqueue(objects);

    if (!tmUiDataPump->isActive())
        tmUiDataPump->start(0);
}

void MainWindow::slot_processPendingUiData()
{
    if (pendingDataIndications.isEmpty()) {
        tmUiDataPump->stop();
        return;
    }

    const qsizetype maxPointsPerTick =
        pendingDataPointCount > 20000 ? 12000 : 4000;
    const qint64 maxMillisPerTick =
        pendingDataPointCount > 20000 ? 30 : 20;

    qsizetype pointsProcessed = 0;
    QElapsedTimer elapsed;
    elapsed.start();

    ui->twPontos->setUpdatesEnabled(false);
    while (!pendingDataIndications.isEmpty()) {
        const QVector<iec_obj> objects = pendingDataIndications.dequeue();
        pendingDataPointCount -= objects.size();
        if (m_pointController->processDataIndicationBatch(objects)) {
            pointTableSortPending = true;
            pointTableResizePending = true;
        }

        pointsProcessed += objects.size();

        if (pointsProcessed >= maxPointsPerTick || elapsed.elapsed() >= maxMillisPerTick) {
            break;
        }
    }        
    ui->twPontos->setUpdatesEnabled(true);
    ui->twPontos->viewport()->update();

    if (pointTableSortPending && pendingDataIndications.isEmpty()) {
        ui->twPontos->sortItems(0);
        pointTableSortPending = false;
    }

    if (pendingDataIndications.isEmpty())
        tmUiDataPump->stop();
    else
        tmUiDataPump->start(0);
}

void MainWindow::slot_tcpconnect(const QString& peerAddress)
{
    m_lbStatus.setText("<font color='green'>" + QString("Соединение установлено c ") + peerAddress + "</font>" );
    ui->pbGI->setEnabled(true);
    ui->pbCopyVals->setEnabled(true);
    ui->sendCommand->setEnabled(true);
    ui->pbConnect->setText("Отключить");
}

void MainWindow::slot_tcpdisconnect()
{
    m_lbStatus.setText("<font color='red'> Соединение отключено!</font>");
    ui->pbGI->setEnabled(false);
    ui->pbCopyVals->setEnabled(false);
    ui->sendCommand->setEnabled(false);

    if (ProtocolKeepAliveActive) {
        ui->pbConnect->setText("Прервать");
		ui->pbSettingsDialog->setEnabled(false);
    }
    else {
        ui->pbConnect->setText("Подключить");
        ui->pbSettingsDialog->setEnabled(true);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    shutdownProtocolThread();
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
    m_pointController->copyToClipboard();
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

	m_pointController->set888Mode(arg1);
}
