#include "codegen.hpp"
#include "context.hpp"
#include "contextmodel.hpp"

#include <qregularexpression.h>

/**
 %0 - member name
 %1 - func name
 */
constexpr static const char* codeTmpApplyFunc = "%0.apply(%1)";

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
    // extra
    %3
};
// related functions
%4std::ostream& operator<<(std::ostream& os, const %0& arg) {
    // FIXME: ostream operator
    return os;
}
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
%4std::ostream& operator<<(std::ostream& os, %0 arg) {
    // FIXME: ostream operator
    return os;
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
    {}

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

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::Member) {
                    members << contextToCode(child, prefix + "    ");
                    memberTypes << contextToCode(getMemTypeContext(child));
                } else if (child->type() == gbp::ContextType::Struct || child->type() == gbp::ContextType::Enum) {
                    structs << contextToCode(child, prefix + "    ");
                }
            }
            extra += QString("using types_as_tuple = std::tuple<%0>;").arg(memberTypes.join(", "));

            return prefix + QString(codeTmpStruct).arg(context->name())
                                                  .arg(structs.join('\n') + "\n" + members.join('\n'))
                                                  .arg(operators)
                                                  .arg(extra)
                                                  .arg(context->parent()->type() == gbp::ContextType::Struct ? "friend " : "");
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

            return prefix + QString(codeTmpEnum).arg(context->name())
                                                .arg(underlyingType)
                                                .arg(members.join(",\n"))
                                                .arg(context->parent()->type() == gbp::ContextType::Struct ? "friend " : "");
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
