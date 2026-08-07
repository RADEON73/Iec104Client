//Класс отвечает за организацию таблицы со списками точек (МЭК-104 тегов)
#pragma once
#include <functional>
#include <map>
#include <QIec104.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qstring.h>
#include <qtablewidget.h>
#include <qvector.h>
#include <utility>
#include "iec104/iec104_class.h"
#include <qtimer.h>
#include <qglobal.h>
#include <qqueue.h>

struct CommandData
{
    bool cb888Mode;
    QString leCmdAddressLow;
    QString leCmdAddressMid;
    QString leCmdAddressHigh;
    QString leCmdAddress;
    QString leASDUAddr;
    QString cbCmdAsdu;
    QString cbCmdDuration;
    QString leCmdValue;
    bool cbSBO;
};

class PointController  : public QObject
{
	Q_OBJECT

public:
    explicit PointController(QTableWidget* table, QIec104* i104, QObject *parent = nullptr);
	~PointController();

    void copyToClipboard();
    void clear();

    bool processDataIndicationBatch(const QVector<iec_obj>& objects);
    void sendCommand(CommandData data);
    void commandActRespIndication1(CommandData data, const iec_obj& obj);

    void set888Mode(bool arg);

public slots:
    void slot_dataIndication(const QVector<iec_obj>& objects);
	void slot_resizeTable();

private slots:
    void slot_processPendingUiData();


private:
    void fmtCP56Time(char*, const cp56time2a*);

    void queueProtocolCommand(const iec_obj& obj);
    void queueProtocolCall(const std::function<void(QIec104*)>& fn);

	QTableWidget* m_table;
	QIec104* m_i104;

    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColAddress; // map of points to cells of table
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCommonAddress;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColValue;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColType;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCause;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColFlags;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColCount;
    std::map <std::pair<int, int>, QTableWidgetItem*> mapPtItem_ColTimeTag;

    bool m_mode888 = false;
    unsigned m_lastCommandAddress = 0;
    qsizetype m_pendingDataPointCount = 0;
    QQueue<QVector<iec_obj>> m_pendingDataIndications;
    bool m_pointTableSortPending = false;
    bool m_pointTableResizePending = false;
    QTimer tmUiDataPump; // timer to batch UI point updates
};

