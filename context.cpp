#include "context.hpp"
#include <iostream>

#include <QFile>
#include <QTextStream>
#include <QTextCodec>

#include <qregularexpression.h>

namespace gbp
{
    QStringRef getNameForward(QStringRef stringRef, ContextType type)
    {
        switch (type) {
        case ContextType::Struct:
        case ContextType::DeclStruct:
        case ContextType::Member:
        case ContextType::Enum:
        case ContextType::EnumClass:
        case ContextType::EnumItem:
        case ContextType::Namespace:
        {
            int pos = stringRef.position() + stringRef.size();
            auto it = stringRef.end();

            while (!it->isLetter() && *it != '_') {
                ++it;
                pos++;
            }
            auto itBegin = it;
            if (it->isLetter() || *it == '_') {
                ++it;
                for (; it->isLetter() || it->isDigit() || *it == '_'; ++it);
            }
            return QStringRef(stringRef.string(), pos, it - itBegin);
        }
        case ContextType::UnderlyingType:
        case ContextType::MemberType:
        {
            QVector<QChar> filter({'_', ':', ' ', '*', '&', '<', '>'});
            auto it = stringRef.end();

            while (*it == '\n') ++it;
            auto itBegin = it;

            for (; it->isLetter() || it->isDigit() || filter.contains(*it); ++it);

            return QStringRef(stringRef.string(), stringRef.position() + stringRef.size() + (stringRef.end() - itBegin), it - itBegin);
        }
        case ContextType::MemberValue:
        {
            auto it = stringRef.end();
            while (*it == '\n') ++it;
            auto itBegin = it;

            for (int counter = 1; counter > 0; ++it) {
                if (*it != ')') {
                    counter--;
                } else if (*it != '(') {
                    counter++;
                }
            }

            return QStringRef(stringRef.string(), stringRef.position() + stringRef.size() + (stringRef.end() - itBegin), it - itBegin);
        }
        case ContextType::None:
        case ContextType::Comment:
        case ContextType::LineComment:
        default:
            return QStringRef(stringRef.string(), stringRef.position() + stringRef.size(), 0);
        }
    }


    Context::Context(ContextType type, const QString& name, Context* parent)
        : m_type(type)
        , m_name(name)
        , m_parent(parent)
        , m_children()
        , m_currentChild(nullptr)
        , m_pos(parent ? parent->m_len : 0)
        , m_len(0)
    {}

    Context::~Context()
    {
        for (Context* context: m_children) {
            delete context;
        }
    }

    bool Context::hasConvertibleSymbols() const
    {
        switch (type()) {
        case ContextType::Struct:
        case ContextType::DeclStruct:
        case ContextType::Member:
        case ContextType::MemberType:
        case ContextType::MemberValue:
        case ContextType::Enum:
        case ContextType::EnumClass:
        case ContextType::UnderlyingType:
        case ContextType::EnumItem:
        case ContextType::Typedef:
        case ContextType::Preproc:
            return true;
        case ContextType::None:
        case ContextType::Comment:
        case ContextType::LineComment:
        case ContextType::Namespace:
        case ContextType::Global:
        case ContextType::ExtraCode:
        default:
            {
                for (Context* child: m_children) {
                    if (child->hasConvertibleSymbols()) {
                        return true;
                    }
                }
                return false;
            }
        }
    }

    const QString *Context::source() const {
        return parent() ? parent()->source() : nullptr;
    }

    Context* Context::addChild(Context *context) {
        m_children.push_back(context);
        return context;
    }

    void Context::onLenghtIncreased()
    {
        QStringRef stringRef = content();
        if (currentChildContext()) {
            currentChildContext()->forward();
        } else {
            ContextType newContext = getStartContext(stringRef, this);

            if (newContext != ContextType::None) {
                QStringRef name = getNameForward(stringRef, newContext);
                setCurrentChildContext(new Context(newContext, name.toString(), this));
            } else if (isFinished(stringRef, m_type)) {
                if (parent())
                {
                    m_len--;
                    if (m_type == ContextType::Comment) {
                        m_len--;
                    }
                    parent()->resetCurrentChildContext();
                }
                else
                {

                }
            }
        }
    }

    Context *Context::currentChildContext() const { return m_currentChild; }

    void Context::setCurrentChildContext(Context *context) {
        m_currentChild = context;
    }
    bool Context::removeChild(Context* context) {
        for (QVector<Context*>::iterator it = m_children.begin(); it != m_children.end(); ++it) {
            if (*it == context) {
                m_children.erase(it);
                //                    delete context;
                return true;
            }
        }
        return false;
    }

    void Context::forward() {
        m_len++;
        onLenghtIncreased();
    }

    QStringRef Context::content() const {
        return parent() ? QStringRef(parent()->content().string(), parent()->content().position() + m_pos, m_len) : QStringRef(source(), m_pos, m_len);
    }

    void Context::resetCurrentChildContext() {
        if (m_currentChild != nullptr)
        {
            m_children << m_currentChild;
            m_currentChild = nullptr;
        }
    }

    Context *Context::parent() const { return m_parent; }

    const QVector<Context *> &Context::children() const { return m_children; }

    GlobalContext::GlobalContext(const QString *source)
        : Context(ContextType::Global, "global", nullptr)
        , m_source(source)
    {}

    const QString *GlobalContext::source() const {
        return m_source;
    }

    ContextType getStartContext(QStringRef ref, Context *currentContext)
    {
        ContextType current = currentContext->type();

        if (current == ContextType::Comment || current == ContextType::LineComment) {
            return ContextType::None;
        }
        if (ref.endsWith("/*")) {
            return ContextType::Comment;
        }
        if (ref.endsWith("//")) {
            return ContextType::LineComment;
        }

        switch (current) {
        case ContextType::Global:
        {
            if (ref.endsWith("#")) {
                return ContextType::Preproc;
            }
            Q_FALLTHROUGH();
        }
        case ContextType::Namespace:
        {
            if (ref.endsWith("namespace ")) {
                return ContextType::Namespace;
            }
            if (ref.endsWith("struct ") || ref.endsWith("class ")) {
                return ContextType::Struct;
            }
            Q_FALLTHROUGH();
        }
        case ContextType::Struct:
        case ContextType::DeclStruct:
        {
            if (ref.endsWith("GBP_DECLARE_TYPE(")) {
                return ContextType::DeclStruct;
            }
            if (ref.endsWith("GBP_DECLARE_ENUM(")) {
                return ContextType::EnumClass;
            }
            if (ref.endsWith("GBP_DECLARE_ENUM_SIMPLE(")) {
                return ContextType::Enum;
            }
            if (ref.endsWith("typedef")) {
                return ContextType::Typedef;
            }
        }
        default:
            break;
        }

        if (current == ContextType::EnumClass && currentContext->children().isEmpty() && ref.endsWith(",")) {
            return ContextType::UnderlyingType;
        }

        if (current == ContextType::DeclStruct)
        {
            if (ref.endsWith("(")) {
                return ContextType::Member;
            }
//            if (!currentContext->children().isEmpty() && ref.endsWith(",")) {
//                return ContextType::ExtraCode;
//            }
            return ContextType::None;
        }

        if (current == ContextType::EnumClass || current == ContextType::Enum)
        {
            if (ref.endsWith("(")) {
                return ContextType::EnumItem;
            }
            return ContextType::None;
        }

        if (current == ContextType::Member)
        {
            if (ref.endsWith("(")) {
                if (currentContext->children().empty()) {
                    return ContextType::MemberType;
                }
                return ContextType::MemberValue;
            }
            return ContextType::None;
        }

        if (current == ContextType::MemberValue || current == ContextType::ExtraCode)
        {
            if (ref.endsWith("(")) {
                return ContextType::ExtraCode;
            }
            return ContextType::None;
        }

        return ContextType::None;
    }


} // namespace gbp
