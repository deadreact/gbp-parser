#pragma once

#include <qtabwidget.h>
#include <qabstractitemmodel.h>
#include <qtreeview.h>
#include <qtextbrowser.h>
#include "context.hpp"
//class Context;

class CodeBrowser : public QTextBrowser
{
    Q_OBJECT
    Q_PROPERTY(int wHint MEMBER m_wHint)
    int m_wHint;
public:
    CodeBrowser(QWidget* parent = nullptr)
        : QTextBrowser(parent)
        , m_wHint(-1)
    {}

    int wHint() const {
        if (m_wHint > 0) {
            return m_wHint;
        }
        return QTextBrowser::sizeHint().width();
    }

    virtual QSize sizeHint() const override {
        return hasHeightForWidth() ? QSize(wHint(), heightForWidth(wHint())) : QSize(wHint(), QTextBrowser::sizeHint().height());
    }
};

class TreeView : public QTreeView
{
    Q_OBJECT
signals:
    void currentChanged(const QModelIndex &current);
public:
    explicit TreeView(QWidget* parent = nullptr)
        : QTreeView(parent)
    {}
protected slots:
    virtual void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
};


class TabWidget : public QTabWidget
{
    Q_OBJECT
private:
    struct Impl;
    Impl* m_impl;
public:
    explicit TabWidget(QWidget *parent = 0);
    ~TabWidget();

protected:
    void changeEvent(QEvent *e);
private slots:
    void on_treeView_currentChanged(const QModelIndex& index);
    void on_actionOpen_triggered();

};
