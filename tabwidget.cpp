#include "tabwidget.h"
#include "ui_tabwidget.h"
#include "page.h"

#include <qtoolbar.h>
#include <qfileinfo.h>
#include <qfiledialog.h>
#include <QSaveFile>
#include <qtextstream.h>
#include <qtextcodec.h>
#include <qdebug.h>

#include "checkedfileslist.hpp"

const char* path = "G:\\_msys64_\\home\\Deadreact\\ultima_poker-client\\common\\api\\lobby_stat\\types.h";

using QActionList = QList<QAction*>;

struct TabWidget::Impl : Ui_TabWidget
{
    QString m_lastPath;

    Impl()
        : Ui_TabWidget()
        , m_lastPath(path)
    {}
    ~Impl()
    {}

    void setupUi(TabWidget* w) {
        Ui_TabWidget::setupUi(w);
        actionOpen->setIcon(w->style()->standardIcon(QStyle::SP_DirIcon));
        actionGenerate->setIcon(w->style()->standardIcon(QStyle::SP_CommandLink));
        actionOpenDir->setIcon(w->style()->standardIcon(QStyle::SP_DirOpenIcon));
    }

    QStringList getFilesRecursively(const QString& dir) {
        QStringList lst;

        for (const QFileInfo& info: QDir(dir).entryInfoList()) {
            if (info.isDir() && info.fileName() != "." && info.fileName() != "..") {
                lst << getFilesRecursively(info.absolutePath() + "/" + info.fileName());
            } else if (info.isFile() && (info.suffix() == "h" || info.suffix() == "hpp")) {
                lst << info.absoluteFilePath();
            }
        }
        return lst;
    }
};

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_impl(new Impl)
{
    m_impl->setupUi(this);
    setTabText(0, QFileInfo(path).fileName());
    m_impl->tab_1->init(path);
    m_impl->tab_1->toolbar()->addActions(QActionList() << m_impl->actionOpen << m_impl->actionOpenDir << m_impl->actionGenerate);

    adjustSize();
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

bool TabWidget::openInANewTab(const QString &filepath)
{
    QFileInfo fileinfo(filepath);
    if (fileinfo.exists()) {
        Page* page = new Page;
        addTab(page, fileinfo.fileName());
        page->init(filepath);
        page->toolbar()->addActions(QActionList() << m_impl->actionOpen << m_impl->actionOpenDir << m_impl->actionGenerate);

        return true;
    }
    return false;
}


void TabWidget::on_actionOpen_triggered()
{
    QStringList pathes = QFileDialog::getOpenFileNames(this, QString(), m_impl->m_lastPath);

    for (const QString& path: pathes) {
        if (openInANewTab(path)) {
            m_impl->m_lastPath = path;
        }
    }
}

void TabWidget::on_actionOpenDir_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, QString(), m_impl->m_lastPath);
    QStringList lst = m_impl->getFilesRecursively(dir);

    if (!lst.isEmpty())
    {
        CheckedFilesList* dialog = new CheckedFilesList(lst, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(dialog, &CheckedFilesList::acceptedList, this, [this](const QStringList& lst) {
            for (const QString& path: lst) {
                if (openInANewTab(path)) {
                    m_impl->m_lastPath = path;
                }
            }
        });
        dialog->open();
    }

}

void TabWidget::on_actionGenerate_triggered()
{
    QList<Page*> pages = findChildren<Page*>();
    QStringList headers;
    QStringList sources;
    QStringList consoleCommands;
    static const QRegularExpression re("/api-gen(/.+)");
    QString rootPath;
    for (Page* page: pages) {
        if (!page->declCode().isEmpty())
        {
            QFileInfo info(page->filepath());
            QString path = info.absolutePath().replace("/api", "/api-gen");
            if (rootPath.isEmpty()) {
                rootPath = QRegularExpression(".+/api-gen").match(path).captured(0);
            }
            QDir().mkpath(path);
            QString capt = re.match(path).captured(1);

            QString newFilePath = path + "/" + info.baseName() + ".h";
            QSaveFile header(newFilePath + ".tmp");
            if (header.open(QIODevice::WriteOnly)) {
                headers << ("$$PWD" + capt + "/" + info.baseName() + ".h");
                QTextStream stream(&header);
                stream.setCodec(QTextCodec::codecForName("UTF-8"));
//                stream << ("#include \"gbp_int.hpp\"\n");
                stream << page->declCode();
                header.commit();
            }
            consoleCommands << QString("G:\\_msys64_\\mingw32\\bin\\clang-format.exe -style=WebKit %0.tmp > %0 && rm %0.tmp").arg(newFilePath).toLatin1();
            if (!page->implCode().isEmpty())
            {
                newFilePath = path + "/" + info.baseName() + ".cpp";
                QSaveFile cpp(newFilePath + ".tmp");
                if (cpp.open(QIODevice::WriteOnly)) {
                    sources << ("$$PWD" + capt + "/" + info.baseName() + ".cpp");
                    QTextStream stream(&cpp);
                    stream.setCodec(QTextCodec::codecForName("UTF-8"));
                    stream << ("#include \"" + info.baseName() + ".h\"\n");
                    stream << page->implCode();
                    cpp.commit();
                }
                consoleCommands << QString("G:\\_msys64_\\mingw32\\bin\\clang-format.exe -style=WebKit %0.tmp > %0 && rm %0.tmp").arg(newFilePath).toLatin1();
            }
        }
    }

    QSaveFile priFile(rootPath + "/api-gen.pri");
    if (priFile.open(QIODevice::WriteOnly)) {
        QTextStream stream(&priFile);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << "SOURCES += \\\n";
        stream << sources.join("\\\n");
        stream << "\n\nHEADERS += \\\n";
        stream << headers.join("\\\n");
        priFile.commit();
    }
    QSaveFile proFile(rootPath + "/api-gen.pro");
    if (proFile.open(QIODevice::WriteOnly)) {
        QTextStream stream(&proFile);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream <<
R"(TEMPLATE = lib
CONFIG += c++17
CONFIG += staticlib
TARGET = gbp-api
include($$PWD/api-gen.pri))";
        proFile.commit();
    }
    QSaveFile batStyle(rootPath + "/api-gen-stylize.bat");
    if (batStyle.open(QIODevice::WriteOnly)) {
        QTextStream stream(&batStyle);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream << consoleCommands.join("\n");
        batStyle.commit();
    }
    system(QString("cd %0 && %1").arg(rootPath).arg("api-gen-stylize.bat").toLatin1());
}
