#include "gbpparser.hpp"
#include "context.hpp"
#include <iostream>
#include <qfile.h>
#include <qtextstream.h>

namespace gbp
{
    /** ---------------- Parser ------------------ */
    Parser::Parser()
        : m_filePath("")
        , m_content(nullptr)
        , m_globalContext(nullptr)
    {}
    Parser::~Parser() {
        delete m_globalContext;
    }

    void Parser::setPath(const QString &path)
    {
        if (m_filePath != path)
        {
            m_filePath = path;
            if (!process()) {
                m_globalContext = nullptr;
            }
        }
    }

    GlobalContext *Parser::globalContext() const {
        return m_globalContext;
    }

    bool Parser::process()
    {
        QFile f(m_filePath);

        if (f.open(QIODevice::ReadOnly))
        {
            if (m_content != nullptr) {
                delete m_content;
                m_content = nullptr;
            }
            m_content = new QString(QString::fromUtf8(f.readAll()));
            f.close();

            if (m_globalContext != nullptr) {
                delete m_globalContext;
                m_globalContext = nullptr;
            }
            m_globalContext = new GlobalContext(m_content);
//            std::cout << m_content.toStdString() << std::endl;

            QTextStream stream((QString*)m_content);
            stream.setCodec("UTF-8");
            QChar buf;
            QChar nextBuf;

            stream >> nextBuf;
            while (!stream.atEnd()) {
                buf = nextBuf;
                m_globalContext->forward();
                stream >> nextBuf;
            }

            return true;
        }
        return false;
    }
} // namespace gbp
