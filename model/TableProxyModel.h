#pragma once

#include <qsortfilterproxymodel.h>
#include <qobjectdefs.h>
#include <qobject.h>

class TableProxyModel  : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	TableProxyModel(QObject* parent = nullptr);
	~TableProxyModel();
};

