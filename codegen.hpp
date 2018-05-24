#pragma once

#include <qobject.h>

class ContextModel;

class CodeGen : public QObject
{
    Q_OBJECT
private:
    struct Impl;
    Impl* m_impl;
signals:
    void codeChanged(const QString&);
public:
    CodeGen(QObject *parent = nullptr);
    virtual ~CodeGen() override;

    void setModel(ContextModel* m);
    ContextModel* model() const;

    const QString& code() const;
private slots:
    void generateCode();
};
