#include "contextmodel.hpp"
#include "context.hpp"

#include <qcolor.h>

ContextModel::ContextModel(gbp::Context *c, QObject *parent)
    : QAbstractItemModel(parent)
    , m_context(c)
{}

gbp::Context* ContextModel::contextForIndex(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    return static_cast<gbp::Context*>(index.internalPointer());
}

void ContextModel::setContext(gbp::Context *context)
{
    if (m_context != context)
    {
        beginResetModel();
        m_context = context;
        endResetModel();
    }
}

QModelIndex ContextModel::index(int row, int column, const QModelIndex &parent) const
{
    if (m_context == nullptr) {
        return QModelIndex();
    }
    if (column < 0 || column > 1) {
        return QModelIndex();
    }

    if (!parent.isValid()) {
        if (row == 0) {
            return createIndex(row, column, m_context);
        }
        return QModelIndex();
    }

    if (parent.column() != 0) {
        return QModelIndex();
    }

    gbp::Context* parentContext = static_cast<gbp::Context*>(parent.internalPointer());
    if (parentContext->children().size() > row)
    {
        gbp::Context* context = parentContext->children().at(row);

        return createIndex(row, column, context);
    }
    return QModelIndex();
}

QModelIndex ContextModel::parent(const QModelIndex &child) const
{
    if (m_context == nullptr) {
        return QModelIndex();
    }
    if (!child.isValid() || child.internalPointer() == nullptr) {
        return QModelIndex();
    }
    gbp::Context* childContext = static_cast<gbp::Context*>(child.internalPointer());
    if (childContext->parent())
    {
        int row = childContext->parent()->children().indexOf(childContext);
        return createIndex(row, 0, childContext->parent());
    }
    return QModelIndex();
}

int ContextModel::rowCount(const QModelIndex &parent) const {
    if (m_context == nullptr) {
        return 0;
    }
    if (!parent.isValid()) {
        return 1;
    }
    const gbp::Context* c = static_cast<const gbp::Context*>(parent.internalPointer());
    return c->children().size();
}

int ContextModel::columnCount(const QModelIndex &/*parent*/) const {
    return 2;
}

QVariant ContextModel::data(const QModelIndex &index, int role) const
{
    if (m_context == nullptr) {
        return QVariant();
    }
    if (!index.isValid()) {
        return QVariant();
    }

    const gbp::Context* c = static_cast<const gbp::Context*>(index.internalPointer());
    if (c == nullptr) {
        c = m_context;
    }

    if (role == Qt::ForegroundRole)
    {
        using namespace gbp;
        switch (c->type()) {
        case ContextType::Comment:
        case ContextType::LineComment:
            return QColor(0x75715e);
        case ContextType::Struct:
        case ContextType::Enum:
        case ContextType::EnumClass:
            return QColor(0xf92660);
        case ContextType::Member:
//            return QColor(0xe99720);
            return QColor(0xfafafa);
        case ContextType::MemberType:
        case ContextType::UnderlyingType:
            return QColor(0x98e22d);
        case ContextType::MemberValue:
        case ContextType::EnumItem:
            return QColor(0xe99720);
        case ContextType::Preproc:
            return QColor(0xe99720);
        case ContextType::Namespace:
            return QColor(0xf92660);
        case ContextType::Typedef:
            return QColor(0x66d9ef);
        case ContextType::Global:
        case ContextType::None:
        default:
            return QVariant();
        }
    }

    if (index.column() == 0)
    {
        switch (role) {
        case Qt::DisplayRole:
            return c->name();
        case Qt::ToolTipRole:
            return c->content().toString();
        default:
            return QVariant();
        }
    }
    else if (index.column() == 1)
    {
        switch (role) {
        case Qt::DisplayRole:
            return c->typeString();
        default:
            return QVariant();
        }
    }

    return QVariant();
}

