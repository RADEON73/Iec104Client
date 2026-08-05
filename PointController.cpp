#include "PointController.h"

#include <AppSettings.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdatetime.h>
#include <qobject.h>
#include <qstring.h>
#include <qtablewidget.h>
#include "iec104/iec104_class.h"
#include "iec104/iec104_types.h"
#include "QIec104.h"

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

PointController::PointController(QTableWidget* table, QIec104* i104, QObject *parent) : QObject(parent),
	m_table(table),
	m_i104(i104)
{
}

PointController::~PointController() = default;

void PointController::copyToClipboard()
{
    QString text = "Address\tCA\tValue\tASDU\tCause\tFlags\tCount\tTimeTag\n";
    for (int i = 0; i < m_table->rowCount(); i++) {
        for (int j = 0; j < 8; j++)
            text = text + m_table->item(i, j)->text() + "\t";
        text = text + "\n";
    }
    QApplication::clipboard()->setText(text);
}

void PointController::clear()
{
    mapPtItem_ColAddress.clear();
    mapPtItem_ColCommonAddress.clear();
    mapPtItem_ColValue.clear();
    mapPtItem_ColType.clear();
    mapPtItem_ColCause.clear();
    mapPtItem_ColFlags.clear();
    mapPtItem_ColCount.clear();
    mapPtItem_ColTimeTag.clear();
    m_table->clearContents();
    m_table->setRowCount(0);
}

bool PointController::processDataIndicationBatch(const QVector<iec_obj>& objects)
{
    char buf[1500];
    char buftt[1500];
    int rw = -1;
    bool inserted = false;
    QTableWidgetItem* pitem;
    static const char* dblmsg[] = { "tra ", "off ", "on ", "ind " };
    const iec_obj* obj = objects.constData();
    const unsigned numpoints = static_cast<unsigned>(objects.size());

    if (numpoints == 0)
        return false;

    for (unsigned i = 0; i < numpoints; i++, obj++) {
        pitem = nullptr;
        pitem = mapPtItem_ColAddress[std::make_pair(obj->ca, obj->address)];
        if (pitem == nullptr) {

            if (Mode888)
                sprintf(buf, "%u-%u-%u", obj->address & 0xFF, (obj->address >> 8) & 0xFF, (obj->address >> 16) & 0xFF);
            else
                sprintf(buf, "%u", obj->address);

            // insere
            rw = m_table->rowCount();
            m_table->insertRow(rw);
            QTableWidgetItem* newItem = new QTableWidgetItem(buf);
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 0, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColAddress[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 1, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCommonAddress[std::make_pair(obj->ca, obj->address)] =
                newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 2, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColValue[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 3, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColType[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 4, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCause[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem();
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 5, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColFlags[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem("0");
            newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(rw, 6, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColCount[std::make_pair(obj->ca, obj->address)] = newItem;

            newItem = new QTableWidgetItem();
            m_table->setItem(rw, 7, newItem);
            newItem->setFlags(Qt::ItemIsSelectable);
            mapPtItem_ColTimeTag[std::make_pair(obj->ca, obj->address)] = newItem;

            inserted = true;
        }

        sprintf(buf, "%9.3f", double(obj->value));
        mapPtItem_ColValue[std::make_pair(obj->ca, obj->address)]->setText(buf);
        sprintf(buf, "%u", obj->ca);
        mapPtItem_ColCommonAddress[std::make_pair(obj->ca, obj->address)]
            ->setText(buf);
        sprintf(buf, "%d:%s", obj->type, m_i104->asduTiStr(obj->type).c_str());
        mapPtItem_ColType[std::make_pair(obj->ca, obj->address)]->setText(buf);
        sprintf(buf, "%d:%s", obj->cause, m_i104->causeStr(obj->cause).c_str());
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

    return inserted;
}

void PointController::set888Mode(bool arg)
{
    for (auto const& pair : mapPtItem_ColAddress) {
        unsigned int address = pair.first.second;
        char buf[64];
        if (arg)
            sprintf(buf, "%u-%u-%u", address & 0xFF, (address >> 8) & 0xFF, (address >> 16) & 0xFF);
        else
            sprintf(buf, "%u", address);
        pair.second->setText(buf);
    }
}

void PointController::commandActRespIndication1(CommandData data, const iec_obj& obj)
{

    char buf[1000];
    char buftt[1000];
    int rw = -1;
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

    auto& s = AppSettings::instance();

    QTableWidgetItem* pitem = mapPtItem_ColAddress[std::make_pair(obj.ca, obj.address)];
    if (!pitem) {
        sprintf(buf, "%06u", obj.address);

        // insere
        rw = m_table->rowCount();
        m_table->insertRow(rw);
        QTableWidgetItem* newItem = new QTableWidgetItem(buf);
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 0, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColAddress[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem();
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 1, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColCommonAddress[std::make_pair(obj.ca, obj.address)] =
            newItem;

        newItem = new QTableWidgetItem();
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 2, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColValue[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem();
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 3, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColType[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem();
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 4, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColCause[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem();
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 5, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColFlags[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem("0");
        newItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rw, 6, newItem);
        newItem->setFlags(Qt::ItemIsSelectable);
        mapPtItem_ColCount[std::make_pair(obj.ca, obj.address)] = newItem;

        newItem = new QTableWidgetItem();
        m_table->setItem(rw, 7, newItem);
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

    bool is_select = false;

    if (obj.cause == iec104_class::REQUEST ||
        obj.cause == iec104_class::ACTIVATION ||
        obj.cause == iec104_class::ACTCONFIRM)
        if (LastCommandAddress == obj.address) {
            m_i104->mLog.pushMsg("     COMMAND CONF INDICATION");
            is_select = (obj.se == iec104_class::SELECT);

            // if confirmed select, execute
            if (is_select && obj.pn == iec104_class::POSITIVE) {
                // if defined ASDU address on UI, use it
                // else will set to zero and use slave address (send Command will
                // substitute zero to slave address)
                iec_obj executeObj = obj;
                executeObj.ca = data.leASDUAddr.toUShort();
                executeObj.se = iec104_class::EXECUTE;
                queueProtocolCommand(executeObj);
            }
        }
}

void PointController::sendCommand(CommandData data)
{
    iec_obj obj = {};
    obj.type = static_cast<unsigned char>(data.cbCmdAsdu.left(data.cbCmdAsdu.indexOf(':')).toUInt());

    if (obj.type == iec104_class::C_RD_NA_1) {
        if (data.cb888Mode) {
            if (data.leCmdAddressLow.trimmed() == "" &&
                data.leCmdAddressMid.trimmed() == "" && 
                data.leCmdAddressHigh.trimmed() == "") 
                return;
            obj.address = data.leCmdAddressLow.toUInt() + 
                (data.leCmdAddressMid.toUInt() << 8) +
                (data.leCmdAddressHigh.toUInt() << 16);
        }
        else {
            if (data.leCmdAddress.trimmed() == "")
                return;
            obj.address = parseIoa(data.leCmdAddress);
        }
        obj.value = 0;
    }
    else
        // reset process and interrogation must have value set (qrp) or (qoi)
        if (obj.type == iec104_class::C_RP_NA_1 ||
            obj.type == iec104_class::C_IC_NA_1 ||
            obj.type == iec104_class::C_CI_NA_1) {
            if (data.leCmdValue.trimmed() == "")
                return;
            obj.address = 0;
            obj.value = data.leCmdValue.toDouble();
        }
        else
            // test command parameters if not sync command or test command (that don't
            // have parameters)
            if (obj.type != iec104_class::C_CS_NA_1 &&
                obj.type != iec104_class::C_TS_TA_1) {
                if (data.leCmdValue.trimmed() == "") 
                    return;
                unsigned int parsedAddr = 0;
                if (data.cb888Mode) {
                    if (data.leCmdAddressLow.trimmed() == "" &&
                        data.leCmdAddressMid.trimmed() == "" &&
                        data.leCmdAddressHigh.trimmed() == "") 
                        return;
                    parsedAddr = data.leCmdAddressLow.toUInt() + 
                        (data.leCmdAddressMid.toUInt() << 8) +
                        (data.leCmdAddressHigh.toUInt() << 16);
                }
                else {
                    if (data.leCmdAddress.trimmed() == "")
                        return;
                    parsedAddr = parseIoa(data.leCmdAddress);
                }
                if (parsedAddr == 0) return;

                obj.address = parsedAddr;
                obj.value = data.leCmdValue.toDouble();
            }

    obj.ca = data.leASDUAddr.toUShort();
    queueProtocolCall([ca = obj.ca](QIec104* worker) {
        worker->setSecondaryASDUAddress(ca);
        });

    QDateTime current = QDateTime::currentDateTime();

    switch (obj.type) {
    case iec104_class::C_IC_NA_1: // Interrogation
        queueProtocolCall([group = data.leCmdValue.toInt()](QIec104* worker) {
            worker->solicitInterrogation(static_cast<char>(group));
            });
        return;
    case iec104_class::C_SC_NA_1:
    case iec104_class::C_SC_TA_1:
        obj.scs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_DC_NA_1:
    case iec104_class::C_DC_TA_1:
        obj.dcs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_RC_NA_1:
    case iec104_class::C_RC_TA_1:
        obj.rcs = static_cast<unsigned char>(data.leCmdValue.toUInt());
        break;
    case iec104_class::C_SE_NA_1:
    case iec104_class::C_SE_TA_1:
        obj.value = data.leCmdValue.toInt();
        break;
    case iec104_class::C_SE_NB_1:
    case iec104_class::C_SE_TB_1:
        obj.value = data.leCmdValue.toInt();
        break;
    case iec104_class::C_SE_NC_1:
    case iec104_class::C_SE_TC_1:
        obj.value = data.leCmdValue.toDouble();
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
        obj.value = data.leCmdValue.toInt();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NB_1:
        obj.value = data.leCmdValue.toInt();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_ME_NC_1:
        obj.value = data.leCmdValue.toDouble();
        obj.kpa = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        obj.lpc = static_cast<unsigned char>(data.cbSBO);
        obj.pop = 0;
        break;
    case iec104_class::P_AC_NA_1:
        obj.value = data.leCmdValue.toInt();
        obj.qpa = data.leCmdValue.toInt();
        break;
    case iec104_class::C_CI_NA_1:
        obj.value = data.leCmdValue.toInt();
        obj.frz = static_cast<unsigned char>(
            data.cbCmdDuration.left(1).toUInt());
        break;
    }
    obj.qu = static_cast<unsigned char>(
        data.cbCmdDuration.left(1).toUInt());
    obj.se = static_cast<unsigned char>(data.cbSBO);

    queueProtocolCommand(obj);

    LastCommandAddress = obj.address;
}

void PointController::fmtCP56Time(char* buf, const cp56time2a* timetag)
{
    if (timetag->month == 0 || timetag->mday == 0)
        return;
    sprintf(buf, "Field: %02d/%02d/%02d %02d:%02d:%02d.%03d %s %s", timetag->year,
        timetag->month, timetag->mday, timetag->hour, timetag->min,
        timetag->msec / 1000, timetag->msec % 1000,
        timetag->iv ? "iv" : "ok",
        timetag->su ? "su" : "");
}

void PointController::queueProtocolCommand(const iec_obj& obj)
{
    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104, command = obj]() mutable { worker->sendCommand(&command); },
        Qt::QueuedConnection);
}

void PointController::queueProtocolCall(const std::function<void(QIec104*)>& fn)
{
    QMetaObject::invokeMethod(
        m_i104,
        [worker = m_i104, fn]() { fn(worker); },
        Qt::QueuedConnection);
}
