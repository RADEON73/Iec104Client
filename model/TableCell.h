#pragma once
#include <qvariant.h>

class TableCell
{
public:
    TableCell() = default;
    explicit TableCell(const QVariant& value);

    QVariant data() const { return value_; }
    void setData(const QVariant& value) { value_ = value; }

private:
    QVariant value_;
};