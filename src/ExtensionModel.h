#pragma once
#include "Extensions.h"
#include <QAbstractTableModel>
#include <QHash>
#include <QPixmap>

class ExtensionModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColExt, ColDesc, ColBytes, ColPercent, ColFiles, ColCount };
    explicit ExtensionModel(ExtensionTable* table, QObject* parent = nullptr);
    void refresh();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QPixmap swatch(const QColor& c) const;
    ExtensionTable* m_table;
    QVector<int> m_rows;
    qint64 m_total = 0;
    mutable QHash<QRgb, QPixmap> m_swatches;
};
