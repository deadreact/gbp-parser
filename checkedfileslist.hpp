#pragma once
#include <qdialog.h>

class Ui_CheckedFilesList;

class CheckedFilesList : public QDialog
{
    Q_OBJECT
signals:
    void acceptedList(const QStringList&);
public:
    explicit CheckedFilesList(const QStringList &lst, QWidget *parent = nullptr);
    virtual ~CheckedFilesList() override;
    virtual void accept() override;
private:
    Ui_CheckedFilesList *m_ui;
};
