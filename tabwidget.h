#pragma once

#include <qtabwidget.h>

class Page;

class TabWidget : public QTabWidget
{
    Q_OBJECT
private:
    struct Impl;
    Impl* m_impl;
public:
    explicit TabWidget(QWidget *parent = nullptr);
    virtual ~TabWidget() override;

protected:
    void changeEvent(QEvent *e);
    bool openInANewTab(const QString& filepath);
private slots:
    void on_actionOpen_triggered();
    void on_actionOpenDir_triggered();
    void on_actionGenerate_triggered();
};
