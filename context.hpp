#pragma once
#include <QVector>
#include <qstring.h>
#include <vector>

namespace gbp
{
    class Context;

    enum class ContextType {
        None,
        Comment,
        LineComment,
        Struct,
        Member,
        MemberType,
        MemberValue,
        Enum,
        EnumClass,
        UnderlyingType,
        EnumItem,
        Namespace,
        Global,
        ExtraCode,
        Preproc,
        Typedef
    };


    ContextType getStartContext(QStringRef ref, Context* currentContext);

    inline bool isFinished(QStringRef ref, ContextType current)
    {
        switch (current) {
        case ContextType::Comment:
            return ref.endsWith("*/");
        case ContextType::LineComment:
        case ContextType::Preproc:
        case ContextType::Typedef:
            return ref.endsWith("\n");
        case ContextType::UnderlyingType:
            return ref.endsWith(",");
        case ContextType::Namespace:
            return ref.endsWith("}");
        case ContextType::Global:
            return ref.endsWith("\0");
//        case ContextType::ExtraCode:
//            return QStringRef(ref.string(), ref.position(), ref.size() + 1).endsWith(')');
        default:
            return ref.endsWith(")");
        }
    }

    class Context
    {
        const ContextType m_type;
        QString m_name;
        Context* m_parent;
        QVector<Context*> m_children;
        Context* m_currentChild;

//        QStringRef m_contents;
        const int m_pos;
        int m_len;
    protected:
        explicit Context(ContextType type, const QString& name, Context* parent = nullptr);

        Context* addChild(Context* context);

        virtual void onLenghtIncreased();

        Context* currentChildContext() const;
        void setCurrentChildContext(Context* context);
    public:
        virtual ~Context();

        inline QString name() const { return m_name; }
        inline ContextType type() const { return m_type; }

        inline QString typeString() const {
            switch (type()) {
            case ContextType::None:           return "None";
            case ContextType::Comment:        return "Comment";
            case ContextType::LineComment:    return "LineComment";
            case ContextType::Struct:         return "Struct";
            case ContextType::Member:         return "Member";
            case ContextType::MemberType:     return "MemberType";
            case ContextType::MemberValue:    return "MemberValue";
            case ContextType::Enum:           return "Enum";
            case ContextType::EnumClass:      return "EnumClass";
            case ContextType::UnderlyingType: return "UnderlyingType";
            case ContextType::EnumItem:       return "EnumItem";
            case ContextType::Namespace:      return "Namespace";
            case ContextType::Global:         return "Global";
            case ContextType::ExtraCode:      return "ExtraCode";
            case ContextType::Preproc:        return "Preproc";
            case ContextType::Typedef:        return "Typedef";
            default:
                return QString();
            }
        }

        void forward();

        virtual const QString* source() const;
        QStringRef content() const;

        template <typename T>
        Context* createChild(const QString& name) {
            return new T(name, this);
        }

        template <typename T>
        Context* addChild(const QString& name) {
            return addChild(createChild<T>(name));
        }
        bool removeChild(Context* context);
        void resetCurrentChildContext();

        Context* parent() const;
        const QVector<Context*>& children() const;
    };

    class GlobalContext : public Context
    {
        const QString* const m_source;
    public:
        GlobalContext(const QString* source);

        virtual const QString* source() const override;
    };

} //namespace gbp
