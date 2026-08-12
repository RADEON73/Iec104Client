#include "TableProxyModel.h"

#include <qnamespace.h>
#include <qobject.h>
#include <qsortfilterproxymodel.h>
#include "TableModel.h"

TableProxyModel::TableProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{
    setSortRole(QtSortRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setDynamicSortFilter(true);
    sort(TableModel::PointColumn::Address, Qt::AscendingOrder);
}

TableProxyModel::~TableProxyModel() = default;
