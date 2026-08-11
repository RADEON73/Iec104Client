#include "TableModel.h"

#include <qabstractitemmodel.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qelapsedtimer.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qstring.h>
#include <qtimer.h>
#include <qvariant.h>
#include <utility>
#include "TableCell.h"
#include "TableRow.h"

static void fmtCP56Time(QString& buf, const cp56time2a* timetag)
{
    if (timetag->month == 0 || timetag->mday == 0)
        return;

    buf = QStringLiteral("Field: %1/%2/%3 %4:%5:%6.%7 %8 %9")
        .arg(timetag->year, 2, 10, QLatin1Char('0'))
        .arg(timetag->month, 2, 10, QLatin1Char('0'))
        .arg(timetag->mday, 2, 10, QLatin1Char('0'))
        .arg(timetag->hour, 2, 10, QLatin1Char('0'))
        .arg(timetag->min, 2, 10, QLatin1Char('0'))
        .arg(timetag->msec / 1000, 2, 10, QLatin1Char('0'))
        .arg(timetag->msec % 1000, 3, 10, QLatin1Char('0'))
        .arg(timetag->iv ? QStringLiteral("iv") : QStringLiteral("ok"))
        .arg(timetag->su ? QStringLiteral("su") : QString());
}

TableModel::TableModel(QObject* parent) : QAbstractTableModel(parent) 
{
    setColumnCount(8);
    setHorizontalHeaderLabels({
        QStringLiteral("Адрес"),
        QStringLiteral("АСДУ"),
        QStringLiteral("Значение"),
        QStringLiteral("Тип"),
        QStringLiteral("Причина"),
        QStringLiteral("Флаги"),
        QStringLiteral("Счетчик"),
        QStringLiteral("Временная метка")
        });

    connect(&tmUiDataPump, &QTimer::timeout, this, &TableModel::slot_processPendingUiData);
}

int TableModel::rowCount(const QModelIndex& parent) const 
{ 
    if (parent.isValid())
        return 0; 
    return rows_.size(); 
}

int TableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return columnCount_;
}



QVariant TableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    if (index.row() < 0 || index.row() >= rows_.size())
        return {};

    if (index.column() < 0 || index.column() >= columnCount_)
        return {};

    const TableCell& cell = rows_[index.row()].cell(index.column());

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return cell.data();
    default:
        return {};
    }
}

bool TableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;

    if (role != Qt::EditRole)
        return false;

    if (index.row() < 0 || index.row() >= rows_.size())
        return false;

    if (index.column() < 0 || index.column() >= columnCount_)
        return false;

    rows_[index.row()].cell(index.column()).setData(value);

    emit dataChanged(index, index, { Qt::DisplayRole, Qt::EditRole });

    return true;
}

Qt::ItemFlags TableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled |
        Qt::ItemIsSelectable |
        Qt::ItemIsEditable;
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const 
{ 
    if (role != Qt::DisplayRole) 
        return {};
    if (orientation == Qt::Horizontal) { 
        if (section >= 0 && section < horizontalHeaders_.size())
            return horizontalHeaders_[section]; 
    } 
    return {}; 
}

void TableModel::setHorizontalHeaderLabels(const QStringList& labels)
{ 
    horizontalHeaders_ = labels; 
    if (horizontalHeaders_.size() > columnCount_) 
        setColumnCount(horizontalHeaders_.size());
    if (columnCount_ > 0) { 
        emit headerDataChanged(Qt::Horizontal, 0, columnCount_ - 1); 
    } 
}

int TableModel::createRow(int ca, int address, const QString& addressText)
{
    const int row = rowCount();
    TableRow tableRow;
    tableRow.setColumnCount(columnCount());

    tableRow.setCA(ca);
    tableRow.setAddress(address);

    for (auto i  = 0; i < tableRow.columnCount(); ++i)
        tableRow.cell(i).setData(QString());
    tableRow.cell(0).setData(addressText);

    addRow(std::move(tableRow));
    rowIndex_[{ca, address}] = row;
    return row;
}

int TableModel::findRow(int ca, int address) const
{
    const auto it = rowIndex_.find({ ca, address });
    if (it == rowIndex_.end())
        return -1;
    return it->second;
}

void TableModel::addRow(TableRow row)
{
    const int rowIndex = rows_.size();
    row.setColumnCount(columnCount_);
    beginInsertRows({}, rowIndex, rowIndex);
    rows_.append(std::move(row));
    endInsertRows();
}

void TableModel::removeRow(int row)
{
    if (row < 0 || row >= rows_.size())
        return;

    beginRemoveRows({}, row, row);
    rows_.removeAt(row);
    endRemoveRows();

    rowIndex_.clear();

    for (int i = 0; i < rows_.size(); ++i) {
        rowIndex_[{
            rows_[i].ca(),
                rows_[i].address()
        }] = i;
    }
}

void TableModel::clear()
{ 
    if (rows_.isEmpty()) 
        return; 
    beginRemoveRows({}, 0, rows_.size() - 1); 
    rows_.clear(); 
    rowIndex_.clear();
    endRemoveRows(); 
}

TableRow& TableModel::row(int index)
{
    return rows_[index];
}

const TableRow& TableModel::row(int index) const
{
    return rows_[index];
}

QVariant TableModel::cellData(int row, int column) const 
{ 
    if (row < 0 || row >= rows_.size()) 
        return {}; 
    if (column < 0 || column >= columnCount_)
        return {}; 
    return rows_[row].cell(column).data(); 
}

bool TableModel::setCellData(int row, int column, const QVariant& value, int role) 
{ 
    if (row < 0 || row >= rows_.size())
        return false; 
    if (column < 0 || column >= columnCount_)
        return false; 
    return setData(index(row, column), value, role); 
}

void TableModel::setColumnCount(int count) 
{ 
    if (count < 0)
        count = 0; 
    if (count == columnCount_)
        return; 
    if (count > columnCount_) { 
        beginInsertColumns({}, columnCount_, count - 1); 
        columnCount_ = count; 
        for (TableRow& row : rows_) 
            row.setColumnCount(columnCount_);
        endInsertColumns();
    } 
    else { 
        beginRemoveColumns({}, count, columnCount_ - 1); 
        columnCount_ = count; 
        for (TableRow& row : rows_) 
            row.setColumnCount(columnCount_);
        endRemoveColumns(); 
    }
}

void TableModel::set888Mode(bool arg)
{
    m_mode888 = arg;

    for (int rw = 0; rw < rowCount(); ++rw) {
        const int address = row(rw).address();
        setCellData(rw, PointColumn::Address, formatAddress(address)
        );
    }
}

void TableModel::slot_commandActRespIndication(const iec_obj& obj)
{
    static const char* pnmsg[] = { "pos ", "neg " };
    static const char* selmsg[] = { "exe ", "sel " };
    static const char* sglmsg[] = { "off ", "on " };
    static const char* dblmsg[] = { "tra ", "off ", "on ", "ind " };
    static const char* qumsg[] = { "uns ", "shp ", "lop ", "per ", "res " };
    static const char* rcsmsg[] = { "na0 ", "dec ", "inc ", "na3 " };
    static const char* kpamsg[] = { "unu ", "thr ", "fil ", "lli ", "hli ", "res " };
    const unsigned qu_idx = obj.qu > 3 ? 4 : obj.qu;
    const unsigned kpa_idx = obj.kpa > 4 ? 5 : obj.kpa;
    const unsigned qpa_idx = obj.qpa > 4 ? 5 : obj.qpa;

    if (obj.address == 0)
        return;

    int row = findRow(obj.ca, obj.address);

    if (row < 0) {
        const QString address = QStringLiteral("%1")
            .arg(obj.address, 6, 10, QLatin1Char('0'));
        row = createRow(obj.ca, obj.address, address);
    }

    auto ca = QString::number(obj.ca);
    setCellData(row, PointColumn::CA, ca);

    auto value = QString("%1").arg(obj.value, 9, 'f', 3);
    setCellData(row, PointColumn::Value, value);

    auto type = QString::number(obj.type);
    setCellData(row, PointColumn::Type, type);

    auto cause = QString::number(obj.cause);
    setCellData(row, PointColumn::Cause, cause);

    const int count = cellData(row, 6).toInt() + 1;
    setCellData(row, PointColumn::Count, QString::number(count));

    QString buff;
    QString buftt = QStringLiteral("Local: %1")
        .arg(QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss.zzz"));

    switch (obj.type) {
    case iec104_class::C_SC_TA_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_SC_NA_1:
        buff = QString::number(obj.scs);
        setCellData(row, 2, buff);
        buff = QString("%1%2%3%4")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(sglmsg[obj.scs]))
            .arg(QString::fromLatin1(selmsg[obj.se]))
            .arg(QString::fromLatin1(qumsg[qu_idx]));
        break;
    case iec104_class::C_DC_TA_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_DC_NA_1:
        buff = QString::number(obj.dcs);
        setCellData(row, 2, buff);
        buff = QString("%1%2%3%4")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(dblmsg[obj.dcs]))
            .arg(QString::fromLatin1(selmsg[obj.se]))
            .arg(QString::fromLatin1(qumsg[qu_idx]));
        break;
    case iec104_class::C_RC_TA_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_RC_NA_1:
        buff = QString::number(obj.rcs);
        setCellData(row, 2, buff);
        buff = QString("%1%2%3")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(rcsmsg[obj.rcs]))
            .arg(QString::fromLatin1(selmsg[obj.se]));
        break;
    case iec104_class::C_SE_TA_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_SE_NA_1:
        buff = QString("%1%2")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(selmsg[obj.se]));
        break;
    case iec104_class::C_SE_TB_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_SE_NB_1:
        buff = QString("%1%2")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(selmsg[obj.se]));
        break;
    case iec104_class::C_SE_TC_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_SE_NC_1:
        buff = QString("%1%2")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(selmsg[obj.se]));
        break;
    case iec104_class::C_BO_TA_1:
        fmtCP56Time(buftt, &obj.timetag);
        [[fallthrough]];
    case iec104_class::C_BO_NA_1:
        buff = QString::fromLatin1(pnmsg[obj.pn]);
        break;
    case iec104_class::P_ME_NA_1:
        [[fallthrough]];
    case iec104_class::P_ME_NB_1:
        [[fallthrough]];
    case iec104_class::P_ME_NC_1:
        buff = QString("%1%2%3%4")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(kpamsg[kpa_idx]))
            .arg(obj.lpc ? QStringLiteral("lpc ") : QString())
            .arg(obj.pop ? QStringLiteral("pop ") : QString());
        break;
    case iec104_class::P_AC_NA_1:
        buff = QString("%1%2")
            .arg(QString::fromLatin1(pnmsg[obj.pn]))
            .arg(QString::fromLatin1(kpamsg[qpa_idx]));
        break;
    }

    setCellData(row, PointColumn::Flags, buff);
    setCellData(row, PointColumn::TimeTag, buftt);
}

bool TableModel::processDataIndicationBatch(const QVector<iec_obj>& objects)
{
    if (objects.isEmpty())
        return false;

    static const char* dblmsg[] = { "tra ", "off ", "on ", "ind " };

    const iec_obj* obj = objects.constData();

    bool inserted = false;

    for (auto i = 0; i < objects.size(); i++, obj++) {

        int row = findRow(obj->ca, obj->address);

        if (row < 0) {
            row = createRow(obj->ca, obj->address, formatAddress(obj->address));
            inserted = true;
        }

        auto ca = QString::number(obj->ca);
        setCellData(row, PointColumn::CA, ca);

        auto value = QStringLiteral("%1").arg(obj->value, 9, 'f', 3);
        setCellData(row, PointColumn::Value, value);

        auto type = QString("%1:%2")
            .arg(obj->type)
            .arg(QString::fromStdString(iec104_class::asduTiStr(obj->type)));
        setCellData(row, PointColumn::Type, type);

        auto cause = QString("%1:%2")
            .arg(obj->cause)
            .arg(QString::fromStdString(iec104_class::causeStr(obj->cause)));
        setCellData(row, PointColumn::Cause, cause);

        const int count = cellData(row, 6).toInt() + 1;
        setCellData(row, PointColumn::Count, QString::number(count));

        QString buff;
        QString buftt = QStringLiteral("Local: %1")
            .arg(QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss.zzz"));

        switch (obj->type) {
        case iec104_class::M_EP_TD_1:
            fmtCP56Time(buftt, &obj->timetag);
            buff = QString("%1%2%3%4%5%6 %7ms")
                .arg(QString::fromLatin1(dblmsg[obj->dp]))
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->ei ? QStringLiteral("ei ") : QString())
                .arg(obj->elapsed_time.milliseconds);
            break;
        case iec104_class::M_EP_TE_1:
            fmtCP56Time(buftt, &obj->timetag);
            buff = QString("%1%2%3%4%5%6%7%8%9%10%11 %12ms")
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->ei ? QStringLiteral("ei ") : QString())
                .arg(obj->spe.gs ? QStringLiteral("gs ") : QString())
                .arg(obj->spe.sl1 ? QStringLiteral("sl1 ") : QString())
                .arg(obj->spe.sl2 ? QStringLiteral("sl2 ") : QString())
                .arg(obj->spe.sl3 ? QStringLiteral("sl3 ") : QString())
                .arg(obj->spe.sie ? QStringLiteral("sie ") : QString())
                .arg(obj->spe.srd ? QStringLiteral("srd ") : QString())
                .arg(obj->elapsed_time.milliseconds);
            break;
        case iec104_class::M_EP_TF_1:
            fmtCP56Time(buftt, &obj->timetag);
            buff = QString("%1%2%3%4%5%6%7%8%9 %10ms")
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->ei ? QStringLiteral("ei ") : QString())
                .arg(obj->oci.gc ? QStringLiteral("gc ") : QString())
                .arg(obj->oci.cl1 ? QStringLiteral("cl1 ") : QString())
                .arg(obj->oci.cl2 ? QStringLiteral("cl2 ") : QString())
                .arg(obj->oci.cl3 ? QStringLiteral("cl3 ") : QString())
                .arg(obj->elapsed_time.milliseconds);
            break;
        case iec104_class::M_SP_TB_1: // 30
            fmtCP56Time(buftt, &obj->timetag);
            [[fallthrough]];
        case iec104_class::M_SP_NA_1: // 1
            buff = QString("%1%2%3%4%5")
                .arg(obj->sp ? QStringLiteral("on ") : QStringLiteral("off "))
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString());
            break;
        case iec104_class::M_DP_TB_1: // 31
            fmtCP56Time(buftt, &obj->timetag);
            [[fallthrough]];
        case iec104_class::M_DP_NA_1: // 3
            buff = QString("%1%2%3%4%5")
                .arg(QString::fromLatin1(dblmsg[obj->dp]))
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString());
            break;
        case iec104_class::M_ST_TB_1: // 32
            fmtCP56Time(buftt, &obj->timetag);
            [[fallthrough]];
        case iec104_class::M_ST_NA_1: // 5
            buff = QString("%1%2%3%4%5%6")
                .arg(obj->ov ? QStringLiteral("ov ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->t ? QStringLiteral("t ") : QString());
            break;

        case iec104_class::M_IT_TB_1: // 37
            fmtCP56Time(buftt, &obj->timetag);
            [[fallthrough]];
        case iec104_class::M_IT_NA_1: // 15
            buff = QString("%1%2%3%4%5")
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->cadj ? QStringLiteral("ca ") : QString())
                .arg(obj->cy ? QStringLiteral("cy ") : QString())
                .arg(QStringLiteral("sq="))
                .arg(obj->sq);
            break;

        case iec104_class::M_PS_NA_1: // 38
            buff = QString("%1%2%3%4%5 ST %6%7%8%9 %10%11%12%13 %14%15%16%17 CH %18%19%20%21 %22%23%24%25 %26%27%28%29 [1-16]")
                .arg(obj->ov ? QStringLiteral("ov ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->stcd.st1)
                .arg(obj->stcd.st2)
                .arg(obj->stcd.st3)
                .arg(obj->stcd.st4)
                .arg(obj->stcd.st5)
                .arg(obj->stcd.st6)
                .arg(obj->stcd.st7)
                .arg(obj->stcd.st8)
                .arg(obj->stcd.st9)
                .arg(obj->stcd.st10)
                .arg(obj->stcd.st11)
                .arg(obj->stcd.st12)
                .arg(obj->stcd.st13)
                .arg(obj->stcd.st14)
                .arg(obj->stcd.st15)
                .arg(obj->stcd.st16)
                .arg(obj->stcd.cd1)
                .arg(obj->stcd.cd2)
                .arg(obj->stcd.cd3)
                .arg(obj->stcd.cd4)
                .arg(obj->stcd.cd5)
                .arg(obj->stcd.cd6)
                .arg(obj->stcd.cd7)
                .arg(obj->stcd.cd8)
                .arg(obj->stcd.cd9)
                .arg(obj->stcd.cd10)
                .arg(obj->stcd.cd11)
                .arg(obj->stcd.cd12)
                .arg(obj->stcd.cd13)
                .arg(obj->stcd.cd14)
                .arg(obj->stcd.cd15)
                .arg(obj->stcd.cd16);
            break;

        case iec104_class::M_BO_TB_1: // 33
            fmtCP56Time(buftt, &obj->timetag);
            [[fallthrough]];
        case iec104_class::M_BO_NA_1: // 7
            buff = QString("%1%2%3%4%5 ST %6%7%8%9 %10%11%12%13 %14%15%16%17 %18%19%20%21 "
                "%22%23%24%25 %26%27%28%29 %30%31%32%33 [1-32]")
                .arg(obj->ov ? QStringLiteral("ov ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->bsi.st1)
                .arg(obj->bsi.st2)
                .arg(obj->bsi.st3)
                .arg(obj->bsi.st4)
                .arg(obj->bsi.st5)
                .arg(obj->bsi.st6)
                .arg(obj->bsi.st7)
                .arg(obj->bsi.st8)
                .arg(obj->bsi.st9)
                .arg(obj->bsi.st10)
                .arg(obj->bsi.st11)
                .arg(obj->bsi.st12)
                .arg(obj->bsi.st13)
                .arg(obj->bsi.st14)
                .arg(obj->bsi.st15)
                .arg(obj->bsi.st16)
                .arg(obj->bsi.st17)
                .arg(obj->bsi.st18)
                .arg(obj->bsi.st19)
                .arg(obj->bsi.st20)
                .arg(obj->bsi.st21)
                .arg(obj->bsi.st22)
                .arg(obj->bsi.st23)
                .arg(obj->bsi.st24)
                .arg(obj->bsi.st25)
                .arg(obj->bsi.st26)
                .arg(obj->bsi.st27)
                .arg(obj->bsi.st28)
                .arg(obj->bsi.st29)
                .arg(obj->bsi.st30)
                .arg(obj->bsi.st31)
                .arg(obj->bsi.st32);
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
            buff = QString("%1%2%3%4%5")
                .arg(obj->ov ? QStringLiteral("ov ") : QString())
                .arg(obj->iv ? QStringLiteral("iv ") : QString())
                .arg(obj->bl ? QStringLiteral("bl ") : QString())
                .arg(obj->sb ? QStringLiteral("sb ") : QString())
                .arg(obj->nt ? QStringLiteral("nt ") : QString());
            break;
        }
        setCellData(row, PointColumn::Flags, buff);
        setCellData(row, PointColumn::TimeTag, buftt);
    }
    return inserted;
}

void TableModel::copyToClipboard() const
{
    QString text = QStringLiteral("Address\tCA\tValue\tASDU\tCause\tFlags\tCount\tTimeTag\n");
    for (int row = 0; row < rowCount(); ++row) {
        for (int column = 0; column < columnCount(); ++column) {
            text += cellData(row, column).toString();
            text += '\t';
        }
        text += '\n';
    }
    QApplication::clipboard()->setText(text);
}

QString TableModel::formatAddress(int address) const
{
    if (!m_mode888)
        return QString::number(address);

    return QStringLiteral("%1-%2-%3")
        .arg(address & 0xFF)
        .arg((address >> 8) & 0xFF)
        .arg((address >> 16) & 0xFF);
}

void TableModel::slot_processPendingUiData()
{
    if (m_pendingDataIndications.isEmpty()) {
        tmUiDataPump.stop();
        return;
    }

    const qsizetype maxPointsPerTick = m_pendingDataPointCount > 20000 ? 12000 : 4000;
    const qint64 maxMillisPerTick = m_pendingDataPointCount > 20000 ? 30 : 20;

    qsizetype pointsProcessed = 0;
    QElapsedTimer elapsed;
    elapsed.start();

    while (!m_pendingDataIndications.isEmpty()) {
        const QVector<iec_obj> objects = m_pendingDataIndications.dequeue();
        m_pendingDataPointCount -= objects.size();
        if (processDataIndicationBatch(objects)) {
            emit tableContentChanged();
        }
        pointsProcessed += objects.size();
        if (pointsProcessed >= maxPointsPerTick || elapsed.elapsed() >= maxMillisPerTick)
            break;
    }

    if (m_pendingDataIndications.isEmpty())
        tmUiDataPump.stop();
    else
        tmUiDataPump.start(0);
}

void TableModel::slot_dataIndication(const QVector<iec_obj>& objects)
{
    if (objects.isEmpty())
        return;

    m_pendingDataPointCount += objects.size();
    m_pendingDataIndications.enqueue(objects);

    if (!tmUiDataPump.isActive())
        tmUiDataPump.start(0);
}