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
    Code(const QString& decl, const QString& impl)
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

    // кэп: для .h
    const QString& declCode() const;
    // кэп: для .cpp
    const QString& implCode() const;
private slots:
    void generateCode();
};
