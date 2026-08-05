#pragma once
#include <functional>
#include <LogController.h>
#include <memory>
#include <PointController.h>
#include <qevent.h>
#include <qglobal.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <qobjectdefs.h>
#include <qqueue.h>
#include <qstring.h>
#include <qthread.h>
#include <qtimer.h>
#include <qvector.h>
#include <qwidget.h>
#include "iec104/iec104_class.h"
#include "QIec104.h"

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void closeEvent(QCloseEvent* event);

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_pbSettingsDialog_clicked(); // App config button pressed
    void on_pbSendCommandsButton_clicked(); // Send Command pressed
    void on_pbConnect_clicked(); // connect button pressed
    void on_pbGI_clicked(); // GI button pressed

    void slot_processPendingUiData();
    void slot_dataIndication(const QVector<iec_obj>& objects);
    void slot_tcpconnect(const QString& peerAddress); // tcp connect for iec104
    void slot_tcpdisconnect();      // tcp disconnect for iec104

    void on_pbCopyVals_clicked(); // copy values table to clipboard
    void on_cbTheme_currentIndexChanged(int index); // Theme selection changed

    void on_cb888Mode_stateChanged(int arg1);
    void reloadProtocolSettings();

    void on_cbLog_clicked(); // Check box for log messages changed
    void on_pbCopyClipb_clicked(); // copy log messages to clipboard

private:
    void queueProtocolCall(const std::function<void(QIec104*)>& fn);

    void shutdownProtocolThread();

    QLabel m_lbStatus;
    QLabel m_lbMode;

    std::unique_ptr<Ui::MainWindow> ui;
    std::unique_ptr<LogController> m_logController;
    std::unique_ptr<PointController> m_pointController;

    QTimer* tmUiDataPump; // timer to batch UI point updates
    QIec104* i104 = nullptr;
    QThread protocolThread;
    QQueue<QVector<iec_obj>> pendingDataIndications;
    qsizetype pendingDataPointCount = 0;
    bool pointTableSortPending = false;
    bool pointTableResizePending = false;

    bool ProtocolKeepAliveActive = false;
    bool ProtocolShutdown = false;
};