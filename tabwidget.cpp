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

const char* path = "G:\\_msys64_\\home\\Deadreact\\ultima_poker-client\\common\\api\\gbp_int.hpp";

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
    QSet<QString> filenames;

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

            QString filename = info.baseName();

            QString newFilePath = path + "/" + filename + "." + info.suffix();
            QSaveFile header(newFilePath + ".tmp");
            if (header.open(QIODevice::WriteOnly)) {
                headers << ("$$PWD" + capt + "/" + info.baseName() + "." + info.suffix());
                QTextStream stream(&header);
                stream.setCodec(QTextCodec::codecForName("UTF-8"));
//                stream << ("#include \"gbp_int.hpp\"\n");
//                if (filename.contains("reply") || filename.contains("request")) {
//                    QString extraInclude = "#pragma once\n#include \"declare_type.h\"\n#include <api" + capt + "/" + info.baseName() + "." + info.suffix() + ">";
//                    stream << page->declCode().replace("#pragma once", extraInclude);
//                } else {
//                }
                stream << page->declCode();
                header.commit();
            }
            consoleCommands << QString("G:\\_msys64_\\mingw32\\bin\\clang-format.exe -style=WebKit %0.tmp > %0 && rm %0.tmp").arg(newFilePath).toLatin1();
            if (!page->implCode().isEmpty())
            {
                if (filenames.contains(filename))
                {
                    QString newFilename = filename + "_0";
                    for (int i = 0; filenames.contains(newFilename); i++) {
                        newFilename = filename + QString("_%0").arg(i);
                    }
                    filename = newFilename;
                }
                filenames.insert(filename);

                newFilePath = path + "/" + filename + ".cpp";
                QSaveFile cpp(newFilePath + ".tmp");
                if (cpp.open(QIODevice::WriteOnly)) {
                    sources << ("$$PWD" + capt + "/" + filename + ".cpp");
                    QTextStream stream(&cpp);
                    stream.setCodec(QTextCodec::codecForName("UTF-8"));
                    stream << ("#include \"" + info.baseName() + "." + info.suffix() + "\"\n");
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
INCLUDEPATH += $$PWD/..
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

    QSaveFile declTypeFile(rootPath + "/declare_type.h");
    if (declTypeFile.open(QIODevice::WriteOnly)) {
        QTextStream stream(&declTypeFile);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream <<
R"(
#ifndef _gbp__api__declare_type
#define _gbp__api__declare_type
#include "gbp_int.hpp"
#include <api/declare_type/decorators.hpp>
#include <api/declare_type/list.hpp>
#include <api/declare_type/map.hpp>
#include <api/declare_type/pair.hpp>
#include <api/declare_type/quoting.hpp>
#include <api/declare_type/set.hpp>
#include <api/declare_type/tuple.hpp>
#include <api/declare_type/unordered_map.hpp>
#include <api/declare_type/unordered_multimap.hpp>
#include <api/declare_type/unordered_set.hpp>
#include <api/declare_type/vector.hpp>
#define GBP_DECLARE_TYPE(...)
#define GBP_DECLARE_ENUM(...)
#define GBP_DECLARE_ENUM_SIMPLE(...)
template<typename T>
struct is_gbp_type
{
private:
    typedef char yes;
    typedef short no;

    template<typename C> static yes test(typename C::types_as_tuple*);
    template<typename C> static no  test(...);
public:
    static const bool value = sizeof(test<T>(0)) == sizeof(yes);
    typedef T type;
};
#endif)";
        declTypeFile.commit();
    }

    QSaveFile gbp_intFile(rootPath + "/gbp_int.hpp");
    if (gbp_intFile.open(QIODevice::WriteOnly)) {
        QTextStream stream(&gbp_intFile);
        stream.setCodec(QTextCodec::codecForName("UTF-8"));
        stream <<
R"(#pragma once

using gbp_i64 = long long int;
using gbp_u64 = unsigned long long int;
using gbp_i32 = int;
using gbp_u32 = unsigned int;
using gbp_i16 = short;
using gbp_u16 = unsigned short;
using gbp_i8  = signed char;
using gbp_u8  = unsigned char;)";
        gbp_intFile.commit();
    }
}
