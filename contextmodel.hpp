#pragma once

#include <qabstractitemmodel.h>

namespace gbp {
    class Context;
} //namespace gbp

class ContextModel : public QAbstractItemModel
{
    Q_OBJECT
    gbp::Context* m_context;
public:
    explicit ContextModel(gbp::Context* c = nullptr, QObject* parent = nullptr);

    gbp::Context* contextForIndex(const QModelIndex& index) const;
    void setContext(gbp::Context* context);
    inline gbp::Context* context() const { return m_context; }

    virtual QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;
    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};
