#include "checkedfileslist.hpp"
#include "ui_checkedfileslist.h"

#include <qstringlistmodel.h>

class CheckableStringListModel : public QStringListModel
{
    std::vector<bool> m_checks;
public:
    CheckableStringListModel(const QStringList& lst, QObject* parent = nullptr)
        : QStringListModel(lst, parent)
        , m_checks(lst.size(), true)
    {}
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override {
        return Qt::ItemFlags((QStringListModel::flags(index) | Qt::ItemIsUserCheckable) ^ Qt::ItemIsEditable);
    }
    virtual QVariant data(const QModelIndex &index, int role) const override {
        if (role == Qt::CheckStateRole && index.isValid() && !index.parent().isValid() && index.column() == 0) {
            return m_checks.at(index.row()) ? Qt::Checked : Qt::Unchecked;
        }
        return QStringListModel::data(index, role);
    }
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override {
        if (role == Qt::CheckStateRole && index.isValid() && !index.parent().isValid() && index.column() == 0) {
            m_checks[index.row()] = (value.value<Qt::CheckState>() == Qt::Checked);
            emit dataChanged(index, index, QVector<int>() << role);
            return true;
        }
        return false;
    }
};

CheckedFilesList::CheckedFilesList(const QStringList& lst, QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui_CheckedFilesList)
{
    m_ui->setupUi(this);

    CheckableStringListModel* m = new CheckableStringListModel(lst, m_ui->listView);

    m_ui->listView->setModel(m);
    m_ui->listView->show();
    m_ui->listView->raise();
}

CheckedFilesList::~CheckedFilesList() {
    delete m_ui;
}

void CheckedFilesList::accept()
{
    QStringList lst;
    QAbstractItemModel* m = m_ui->listView->model();
    for (int i = 0; i < m->rowCount(); i++) {
        if (m->index(i, 0).data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked) {
            lst << m->index(i, 0).data(Qt::DisplayRole).toString();
        }
    }
    emit acceptedList(lst);
    QDialog::accept();
}

