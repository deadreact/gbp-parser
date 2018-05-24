#include "codegen.hpp"
#include "contextmodel.hpp"
#include "gbpparser.hpp"
#include "tabwidget.h"
#include "ui_tabwidget.h"

#include <QLineEdit>
#include <QFileDialog>

const char* path = "G:\\_msys64_\\home\\Deadreact\\ultima_poker-client\\common\\api\\lobby_stat\\types.h";


void TreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);
    emit currentChanged(current);
}

struct TabWidget::Impl : Ui_TabWidget
{
    gbp::Parser m_parser;
    ContextModel* m_model;
    CodeGen* m_codegen;

    Impl()
        : Ui_TabWidget()
        , m_parser()
        , m_model(nullptr)
        , m_codegen(new CodeGen)
    {}
    ~Impl()
    {
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

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_impl(new Impl)
{
    m_impl->setupUi(this);

    m_impl->actionOpen->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));

    m_impl->m_model = new ContextModel(m_impl->m_parser.globalContext(), this);
    m_impl->treeView->setModel(m_impl->m_model);

    connect(m_impl->m_codegen, &CodeGen::codeChanged, m_impl->codegenBrowser, &QTextBrowser::setPlainText);

    m_impl->m_codegen->setModel(m_impl->m_model);

    if (gbp::Context* c = m_impl->m_parser.globalContext()) {
        m_impl->codeBrowser->setText(c->content().toString());
    }

    m_impl->codeBrowser->hide();
    connect(m_impl->codeBrowser, &QTextBrowser::textChanged, this, [this]{
        m_impl->codeBrowser->setHidden(m_impl->codeBrowser->toPlainText().isEmpty());
    });
    m_impl->codegenBrowser->hide();
    connect(m_impl->codegenBrowser, &QTextBrowser::textChanged, this, [this]{
        m_impl->codegenBrowser->setHidden(m_impl->codegenBrowser->toPlainText().isEmpty());
    });

    connect(m_impl->m_model, &ContextModel::modelReset, this, [this]{
        if (gbp::Context* c = m_impl->m_parser.globalContext()) {
            m_impl->codeBrowser->setText(c->content().toString());
        } else {
            m_impl->codeBrowser->clear();
        }
    });

    m_impl->loadFile(QString::fromLatin1(path));
}

TabWidget::~TabWidget()
{
    delete m_impl;
}

void TabWidget::changeEvent(QEvent *e)
{
    QTabWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        m_impl->retranslateUi(this);
        break;
    default:
        break;
    }
}

void TabWidget::on_treeView_currentChanged(const QModelIndex &index)
{
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

void TabWidget::on_actionOpen_triggered()
{
    QString path = QFileDialog::getOpenFileName(this, QString(), ::path);

    m_impl->loadFile(path);
}

