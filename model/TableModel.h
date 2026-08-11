#pragma once
#include <iec104/iec104_class.h>
#include <map>
#include <qabstractitemmodel.h>
#include <qglobal.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qqueue.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtimer.h>
#include <qvariant.h>
#include <qvector.h>
#include <utility>
#include "TableRow.h"

class TableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum PointColumn
    {
        Address,
        CA,
        Value,
        Type,
        Cause,
        Flags,
        Count,
        TimeTag,
    };

    explicit TableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setHorizontalHeaderLabels(const QStringList& labels);

    int createRow(int ca, int address, const QString& addressText);
    int findRow(int ca, int address) const;

    void addRow(TableRow row);
    void removeRow(int row);

    void clear();

    TableRow& row(int index);
    const TableRow& row(int index) const;

    QVariant cellData(int row, int column) const; 
    bool setCellData(int row, int column, const QVariant& value, int role = Qt::EditRole);

    void setColumnCount(int count);

    void set888Mode(bool arg);
    bool processDataIndicationBatch(const QVector<iec_obj>& objects);
    void copyToClipboard() const;
    QString formatAddress(int address) const;

signals:
    void tableContentChanged();

public slots:
    void slot_commandActRespIndication(const iec_obj& obj);
    void slot_dataIndication(const QVector<iec_obj>& objects);

private slots:
    void slot_processPendingUiData();

private:
    QVector<TableRow> rows_;
    std::map<std::pair<int, int>, int> rowIndex_;
    QStringList horizontalHeaders_;
    int columnCount_ = 0;

    QTimer tmUiDataPump; // timer to batch UI point updates
    qsizetype m_pendingDataPointCount = 0;
    QQueue<QVector<iec_obj>> m_pendingDataIndications;
    bool m_mode888 = false;
};