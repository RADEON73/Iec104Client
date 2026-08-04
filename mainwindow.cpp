#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <qapplication.h>
#include <qchar.h>
#include <qclipboard.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qcoreevent.h>
#include <qdatetime.h>
#include <qelapsedtimer.h>
#include <qevent.h>
#include <qfont.h>
#include <qglobal.h>
#include <qmainwindow.h>
#include <qnamespace.h>
#include <qobjectdefs.h>
#include <qpalette.h>
#include <qregularexpression.h>
#include <qscrollbar.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qstylefactory.h>
#include <qtablewidget.h>
#include <qtextcursor.h>
#include <qtextformat.h>
#include <qtimer.h>
#include <qvector.h>
#include <qwidget.h>
#include <string>
#include <utility>
#include "AppSettings.h"
#include "iec104/iec104_class.h"
#include "iec104/iec104_types.h"
#include "QIec104.h"
#include "SettingsDialog.h"

constexpr auto VERSION = "v1.0.0";

static unsigned int parseIoa(const QString& str)
{
    if (str.contains('-') || str.contains('.')) {
        QStringList parts = str.split(QRegularExpression("[-.]"));
        if (parts.size() >= 3) {
            unsigned int low = parts[0].toUInt();
            unsigned int mid = parts[1].toUInt();
            unsigned int high = parts[2].toUInt();
            return low + (mid << 8) + (high << 16);
        }
    }
    return str.toUInt();
}

void MainWindow::queueProtocolCall(const std::function<void(QIec104*)>& fn)
{
    QMetaObject::invokeMethod(
        i104,
        [worker = i104, fn]() { fn(worker); },
        Qt::QueuedConnection);
}

void MainWindow::queueProtocolCommand(const iec_obj& obj)
{
    QMetaObject::invokeMethod(
        i104,
        [worker = i104, command = obj]() mutable { worker->sendCommand(&command); },
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

    i104->mLog.deactivateLog();

    AppSettings::instance().load();

    auto separator = []() {
        auto f = new QFrame;
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
        };
    statusBar()->addWidget(&m_lbStatus, 1);          // занимает все свободное место
    statusBar()->addPermanentWidget(&m_lbMode);
    statusBar()->addPermanentWidget(separator());
    statusBar()->addPermanentWidget(new QLabel(VERSION));

    on_cb888Mode_stateChanged(ui->cb888Mode->isChecked());

    if (ui->cbLog->isChecked()) {
        QDate dt = QDate::currentDate();
        QString str = dt.toString() + QString(" - ") + QString(VERSION);
        i104->mLog.activateLog();
        i104->mLog.pushMsg(str.toStdString().c_str());
    }
    else
        i104->mLog.deactivateLog();

    auto& s = AppSettings::instance();

    i104->moveToThread(&protocolThread);
    protocolThread.start();

    tmUiDataPump = new QTimer(this);
    connect(tmUiDataPump, &QTimer::timeout, this, &MainWindow::slot_processPendingUiData);

    connect(i104, &QIec104::signal_dataIndication, this, &MainWindow::slot_dataIndication);
    connect(i104, &QIec104::signal_interrogationActConfIndication, this, &MainWindow::slot_interrogationActConfIndication);
    connect(i104, &QIec104::signal_interrogationActTermIndication, this, &MainWindow::slot_interrogationActTermIndication);
    connect(i104, &QIec104::signal_tcp_connect, this, &MainWindow::slot_tcpconnect);
    connect(i104, &QIec104::signal_tcp_disconnect, this, &MainWindow::slot_tcpdisconnect);
    connect(i104, &QIec104::signal_commandActRespIndication, this, &MainWindow::slot_commandActRespIndication);

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

    tmLogMsg = new QTimer(this);
    connect(tmLogMsg, &QTimer::timeout, this, &MainWindow::slot_timer_logmsg);
    tmLogMsg->start(350);

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
    queueProtocolCall([](QIec104* worker) { worker->solicitGI(); });
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
        auto& s = AppSettings::instance();

        queueProtocolCall([=, &s](QIec104* worker) {
            worker->setSecondaryIP(
                const_cast<char*>(s.IpAddress.toStdString().c_str()));
            worker->setPortTCP(s.TcpPort);
            worker->setSecondaryAddress(s.CA);
            worker->setPrimaryAddress(s.OA);
            });

		ui->pbSettingsDialog->setEnabled(false);
        ui->pbConnect->setText("Прервать");
        m_lbStatus.setText("<font color='green'>Попытка подключения...</font>");

        mapPtItem_ColAddress.clear();
        mapPtItem_ColCommonAddress.clear();
        mapPtItem_ColValue.clear();
        mapPtItem_ColType.clear();
        mapPtItem_ColCause.clear();
        mapPtItem_ColFlags.clear();
        mapPtItem_ColCount.clear();
        mapPtItem_ColTimeTag.clear();
        ui->twPontos->clearContents();
        ui->twPontos->setRowCount(0);
        // ui->lwLog->clear();
        ProtocolKeepAliveActive = true;
        queueProtocolCall([](QIec104* worker) { worker->tmKeepAlive->start(1000); });
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

// Envio de comando
void MainWindow::on_pbSendCommandsButton_clicked()
{
    iec_obj obj = {};
    obj.type = static_cast<unsigned char>(
        ui->cbCmdAsdu->currentText()
        .left(ui->cbCmdAsdu->currentText().indexOf(':'))
        .toUInt());

    if (obj.type == iec104_class::C_RD_NA_1) {
        if (ui->cb888Mode->isChecked()) {
            if (ui->leCmdAddressLow->text().trimmed() == "" && ui->leCmdAddressMid->text().trimmed() == "" && ui->leCmdAddressHigh->text().trimmed() == "") return;
            obj.address = ui->leCmdAddressLow->text().toUInt() + (ui->leCmdAddressMid->text().toUInt() << 8) + (ui->leCmdAddressHigh->text().toUInt() << 16);
        }
        else {
            if (ui->leCmdAddress->text().trimmed() == "") return;
            obj.address = parseIoa(ui->leCmdAddress->text());
        }
        obj.value = 0;
    }
    else
        // reset process and interrogation must have value set (qrp) or (qoi)
        if (obj.type == iec104_class::C_RP_NA_1 ||
            obj.type == iec104_class::C_IC_NA_1 ||
            obj.type == iec104_class::C_CI_NA_1) {
            if (ui->leCmdValue->text().trimmed() == "")
                return;
            obj.address = 0;
            obj.value = ui->leCmdValue->text().toDouble();
        }
        else
            // test command parameters if not sync command or test command (that don't
            // have parameters)
            if (obj.type != iec104_class::C_CS_NA_1 &&
                obj.type != iec104_class::C_TS_TA_1) {
                if (ui->leCmdValue->text().trimmed() == "") return;
                unsigned int parsedAddr = 0;
                if (ui->cb888Mode->isChecked()) {
                    if (ui->leCmdAddressLow->text().trimmed() == "" && ui->leCmdAddressMid->text().trimmed() == "" && ui->leCmdAddressHigh->text().trimmed() == "") return;
                    parsedAddr = ui->leCmdAddressLow->text().toUInt() + (ui->leCmdAddressMid->text().toUInt() << 8) + (ui->leCmdAddressHigh->text().toUInt() << 16);
                }
                else {
                    if (ui->leCmdAddress->text().trimmed() == "") return;
                    parsedAddr = parseIoa(ui->leCmdAddress->text());
                }
                if (parsedAddr == 0) return;

                obj.address = parsedAddr;
                obj.value = ui->leCmdValue->text().toDouble();
            }

    obj.ca = ui->leASDUAddr->text().toUShort();
    queueProtocolCall([ca = obj.ca](QIec104* worker) {
        worker->setSecondaryASDUAddress(ca);
        });
    QDateTime current = QDateTime::currentDateTime();

    switch (obj.type) {
    case iec104_class::C_IC_NA_1: // Interrogation
        queueProtocolCall([group = ui->leCmdValue->text().toInt()](QIec104* worker) {
            worker->solicitInterrogation(static_cast<char>(group));
            });
        return;
    case iec104_class::C_SC_NA_1:
    case iec104_class::C_SC_TA_1:
        obj.scs = static_cast<unsigned char>(ui->leCmdValue->text().toUInt());
        break;
    case iec104_class::C_DC_NA_1:
    case iec104_class::C_DC_TA_1:
        obj.dcs = static_cast<unsigned char>(ui->leCmdValue->text().toUInt());
        break;
    case iec104_class::C_RC_NA_1:
    case iec104_class::C_RC_TA_1:
        obj.rcs = static_cast<unsigned char>(ui->leCmdValue->text().toUInt());
        break;
    case iec104_class::C_SE_NA_1:
    case iec104_class::C_SE_TA_1:
        obj.value = ui->leCmdValue->text().toInt();
        break;
    case iec104_class::C_SE_NB_1:
    case iec104_class::C_SE_TB_1:
        obj.value = ui->leCmdValue->text().toInt();
        break;
    case iec104_class::C_SE_NC_1:
    case iec104_class::C_SE_TC_1:
        obj.value = ui->leCmdValue->text().toDouble();
        break;
    case iec104_class::C_CS_NA_1:
        obj.timetag.year = static_cast<uint8_t>(current.date().year() % 100);
        obj.timetag.month = static_cast<uint8_t>(current.date().month());
        obj.timetag.mday = static_cast<uint8_t>(current.date().day());
        obj.timetag.hour = static_cast<uint8_t>(current.time().hour());
        obj.timetag.min = static_cast<uint8_t>(current.time().minute());
        obj.timetag.msec = static_cast<uint16_t>(
            current.time().second() * 1000 + current.time().msec());
        obj.timetag.iv = 0;
        obj.timetag.su = static_cast<uint8_t>(current.isDaylightTime());
        obj.timetag.wday = static_cast<uint8_t>(current.date().dayOfWeek());;
        obj.timetag.res1 = 0;
        obj.timetag.res2 = 0;
        obj.timetag.res3 = 0;
        obj.timetag.res4 = 0;
        break;
    case iec104_class::C_TS_TA_1:
        obj.timetag.year = static_cast<uint8_t>(current.date().year() % 100);
        obj.timetag.month = static_cast<uint8_t>(current.date().month());
        obj.timetag.mday = static_cast<uint8_t>(current.date().day());
        obj.timetag.hour = static_cast<uint8_t>(current.time().hour());
        obj.timetag.min = static_cast<uint8_t>(current.time().minute());
        obj.timetag.msec = static_cast<uint16_t>(
            current.time().second() * 1000 + current.time().msec());
        obj.timetag.iv = 0;
        obj.timetag.su = static_cast<uint8_t>(current.isDaylightTime());
        obj.timetag.wday = static_cast<uint8_t>(current.date().dayOfWeek());;
        obj.timetag.res1 = 0;
        obj.timetag.res2 = 0;
        obj.timetag.res3 = 0;
        obj.timetag.res4 = 0;
        break;
    case iec104_class::C_RD_NA_1:
        break;
    case iec104_class::C_RP_NA_1:
        break;
    case iec104_class::P_ME_NA_1:
        obj.value = ui->leCmdValue->text().toInt();
        obj.kpa = static_cast<unsigned char>(
            ui->cbCmdDuration->currentText().left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(ui->cbSBO->isChecked());
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NB_1:
        obj.value = ui->leCmdValue->text().toInt();
        obj.kpa = static_cast<unsigned char>(
            ui->cbCmdDuration->currentText().left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(ui->cbSBO->isChecked());
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NC_1:
        obj.value = ui->leCmdValue->text().toDouble();
        obj.kpa = static_cast<unsigned char>(
            ui->cbCmdDuration->currentText().left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(ui->cbSBO->isChecked());
        obj.pop = 0;
        break;
    case iec104_class::P_AC_NA_1:
        obj.value = ui->leCmdValue->text().toInt();
        obj.qpa = ui->leCmdValue->text().toInt();
        break;
    case iec104_class::C_CI_NA_1:
        obj.value = ui->leCmdValue->text().toInt();
        obj.frz = static_cast<unsigned char>(
            ui->cbCmdDuration->currentText().left(1).toUInt());
        break;
    }
    obj.qu = static_cast<unsigned char>(
        ui->cbCmdDuration->currentText().left(1).toUInt());
    obj.se = static_cast<unsigned char>(ui->cbSBO->isChecked());

    queueProtocolCommand(obj);
    LastCommandAddress = obj.address;
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
        processDataIndicationBatch(objects);
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

void MainWindow::processDataIndicationBatch(const QVector<iec_obj>& objects)
{
    char buf[1500];
    char buftt[1500];
    int rw = -1;
    bool inserted = false;
    QTableWidgetItem* pitem;
    static const char* dblmsg[] = { "tra ", "off ", "on ", "ind " };
    const iec_obj* obj = objects.constData();
    const unsigned numpoints = static_cast<unsigned>(objects.size());

    if (numpoints == 0) {
        return;
    }

    if (true/*ui->cbPointMap->isChecked()*/) {
        for (unsigned i = 0; i < numpoints; i++, obj++) {
            pitem = nullptr;
            pitem = mapPtItem_ColAddress[std::make_pair(obj->ca, obj->address)];
            if (pitem == nullptr) {
                if (ui->cb888Mode->isChecked()) {
                    sprintf(buf, "%u-%u-%u", obj->address & 0xFF, (obj->address >> 8) & 0xFF, (obj->address >> 16) & 0xFF);
                }
                else {
                    sprintf(buf, "%u", obj->address);
                }

                // insere
                rw = ui->twPontos->rowCount();
                ui->twPontos->insertRow(rw);
                QTableWidgetItem* newItem = new QTableWidgetItem(buf);
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 0, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColAddress[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem();
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 1, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColCommonAddress[std::make_pair(obj->ca, obj->address)] =
                    newItem;

                newItem = new QTableWidgetItem();
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 2, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColValue[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem();
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 3, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColType[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem();
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 4, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColCause[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem();
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 5, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColFlags[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem("0");
                newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                ui->twPontos->setItem(rw, 6, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColCount[std::make_pair(obj->ca, obj->address)] = newItem;

                newItem = new QTableWidgetItem();
                ui->twPontos->setItem(rw, 7, newItem);
                newItem->setFlags(Qt::ItemIsSelectable);
                mapPtItem_ColTimeTag[std::make_pair(obj->ca, obj->address)] = newItem;

                inserted = true;
            }

            sprintf(buf, "%9.3f", double(obj->value));
            mapPtItem_ColValue[std::make_pair(obj->ca, obj->address)]->setText(buf);
            sprintf(buf, "%u", obj->ca);
            mapPtItem_ColCommonAddress[std::make_pair(obj->ca, obj->address)]
                ->setText(buf);
            sprintf(buf, "%d:%s", obj->type, i104->asduTiStr(obj->type).c_str());
            mapPtItem_ColType[std::make_pair(obj->ca, obj->address)]->setText(buf);
            sprintf(buf, "%d:%s", obj->cause, i104->causeStr(obj->cause).c_str());
            mapPtItem_ColCause[std::make_pair(obj->ca, obj->address)]->setText(buf);
            sprintf(buf, "%d",
                1 + mapPtItem_ColCount[std::make_pair(obj->ca, obj->address)]
                ->text()
                .toInt());
            mapPtItem_ColCount[std::make_pair(obj->ca, obj->address)]->setText(buf);

            QDateTime current = QDateTime::currentDateTime();
            sprintf(
                buftt, "Local: %s",
                current.toString("yyyy/MM/dd hh:mm:ss.zzz").toStdString().c_str());

            switch (obj->type) {
            case iec104_class::M_EP_TD_1:
                fmtCP56Time(buftt, &obj->timetag);
                sprintf(buf, "%s%s%s%s%s%s %dms", dblmsg[obj->dp], obj->bl ? "bl " : "",
                    obj->nt ? "nt " : "", obj->sb ? "sb " : "",
                    obj->iv ? "iv " : "", obj->ei ? "ei " : "",
                    obj->elapsed_time.milliseconds);
                break;
            case iec104_class::M_EP_TE_1:
                fmtCP56Time(buftt, &obj->timetag);
                sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s %dms", obj->bl ? "bl " : "",
                    obj->nt ? "nt " : "", obj->sb ? "sb " : "",
                    obj->iv ? "iv " : "", obj->ei ? "ei " : "",
                    obj->spe.gs ? "gs " : "", obj->spe.sl1 ? "sl1 " : "",
                    obj->spe.sl2 ? "sl2 " : "", obj->spe.sl3 ? "sl3 " : "",
                    obj->spe.sie ? "sie " : "", obj->spe.srd ? "srd " : "",
                    obj->elapsed_time.milliseconds);
                break;
            case iec104_class::M_EP_TF_1:
                fmtCP56Time(buftt, &obj->timetag);
                sprintf(buf, "%s%s%s%s%s%s%s%s%s %dms", obj->bl ? "bl " : "",
                    obj->nt ? "nt " : "", obj->sb ? "sb " : "",
                    obj->iv ? "iv " : "", obj->ei ? "ei " : "",
                    obj->oci.gc ? "gc " : "", obj->oci.cl1 ? "cl1 " : "",
                    obj->oci.cl2 ? "cl2 " : "", obj->oci.cl3 ? "cl3 " : "",
                    obj->elapsed_time.milliseconds);
                break;

            case iec104_class::M_SP_TB_1: // 30
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_SP_NA_1: // 1
                sprintf(buf, "%s%s%s%s%s", obj->sp ? "on " : "off ",
                    obj->iv ? "iv " : "", obj->bl ? "bl " : "",
                    obj->sb ? "sb " : "", obj->nt ? "nt " : "");
                break;
            case iec104_class::M_DP_TB_1: // 31
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_DP_NA_1: // 3
                sprintf(buf, "%s%s%s%s%s", dblmsg[obj->dp], obj->iv ? "iv " : "",
                    obj->bl ? "bl " : "", obj->sb ? "sb " : "",
                    obj->nt ? "nt " : "");
                break;
            case iec104_class::M_ST_TB_1: // 32
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_ST_NA_1: // 5
                sprintf(buf, "%s%s%s%s%s%s", obj->ov ? "ov " : "", obj->iv ? "iv " : "",
                    obj->bl ? "bl " : "", obj->sb ? "sb " : "",
                    obj->nt ? "nt " : "", obj->t ? "t " : "");
                break;

            case iec104_class::M_IT_TB_1: // 37
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_IT_NA_1: // 15
                sprintf(buf, "%s%s%s%s%u", obj->iv ? "iv " : "", obj->cadj ? "ca " : "",
                    obj->cy ? "cy " : "", "sq=", obj->sq);
                break;

            case iec104_class::M_PS_NA_1: // 38
                sprintf(buf,
                    "%s%s%s%s%s ST %d%d%d%d %d%d%d%d %d%d%d%d %d%d%d%d CH %d%d%d%d "
                    "%d%d%d%d %d%d%d%d %d%d%d%d [1-16]",
                    obj->ov ? "ov " : "", obj->bl ? "bl " : "",
                    obj->nt ? "nt " : "", obj->sb ? "sb " : "",
                    obj->iv ? "iv " : "", obj->stcd.st1, obj->stcd.st2,
                    obj->stcd.st3, obj->stcd.st4, obj->stcd.st5, obj->stcd.st6,
                    obj->stcd.st7, obj->stcd.st8, obj->stcd.st9, obj->stcd.st10,
                    obj->stcd.st11, obj->stcd.st12, obj->stcd.st13, obj->stcd.st14,
                    obj->stcd.st15, obj->stcd.st16, obj->stcd.cd1, obj->stcd.cd2,
                    obj->stcd.cd3, obj->stcd.cd4, obj->stcd.cd5, obj->stcd.cd6,
                    obj->stcd.cd7, obj->stcd.cd8, obj->stcd.cd9, obj->stcd.cd10,
                    obj->stcd.cd11, obj->stcd.cd12, obj->stcd.cd13, obj->stcd.cd14,
                    obj->stcd.cd15, obj->stcd.cd16);
                break;

            case iec104_class::M_BO_TB_1: // 33
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_BO_NA_1: // 7
                sprintf(buf,
                    "%s%s%s%s%s ST %d%d%d%d %d%d%d%d %d%d%d%d %d%d%d%d %d%d%d%d "
                    "%d%d%d%d %d%d%d%d %d%d%d%d [1-32]",
                    obj->ov ? "ov " : "", obj->bl ? "bl " : "",
                    obj->nt ? "nt " : "", obj->sb ? "sb " : "",
                    obj->iv ? "iv " : "", obj->bsi.st1, obj->bsi.st2, obj->bsi.st3,
                    obj->bsi.st4, obj->bsi.st5, obj->bsi.st6, obj->bsi.st7,
                    obj->bsi.st8, obj->bsi.st9, obj->bsi.st10, obj->bsi.st11,
                    obj->bsi.st12, obj->bsi.st13, obj->bsi.st14, obj->bsi.st15,
                    obj->bsi.st16, obj->bsi.st17, obj->bsi.st18, obj->bsi.st19,
                    obj->bsi.st20, obj->bsi.st21, obj->bsi.st22, obj->bsi.st23,
                    obj->bsi.st24, obj->bsi.st25, obj->bsi.st26, obj->bsi.st27,
                    obj->bsi.st28, obj->bsi.st29, obj->bsi.st30, obj->bsi.st31, obj->bsi.st32);
                break;

            case iec104_class::M_ME_TD_1: // 34
            case iec104_class::M_ME_TE_1: // 35
            case iec104_class::M_ME_TF_1: // 36
                fmtCP56Time(buftt, &obj->timetag);
                [[fallthrough]];
            case iec104_class::M_ME_NA_1: // 9
            case iec104_class::M_ME_NB_1: // 11
            case iec104_class::M_ME_NC_1: // 13
            case iec104_class::M_ME_ND_1: // 21
                sprintf(buf, "%s%s%s%s%s", obj->ov ? "ov " : "", obj->iv ? "iv " : "",
                    obj->bl ? "bl " : "", obj->sb ? "sb " : "",
                    obj->nt ? "nt " : "");
                break;
            }

            mapPtItem_ColFlags[std::make_pair(obj->ca, obj->address)]->setText(buf);
            mapPtItem_ColTimeTag[std::make_pair(obj->ca, obj->address)]->setText(
                buftt);
        }

        if (inserted) {
            pointTableSortPending = true;
            pointTableResizePending = true;
        }
    }
}

void MainWindow::slot_timer_logmsg()
{
    static const int maxLogMsgsPerTick = 250;

    // adjust size of rows and columns
    if (!(++logTickCount % 50))
        if (pointTableResizePending && pendingDataIndications.isEmpty()) {
            ui->twPontos->resizeRowsToContents();
            ui->twPontos->resizeColumnsToContents();
            pointTableResizePending = false;
        }

    if (i104->mLog.haveMsg()) {
        // append the whole batch inside a single edit block: one layout/paint pass;
        // the document's maximumBlockCount (set in the .ui) discards the oldest
        // lines automatically once the log is full
        int logMsgsProcessed = 0;
        QTextCursor cur(ui->lwLog->document());
        cur.movePosition(QTextCursor::End);
        cur.beginEditBlock();
        while (i104->mLog.haveMsg() && logMsgsProcessed < maxLogMsgsPerTick) {
            const QString msg = QString::fromStdString(i104->mLog.pullMsg());

            QTextCharFormat fmt;
            if (msg.contains(QLatin1String("I104M"))) {
                fmt.setForeground(Qt::darkGray);
            }
            else if (msg.contains(QLatin1String("COMMAND"))) {
                fmt.setForeground(Qt::darkGray);
                fmt.setBackground(Qt::lightGray);
            }
            else if (msg.contains(QLatin1Char('['))) {
                fmt.setForeground(Qt::red);
            }
            else {
                fmt.setForeground(Qt::darkGray);
            }

            if (!cur.atStart())
                cur.insertBlock();
            cur.insertText(msg, fmt);
            logMsgsProcessed++;
        }
        cur.endEditBlock();

        if (ui->cbAutoScroll->isChecked()) {
            QScrollBar* vbar = ui->lwLog->verticalScrollBar();
            vbar->setValue(vbar->maximum());
        }
    }
}

void MainWindow::slot_interrogationActConfIndication() {}

void MainWindow::slot_interrogationActTermIndication() {}

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

void MainWindow::slot_commandActRespIndication(const iec_obj& obj)
{

    char buf[1000];
    char buftt[1000];
    int rw = -1;
    QTableWidgetItem* pitem;
    static const char* pnmsg[] = { "pos ", "neg " };
    static const char* selmsg[] = { "exe ", "sel " };
    static const char* sglmsg[] = { "off ", "on " };
    static const char* dblmsg[] = { "tra ", "off ", "on ", "ind " };
    static const char* qumsg[] = { "uns ", "shp ", "lop ", "per ", "res " };
    static const char* rcsmsg[] = { "na0 ", "dec ", "inc ", "na3 " };
    static const char* kpamsg[] = { "unu ", "thr ", "fil ",
        "lli ", "hli ", "res " };
    // static const char* qpamsg[] = { "unu ", "gen ", "obj ", "trm ", "res " };
    // clamp device provided indexes to the "reserved" entry of the tables above
    const unsigned qu_idx = obj.qu > 3 ? 4 : obj.qu;
    const unsigned kpa_idx = obj.kpa > 4 ? 5 : obj.kpa;
    const unsigned qpa_idx = obj.qpa > 4 ? 5 : obj.qpa;

    if (obj.address == 0)
        return;

    auto& s = AppSettings::instance();

    if (true/*ui->cbPointMap->isChecked()*/) {
        pitem = nullptr;
        pitem = mapPtItem_ColAddress[std::make_pair(obj.ca, obj.address)];
        if (pitem == nullptr) {
            sprintf(buf, "%06u", obj.address);

            // insere
            rw = ui->twPontos->rowCount();
            ui->twPontos->insertRow(rw);
            QTableWidgetItem* newItem = new QTableWidgetItem(buf);
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 0, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColAddress[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 1, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCommonAddress[std::make_pair(obj.ca, obj.address)] =
                newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 2, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 3, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColType[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 4, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCause[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 5, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColFlags[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem("0");
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            ui->twPontos->setItem(rw, 6, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCount[std::make_pair(obj.ca, obj.address)] = newItem;

            newItem = new QTableWidgetItem();
            ui->twPontos->setItem(rw, 7, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColTimeTag[std::make_pair(obj.ca, obj.address)] = newItem;
        }

        sprintf(buf, "%9.3f", double(obj.value));
        mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)]->setText(buf);
        sprintf(buf, "%u", obj.ca);
        mapPtItem_ColCommonAddress[std::make_pair(obj.ca, obj.address)]->setText(
            buf);
        sprintf(buf, "%d", obj.type);
        mapPtItem_ColType[std::make_pair(obj.ca, obj.address)]->setText(buf);
        sprintf(buf, "%d", obj.cause);
        mapPtItem_ColCause[std::make_pair(obj.ca, obj.address)]->setText(buf);
        sprintf(buf, "%d",
            1 + mapPtItem_ColCount[std::make_pair(obj.ca, obj.address)]
            ->text()
            .toInt());
        mapPtItem_ColCount[std::make_pair(obj.ca, obj.address)]->setText(buf);

        QDateTime current = QDateTime::currentDateTime();
        sprintf(buftt, "Local: %s",
            current.toString("yyyy/MM/dd hh:mm:ss.zzz").toStdString().c_str());
        switch (obj.type) {
        case iec104_class::C_SC_TA_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_SC_NA_1:
            sprintf(buf, "%d", int(obj.scs));
            mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)]->setText(buf);
            sprintf(buf, "%s%s%s%s", pnmsg[obj.pn], sglmsg[obj.scs],
                selmsg[obj.se], qumsg[qu_idx]);
            break;
        case iec104_class::C_DC_TA_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_DC_NA_1:
            sprintf(buf, "%d", int(obj.dcs));
            mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)]->setText(buf);
            sprintf(buf, "%s%s%s%s", pnmsg[obj.pn], dblmsg[obj.dcs],
                selmsg[obj.se], qumsg[qu_idx]);
            break;
        case iec104_class::C_RC_TA_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_RC_NA_1:
            sprintf(buf, "%d", int(obj.rcs));
            mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)]->setText(buf);
            sprintf(buf, "%s%s%s", pnmsg[obj.pn], rcsmsg[obj.rcs], selmsg[obj.se]);
            break;
        case iec104_class::C_SE_TA_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_SE_NA_1:
            sprintf(buf, "%s%s", pnmsg[obj.pn], selmsg[obj.se]);
            break;
        case iec104_class::C_SE_TB_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_SE_NB_1:
            sprintf(buf, "%s%s", pnmsg[obj.pn], selmsg[obj.se]);
            break;
        case iec104_class::C_SE_TC_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_SE_NC_1:
            sprintf(buf, "%s%s", pnmsg[obj.pn], selmsg[obj.se]);
            break;
        case iec104_class::C_BO_TA_1:
            fmtCP56Time(buftt, &obj.timetag);
            [[fallthrough]];
        case iec104_class::C_BO_NA_1:
            sprintf(buf, "%s", pnmsg[obj.pn]);
            break;
        case iec104_class::P_ME_NA_1:
            [[fallthrough]];
        case iec104_class::P_ME_NB_1:
            [[fallthrough]];
        case iec104_class::P_ME_NC_1:
            sprintf(buf, "%s%s%s%s", pnmsg[obj.pn], kpamsg[kpa_idx],
                obj.lpc ? "lpc " : "", obj.pop ? "pop " : "");
            break;
        case iec104_class::P_AC_NA_1:
            sprintf(buf, "%s%s", pnmsg[obj.pn], kpamsg[qpa_idx]);
            break;
        }

        mapPtItem_ColFlags[std::make_pair(obj.ca, obj.address)]->setText(buf);
        mapPtItem_ColTimeTag[std::make_pair(obj.ca, obj.address)]->setText(buftt);
    }

    bool is_select = false;

    if (obj.cause == iec104_class::REQUEST ||
        obj.cause == iec104_class::ACTIVATION ||
        obj.cause == iec104_class::ACTCONFIRM)
        if (LastCommandAddress == obj.address) {
            i104->mLog.pushMsg("     COMMAND CONF INDICATION");
            is_select = (obj.se == iec104_class::SELECT);

            // if confirmed select, execute
            if (is_select && obj.pn == iec104_class::POSITIVE) {
                // if defined ASDU address on UI, use it
                // else will set to zero and use slave address (send Command will
                // substitute zero to slave address)
                iec_obj executeObj = obj;
                executeObj.ca = ui->leASDUAddr->text().toUShort();
                executeObj.se = iec104_class::EXECUTE;
                queueProtocolCommand(executeObj);
            }
        }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    shutdownProtocolThread();
    event->accept();
}

void MainWindow::on_cbLog_clicked()
{
    if (ui->cbLog->isChecked()) {
        i104->mLog.activateLog();
        QDate dt = QDate::currentDate();
        QString str = dt.toString() + QString(" - ") + QString(VERSION);
        i104->mLog.pushMsg(str.toStdString().c_str());
    }
    else
        i104->mLog.deactivateLog();
}

void MainWindow::on_pbCopyClipb_clicked()
{
    QApplication::clipboard()->setText(ui->lwLog->toPlainText());
}

void MainWindow::on_pbCopyVals_clicked()
{
    QString text = "Address\tCA\tValue\tASDU\tCause\tFlags\tCount\tTimeTag\n";

    for (int i = 0; i < ui->twPontos->rowCount(); i++) {
        for (int j = 0; j < 8; j++) {
            text = text + ui->twPontos->item(i, j)->text() + "\t";
        }
        text = text + "\n";
    }

    QApplication::clipboard()->setText(text);
}

void MainWindow::fmtCP56Time(char* buf, const cp56time2a* timetag)
{
    if (timetag->month == 0 || timetag->mday == 0)
        return;
    sprintf(buf, "Field: %02d/%02d/%02d %02d:%02d:%02d.%03d %s %s", timetag->year,
        timetag->month, timetag->mday, timetag->hour, timetag->min,
        timetag->msec / 1000, timetag->msec % 1000,
        timetag->iv ? "iv" : "ok",
        timetag->su ? "su" : "");
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

    for (auto const& pair : mapPtItem_ColAddress) {
        unsigned int address = pair.first.second;
        char buf[64];
        if (arg1) {
            sprintf(buf, "%u-%u-%u", address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xFF);
        }
        else {
            sprintf(buf, "%u", address);
        }
        pair.second->setText(buf);
    }
}
