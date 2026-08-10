#pragma once
#include <memory>
#include <qabstractsocket.h>
#include <qevent.h>
#include <qglobal.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <qobjectdefs.h>
#include <qwidget.h>
#include "iec104/iec104_class.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ProtocolController;
class PointController;
class LogController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event);

private slots:
    void on_pbSettingsDialog_clicked(); // App config button pressed
    void on_pbSendCommandsButton_clicked(); // Send Command pressed
    void on_pbConnect_clicked(); // connect button pressed
    void on_pbGI_clicked(); // GI button pressed

    void slot_actRespIndication(const iec_obj& obj);

    void slot_stateChanged(QAbstractSocket::SocketState state); // State changed for iec104

    void on_pbCopyVals_clicked(); // copy values table to clipboard
    void on_cbTheme_currentIndexChanged(int index); // Theme selection changed

    void on_cb888Mode_stateChanged(int arg1);

    void on_cbLog_clicked(); // Check box for log messages changed
    void on_pbCopyClipb_clicked(); // copy log messages to clipboard

private:
    std::unique_ptr<Ui::MainWindow> ui;

    std::unique_ptr<ProtocolController> m_protocolController;
    std::unique_ptr<PointController> m_pointController;
    std::unique_ptr<LogController> m_logController;

    QLabel m_lbStatus;
};