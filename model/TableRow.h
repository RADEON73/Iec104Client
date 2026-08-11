#pragma once
#include <qvector.h>
#include "TableCell.h"

class TableRow
{
public:
    TableRow() = default;

    int ca() const { return ca_; }
    void setCA(int ca) { ca_ = ca; }

    int address() const { return address_; }
    void setAddress(int address) { address_ = address; }

    TableCell& cell(int column) { return cells_[column]; }
    const TableCell& cell(int column) const { return cells_[column]; }

    int columnCount() const { return cells_.size(); }
    void setColumnCount(int count) { cells_.resize(count); }

private:
    QVector<TableCell> cells_;
    int ca_ = -1;
    int address_ = -1;
};