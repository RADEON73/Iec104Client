#pragma once
#include <functional>
#include <map>
#include <memory>
#include <qglobal.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <qobjectdefs.h>
#include <qqueue.h>
#include <qstring.h>
#include <qtablewidget.h>
#include <qthread.h>
#include <qtimer.h>
#include <qvector.h>
#include <qwidget.h>
#include <utility>
#include "iec104/iec104_class.h"
#include "iec104/iec104_types.h"
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
    void on_cbLog_clicked(); // Check box for log messages changed
    void on_pbSettingsDialog_clicked(); // App config button pressed
    void on_pbSendCommandsButton_clicked(); // Send Command pressed
    void on_pbConnect_clicked(); // connect button pressed
    void on_pbGI_clicked(); // GI button pressed
    void slot_timer_logmsg(); // timer for log messages
    void slot_processPendingUiData();
    void slot_dataIndication(const QVector<iec_obj>& objects);
    void slot_interrogationActConfIndication();
    void slot_interrogationActTermIndication();
    void slot_tcpconnect(const QString& peerAddress); // tcp connect for iec104
    void slot_tcpdisconnect();      // tcp disconnect for iec104
    void slot_commandActRespIndication(const iec_obj& obj);

    void on_pbCopyClipb_clicked(); // copy log messages to clipboard
    void on_pbCopyVals_clicked(); // copy values table to clipboard
    void on_cbTheme_currentIndexChanged(int index); // Theme selection changed

    void on_cb888Mode_stateChanged(int arg1);
    void reloadProtocolSettings();

private:
    void queueProtocolCall(const std::function<void(QIec104*)>& fn);
    void queueProtocolCommand(const iec_obj& obj);
    void processDataIndicationBatch(const QVector<iec_obj>& objects);
    void shutdownProtocolThread();

    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColAddress; // map of points to cells of table
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCommonAddress;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColValue;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColType;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCause;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColFlags;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCount;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColTimeTag;

    QLabel m_lbStatus;
    QLabel m_lbMode;

    std::unique_ptr<Ui::MainWindow> ui;
    QTimer* tmLogMsg; // timer to show log messages
    QTimer* tmUiDataPump; // timer to batch UI point updates
    QIec104* i104 = nullptr;
    QThread protocolThread;
    QQueue<QVector<iec_obj>> pendingDataIndications;
    qsizetype pendingDataPointCount = 0;
    bool pointTableSortPending = false;
    bool pointTableResizePending = false;
    int logTickCount = 0;

    unsigned LastCommandAddress = 0;

    bool ProtocolKeepAliveActive = false;
    bool ProtocolShutdown = false;

    void fmtCP56Time(char*, const cp56time2a*);
};