#pragma once

#include <qobject.h>

class ContextModel;

struct Code {
    QString decl;
    QString impl;

    Code()
        : decl()
        , impl()
    {}
    Code(QString decl, QString impl = QString())
        : decl(decl)
        , impl(impl)
    {}

    Code(const Code& other)
        : decl(other.decl)
        , impl(other.impl)
    {}
    Code& operator=(const Code& other) {
        decl = other.decl;
        impl = other.impl;
        return *this;
    }

};

class CodeGen : public QObject
{
    Q_OBJECT
private:
    struct Impl;
    Impl* m_impl;
signals:
    void declCodeChanged(const QString&);
    void implCodeChanged(const QString&);
public:
    CodeGen(QObject *parent = nullptr);
    virtual ~CodeGen() override;

    void setModel(ContextModel* m);
    ContextModel* model() const;

    void setRootIndex(const QModelIndex& index);
    QModelIndex rootIndex() const;

    const Code& code() const;
private slots:
    void generateCode();
};
