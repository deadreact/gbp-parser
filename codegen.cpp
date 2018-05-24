#include "codegen.hpp"
#include "context.hpp"
#include "contextmodel.hpp"

#include <qregularexpression.h>

namespace
{
    template <typename T>
    QString formatString(QString codeTmp, const T& arg) {
        return codeTmp.arg(arg);
    }
    template <typename T, typename ...Args>
    QString formatString(QString codeTmp, const T& arg, const Args&... args) {
        return formatString(codeTmp.arg(arg), args...);
    }

    QString genEqOperator(const QString& classname, const QStringList& memberNames) {
        QStringList memCompares;

        for (const QString& memberName: memberNames) {
            memCompares << QString("%0 == other.%0").arg(memberName);
        }
        return QString("inline bool operator==(const %0& other) const { return %1; }\n").arg(classname).arg(memCompares.join(" && "));
    }

    QString genGetMember(const QString& classname, const QStringList& memberNames) {
        QStringList specMethods;

        for (int i = 0; i < memberNames.size(); i++) {
            specMethods << QString("template <> inline typename std::tuple_element<%2, %1::types_as_tuple>::type& %1::get_member<%2>() { return %3; }").arg(classname).arg(i).arg(memberNames.at(i));
            specMethods << QString("template <> inline const typename std::tuple_element<%2, %1::types_as_tuple>::type& %1::get_member<%2>() const { return %3; }").arg(classname).arg(i).arg(memberNames.at(i));
        }

        return specMethods.join("\n");
    }

    QString genApplyMethod(const QStringList& memberNames) {
        QStringList funcCalls;

        for (const QString& memberName: memberNames) {
            funcCalls << QString("f(\"%0\", %0);").arg(memberName);
        }

        return QString("\ntemplate <typename F> inline void apply(F&& f) {\n%0\n}\ntemplate <typename F> void apply(F&& f) const {\n%0\n}\n").arg(funcCalls.join("\n"));
    }
    QString genCompareMethod(const QString& classname, const QStringList& memberNames) {
        QStringList cmpBlocks;

        for (const QString& memberName: memberNames) {
            cmpBlocks << QString("if (%0 != obj.%0) { f(\"%0\", %0, obj.%0); result = true; }").arg(memberName);
        }

        return QString("template <typename F> inline bool compare(const %0& obj, F&& f) const {\nbool result = false;\n%1\nreturn result;\n}\n"
                       "template <typename F> inline bool compare(const %0& obj, F&& f) {\nbool result = false;\n%1\nreturn result;\n}\n").arg(classname).arg(cmpBlocks.join("\n"));
    }

} //namespace

/**
 %0 - namespace name
 %1 - content
 */
constexpr static const char* codeTmpNamespace =
R"code(
namespace %0
{
%1
}
)code";

/**
 %0 - struct name
 %1 - member declarations
 %2 - operators declarations
 %3 - extra code
 %4 - friend if needed
 %5 - overloads outside the class
 */
constexpr static const char* codeTmpStruct =
R"code(
struct %0
{
    // methods
    %0() = default;
    %0(const %0&) = default;
    %0& operator=(const %0&) = default;
    %0(%0&&) = default;



    // members
    %1
    // operators
    %2
#ifdef GBP_DECLARE_TYPE_GEN_ADDITIONALS
    // extra
    %3
#endif //GBP_DECLARE_TYPE_GEN_ADDITIONALS
};
// related functions
%4inline std::ostream& operator<<(std::ostream& os, const %0& arg) {
    // FIXME: ostream operator
    return os;
}
#ifdef GBP_DECLARE_TYPE_GEN_ADDITIONALS
%5
#endif //GBP_DECLARE_TYPE_GEN_ADDITIONALS
)code";

/**
 %0 - enum name
 %1 - enum underlying type
 %2 - enum members
 %4 - friend if needed
 */
constexpr static const char* codeTmpEnum =
R"code(
enum class %0 : %1
{
    %2
};
// related functions
%4inline std::ostream& operator<<(std::ostream& os, %0 arg) {
    // FIXME: ostream operator
    return os;
}
%4inline const char* enum_cast(%0 val, bool full_name = false) {
    // FIXME: implement me
    static const char* str = "NOT IMPLEMENTED!!!";
    return str;
}
)code";

/**
 %0 - member name
 %1 - member type
 %2 - member initial value or just ';'
 */
constexpr static const char* codeTmpDeclMember =
R"code(
%1 %0%2
)code";

gbp::Context* getMemTypeContext(gbp::Context* c)
{
    Q_ASSERT(c->type() == gbp::ContextType::Member);
    for (gbp::Context* child: c->children()) {
        if (child->type() == gbp::ContextType::MemberType) {
            return child;
        }
    }
    return nullptr;
}

struct CodeGen::Impl
{
    ContextModel* m_model;
    QString m_code;

    Impl()
        : m_model(nullptr)
        , m_code()
    {

    }

    void disconnectFromModel(CodeGen* owner)
    {
        if (m_model != nullptr) {
            owner->disconnect(m_model);
            m_model->disconnect(owner);
        }
    }
    void connectToModel(CodeGen* owner)
    {
        if (m_model != nullptr) {
            owner->connect(m_model, &ContextModel::modelReset, owner, &CodeGen::generateCode);
            owner->connect(m_model, &ContextModel::dataChanged, owner, &CodeGen::generateCode);
            owner->connect(m_model, &ContextModel::rowsInserted, owner, &CodeGen::generateCode);
            owner->connect(m_model, &ContextModel::rowsRemoved, owner, &CodeGen::generateCode);
            owner->connect(m_model, &ContextModel::rowsMoved, owner, &CodeGen::generateCode);
        }
    }

    QString contextToCode(gbp::Context* context, QString prefix = "")
    {
        switch (context->type()) {
        case gbp::ContextType::Global:
        {
            QStringList lst;
            for (gbp::Context* child: context->children()) {
                lst << contextToCode(child, prefix + "    ");
            }
            return prefix + lst.join('\n');
        }
        case gbp::ContextType::Namespace:
        {
            QStringList lst;
            for (gbp::Context* child: context->children()) {

                lst << contextToCode(child, prefix + "    ");
            }
            return prefix + QString(codeTmpNamespace).arg(context->name()).arg(lst.join('\n'));
        }
        case gbp::ContextType::Struct:
        {
            QStringList structs;
            QStringList members;
            QString operators;
            QString extra;

            QStringList memberTypes;
            QStringList memberNames;

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::Member) {
                    members << contextToCode(child, prefix + "    ");
                    memberTypes << contextToCode(getMemTypeContext(child));
                    memberNames << child->name();
                } else if (child->type() == gbp::ContextType::Struct || child->type() == gbp::ContextType::Enum) {
                    structs << contextToCode(child, prefix + "    ");
                }
            }
            extra += QString("using types_as_tuple = std::tuple<%0>;\n").arg(memberTypes.join(", "));
            extra += genEqOperator(context->name(), memberNames);
            extra += QString("inline bool operator!=(const %0& other) const { return !operator==(other); }\n").arg(context->name());

            extra += QString("template <int N> typename std::tuple_element<N, types_as_tuple>::type& get_member();\n");
            extra += QString("template <int N> const typename std::tuple_element<N, types_as_tuple>::type& get_member() const;\n");

            extra += genApplyMethod(memberNames);
            extra += genCompareMethod(context->name(), memberNames);

            QString genOutside = genGetMember(context->name(), memberNames);
            static QString genOutsideUpper;

            if (!genOutsideUpper.isEmpty()) {
                genOutside += genOutsideUpper;
                genOutsideUpper = "";
            }

            if (context->parent()->type() ==  gbp::ContextType::Struct) {
                genOutsideUpper = genOutside.replace(context->name() + "::", context->parent()->name() + "::" + context->name() + "::");
                genOutside = "";
            }

            return prefix + formatString(codeTmpStruct
                                       , context->name()
                                       , structs.join('\n') + "\n" + members.join('\n')
                                       , operators
                                       , extra
                                       , context->parent()->type() == gbp::ContextType::Struct ? "friend " : ""
                                       , genOutside);
        }
        case gbp::ContextType::Enum:
        {
            QString underlyingType("gbp_u8");
            QStringList members;

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::EnumValue) {
                    members << contextToCode(child, prefix + "    ");
                }
            }

            return prefix + formatString(codeTmpEnum
                                       , context->name()
                                       , underlyingType
                                       , members.join(",\n")
                                       , context->parent()->type() == gbp::ContextType::Struct ? "friend " : "");
        }
        case gbp::ContextType::Member:
        {
            QString memType;
            QString memVal(";");

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::MemberType) {
                    memType = contextToCode(child, prefix);
                } else if (child->type() == gbp::ContextType::MemberValue) {
                    memVal = " = " + contextToCode(child) + ";";
                } else {
                    Q_UNREACHABLE();
                }
            }

            return prefix + QString(codeTmpDeclMember).arg(context->name()).arg(memType).arg(memVal).simplified();
        }
        case gbp::ContextType::EnumValue:
            return context->content().toString().replace(",", "=");
        case gbp::ContextType::MemberType:
        case gbp::ContextType::MemberValue:
        case gbp::ContextType::ExtraCode:
            return context->content().toString();
        case gbp::ContextType::Comment:
            return prefix + QString("/*%0*/").arg(context->content());
        case gbp::ContextType::LineComment:
            return prefix + QString("//%0\n").arg(context->content());
        case gbp::ContextType::None:
        default:
            return QString();
        }
    }
};

CodeGen::CodeGen(QObject* parent)
    : QObject(parent)
    , m_impl(new Impl)
{}

CodeGen::~CodeGen() {
    delete m_impl;
}

void CodeGen::setModel(ContextModel *m)
{
    if (m_impl->m_model != m)
    {
        m_impl->disconnectFromModel(this);
        m_impl->m_model = m;
        m_impl->connectToModel(this);

        generateCode();
    }
}

ContextModel *CodeGen::model() const {
    return m_impl->m_model;
}

const QString &CodeGen::code() const {
    return m_impl->m_code;
}

void CodeGen::generateCode()
{
    QString newCode;

    if (m_impl->m_model)
    {
        if (gbp::Context* context = m_impl->m_model->context()) {
            newCode = m_impl->contextToCode(context);
        }
    }

    if (newCode != m_impl->m_code)
    {
        m_impl->m_code = newCode;
        emit codeChanged(newCode);
    }
}
