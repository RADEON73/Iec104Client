#pragma once
#include <memory>
#include <qabstractsocket.h>
#include <qglobal.h>
#include <qlabel.h>
#include <qmainwindow.h>
#include <qobjectdefs.h>
#include <qwidget.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TableModel;
class QIec104;
class ProtocolController;
class LogController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_pb_SettingsDialog_clicked(); //Открывает диалог настройки подключения
    void on_pb_Connect_clicked(); //Запускает цикл подключения к серверам
    void on_cb_Theme_currentIndexChanged(int index); //При изменении темы приложения

    void on_pb_PointsGI_clicked(); //Запускает цикл общего опроса тегов
    void on_pb_PointsCopy_clicked(); //Копирует содержимое списка сигналов в буфер

    void on_cb_888Mode_stateChanged(int state); //Изменилось состояние режима 888
    void on_pb_SendCommand_clicked(); //Нажатие кнопки "Отправить команду"

    void on_gb_LogOn_toggled(bool on); //Включить/отключить лог
    void on_cb_LogAutoscroll_toggled(bool on); //Автоскролл к концу лога
    void on_pb_LogCopy_clicked(); //Копирует содержимое лога в буфер

    void slot_stateChanged(QAbstractSocket::SocketState state); // State changed for iec104

private:
    std::unique_ptr<Ui::MainWindow> ui;

    std::unique_ptr<TableModel> m_model;
    std::unique_ptr<ProtocolController> m_protocolController;
    std::unique_ptr<LogController> m_logController;

    QLabel m_lbStatus;
};