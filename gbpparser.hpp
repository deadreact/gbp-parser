#pragma once
#include <QVector>
#include <qstring.h>
#include <vector>

namespace gbp
{
    class GlobalContext;

    class Parser
    {
        QString m_filePath;
        const QString* m_content;
        GlobalContext* m_globalContext;
    public:
        Parser();
        virtual ~Parser();

        void setPath(const QString& path);

        GlobalContext* globalContext() const;

        bool process();
        QStringRef content() const { return m_content; }
    };
} //namespace gbp
