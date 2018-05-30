#include "page.h"
#include "ui_page.h"
#include "codegen.hpp"
#include "contextmodel.hpp"
#include "gbpparser.hpp"

#include <QLineEdit>
#include <QFileDialog>
#include <qdebug.h>

void TreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);
    emit currentChanged(current);
}

struct Page::Impl : Ui_page
{
    QString m_filepath;
    gbp::Parser m_parser;
    ContextModel* m_model;
    CodeGen* m_codegen;
    CodeGen* m_codegenFragment;
    static const QStringList sm_knownFiles;

    Impl()
        : Ui_page()
        , m_filepath()
        , m_parser()
        , m_model(nullptr)
        , m_codegen(new CodeGen)
        , m_codegenFragment(new CodeGen)
    {}
    ~Impl()
    {
        delete m_codegenFragment;
        delete m_codegen;
    }

    void loadFile(const QString& path)
    {
        if (!path.isEmpty())
        {
            m_model->setContext(nullptr);
            m_parser.setPath(path);
            m_model->setContext(m_parser.globalContext());
        }
    }
};

const QStringList Page::Impl::sm_knownFiles = QStringList() << "declare_type/decorators.hpp"
                                                            << "declare_type/list.hpp"
                                                            << "declare_type/map.hpp"
                                                            << "declare_type/pair.hpp"
                                                            << "declare_type/quoting.hpp"
                                                            << "declare_type/set.hpp"
                                                            << "declare_type/tuple.hpp"
                                                            << "declare_type/unordered_map.hpp"
                                                            << "declare_type/unordered_multimap.hpp"
                                                            << "declare_type/unordered_set.hpp"
                                                            << "declare_type/vector.hpp"
                                                            << "tools/cards.hpp"
                                                            << "tools/datetimes.hpp"
                                                            << "tools/filesystem.hpp"
                                                            << "tools/random.hpp"
                                                            << "tools/sha1.h"
                                                            << "tools/syslog_message.h"
                                                            << "tools/version.hpp"
                                                            << "tools/version.cpp";

Page::Page(QWidget *parent)
    : QWidget(parent)
    , m_impl(new Impl)
{

}

Page::~Page()
{
    delete m_impl;
}

void Page::init(const QString &filepath)
{
    m_impl->setupUi(this);
    m_impl->m_filepath = filepath;

    m_impl->m_model = new ContextModel(m_impl->m_parser.globalContext(), this);
    m_impl->treeView->setModel(m_impl->m_model);

    static const QRegularExpression re("/api/(.+)");
    QString localPath = re.match(filepath).captured(1);
//    qDebug() << filepath << localPath;

    if (m_impl->sm_knownFiles.contains(localPath)) {
        connect(m_impl->codeBrowser, &QTextBrowser::textChanged, this, [this]{ m_impl->codegenBrowser_decl->setPlainText(m_impl->codeBrowser->toPlainText()); });
    } else {
        connect(m_impl->m_codegen, &CodeGen::declCodeChanged, m_impl->codegenBrowser_decl, &QTextBrowser::setPlainText);
        connect(m_impl->m_codegen, &CodeGen::implCodeChanged, m_impl->codegenBrowser_impl, &QTextBrowser::setPlainText);
        connect(m_impl->m_codegenFragment, &CodeGen::declCodeChanged, m_impl->codegenBrowser_fragment_decl, &QTextBrowser::setPlainText);
        connect(m_impl->m_codegenFragment, &CodeGen::implCodeChanged, m_impl->codegenBrowser_fragment_impl, &QTextBrowser::setPlainText);
        m_impl->m_codegen->setModel(m_impl->m_model);
        m_impl->m_codegenFragment->setModel(m_impl->m_model);
    }



    if (gbp::Context* c = m_impl->m_parser.globalContext()) {
        m_impl->codeBrowser->setText(c->content().toString());
    }

    m_impl->codeBrowser->hide();
    connect(m_impl->codeBrowser, &QTextBrowser::textChanged, this, [this]{
        m_impl->codeBrowser->setHidden(m_impl->codeBrowser->toPlainText().isEmpty());
    });

    connect(m_impl->m_model, &ContextModel::modelReset, this, [this]{
        if (gbp::Context* c = m_impl->m_parser.globalContext()) {
            m_impl->codeBrowser->setText(c->content().toString());
        } else {
            m_impl->codeBrowser->clear();
        }
    });

    loadFile(filepath);
}

void Page::loadFile(const QString &filepath)
{
    if (!filepath.isEmpty()) {
        m_impl->loadFile(filepath);
    }
}

QToolBar *Page::toolbar() const {
    return m_impl->toolbar;
}

QString Page::filepath() const {
    return m_impl->m_filepath;
}

QString Page::declCode() const {
    return m_impl->codegenBrowser_decl->toPlainText();
}

QString Page::implCode() const {
    return m_impl->codegenBrowser_impl->toPlainText();
}

void Page::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_impl->retranslateUi(this);
        break;
    default:
        break;
    }
}

void Page::on_treeView_currentChanged(const QModelIndex &index)
{
    m_impl->m_codegenFragment->setRootIndex(index);
    if (gbp::Context* c = m_impl->m_model->contextForIndex(index)) {
//            m_impl->textBrowser->setFocus();
            QTextCursor txtCursor = m_impl->codeBrowser->textCursor();
//                int prevPos = txtCursor.position();
            txtCursor.setPosition(c->content().position());
            txtCursor.setPosition(c->content().position() + c->content().size(), QTextCursor::KeepAnchor);
            m_impl->codeBrowser->setTextCursor(txtCursor);
        ;
    }
}


