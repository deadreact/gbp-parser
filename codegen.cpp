#include "codegen.hpp"
#include "context.hpp"
#include "contextmodel.hpp"

#include <qdebug.h>
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

    QString genMemberName(const QString& classname, const QStringList& memberNames) {
        QStringList specMethods;

        for (int i = 0; i < memberNames.size(); i++) {
            specMethods << QString("template <> const char* %1::member_name<%2>() { return \"%3\"; }").arg(classname).arg(i).arg(memberNames.at(i));
        }

        return specMethods.join("\n");
    }

    QString genGetMember(const QString& classname, const QStringList& memberNames) {
        QStringList specMethods;

        for (int i = 0; i < memberNames.size(); i++) {
            specMethods << QString("template <>       typename std::tuple_element<%2, %1::types_as_tuple>::type& %1::get_member<%2>()       { return %3; }").arg(classname).arg(i).arg(memberNames.at(i));
            specMethods << QString("template <> const typename std::tuple_element<%2, %1::types_as_tuple>::type& %1::get_member<%2>() const { return %3; }").arg(classname).arg(i).arg(memberNames.at(i));
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

        QString cmpFuncSign = QString("template <typename F, typename T> inline callCmpFunc(const char* memberName, T& mem1, const T& mem2, F f) {\nf(memberName, mem1, mem2);}\n");
        cmpFuncSign += QString("template <typename F, typename T> inline callCmpFunc(const char* memberName, const T& mem1, const T& mem2, F f) const {\nf(memberName, mem1, mem2);}\n");

//        for (const QString& memberName: memberNames) {
//            cmpBlocks << QString("if (%0 != obj.%0) { f(\"%0\", %0, obj.%0); result = true; }").arg(memberName);
//        }
        for (int i = 0; i < memberNames.size(); i++) {
            cmpBlocks << QString("if (get_member<%0>() != obj.get_member<%0>()) { f(member_name<%0>(), get_member<%0>(), obj.get_member<%0>()); result = true; }").arg(i);
        }

        QString decl/* = cmpFuncSign*/;
        decl += QString("template <typename F> inline bool compare(const %0& obj, F&& f) const {\nbool result = false;\n%1\nreturn result;\n}\n"
                       "template <typename F> inline bool compare(const %0& obj, F&& f) {\nbool result = false;\n%1\nreturn result;\n}\n").arg(classname).arg(cmpBlocks.join("\n"));
        return decl;
    }

    QString genSerialize(const QStringList& memberNames) {
        return QString("\ntemplate<typename Archive>\nvoid serialize(Archive &ar) { ar & %1; }\n").arg(memberNames.join(" & "));
    }
    Code genOstreamOp(const QString& classname, const QStringList& memberNames) {
        QStringList membersToOs;

        for (const QString& memberName: memberNames) {
            membersToOs << QString("\"%0: \" << obj.%0").arg(memberName);
        }

        Code code;
        code.decl = QString("std::ostream& operator<<(std::ostream& os, const %0& obj);\n").arg(classname);
        code.impl = QString("std::ostream& operator<<(std::ostream& os, const %0& obj) {\n"
                            "    os << \"{\" << %1 << \"}\";\n"
                            "    return os;\n"
                            "}\n").arg(classname).arg(membersToOs.join(" << \", \" << "));
        return code;
    }

    Code genOstreamOpEnum(const QString& classname, const QStringList& memberNames) {
//        QStringList membersToOs;
        QStringList membersToEnumCast;

        for (const QString& memberName: memberNames) {
//            membersToOs << QString("case %0::%1: os << \"%1\"; break;").arg(classname).arg(memberName);
            membersToEnumCast << QString("case %0::%1: return is_full_name ? \"%0::%1\" : \"%1\";").arg(classname).arg(memberName);
        }

        Code code;
        code.decl = QString("const char* enum_cast(%0 e, bool is_full_name = false);\n").arg(classname);
        code.decl += QString("std::ostream& operator<<(std::ostream& os, %0 e);\n").arg(classname);
        code.impl = QString("const char* enum_cast(%0 e, bool is_full_name) {\n"
                            "    switch (e) {\n"
                            "    %1\n"
                            "    }\n"
                            "    return \"\";\n"
                            "}\n").arg(classname).arg(membersToEnumCast.join("\n"));
        code.impl += QString("std::ostream& operator<<(std::ostream& os, %0 e) {\n"
                            "    os << enum_cast(e);\n"
                            "    return os;\n"
                            "}\n").arg(classname);
        return code;
    }

} //namespace

constexpr static const char* codeTmpGuardsAdditional = "#ifdef GBP_DECLARE_TYPE_GEN_ADDITIONALS\n%0\n#endif //GBP_DECLARE_TYPE_GEN_ADDITIONALS\n";
/**
 %0 - namespace name
 %1 - content
 */
constexpr static const char* codeTmpNamespace =
R"code(namespace %0
{
%1
} //namespace %0)code";

/**
 %0 - struct name
 %1 - member declarations
 %2 - operators declarations
 %3 - extra code
 %4 - friend if needed
 %5 - overloads outside the class
 */
constexpr static const char* codeTmpStruct =
R"code(struct %0
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
%4%5)code";

/**
 %0 - enum name
 %1 - enum underlying type
 %2 - enum members
 %4 - friend if needed
 */
constexpr static const char* codeTmpEnumClass =
R"code(enum class %0 : %1
{
    %2
};
// related functions
%4%5)code";

/**
 %0 - enum name
 %1 - enum members
 %2 - friend if needed
 */
constexpr static const char* codeTmpSimpleEnum =
R"code(enum %0
{
    %1
};
// related functions
%2%3)code";

/**
 %0 - member name
 %1 - member type
 %2 - member initial value or just ';'
 */
constexpr static const char* codeTmpDeclMember =
R"code(%1 %0%2)code";

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
    QModelIndex m_rootIndex;
    Code m_code;

    Impl()
        : m_model(nullptr)
        , m_rootIndex()
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

    Code contextToCode(gbp::Context* context)
    {
        if (!context->hasConvertibleSymbols()) {
            return Code();
        }
        switch (context->type()) {
        case gbp::ContextType::Preproc:
            return QString("#%0").arg(context->content());
        case gbp::ContextType::Typedef:
            return QString("typedef %0").arg(context->content());
        case gbp::ContextType::Global:
        {
            QStringList decl;
            QStringList impl;
            for (gbp::Context* child: context->children()) {
                auto code = contextToCode(child);
                if (!code.decl.isEmpty()) {
                    decl << code.decl;
                }
                if (!code.impl.isEmpty()) {
                    impl << code.impl;
                }
            }
            return Code(decl.join('\n'), impl.join('\n'));
        }
        case gbp::ContextType::Namespace:
        {
            QStringList decl;
            QStringList impl;
            for (gbp::Context* child: context->children()) {
                Code code = contextToCode(child);
                decl << code.decl;
                impl << code.impl;
            }
            QString declstr = QString(codeTmpNamespace).arg(context->name()).arg(decl.join('\n'));
            QString implstr = QString(codeTmpNamespace).arg(context->name()).arg(impl.join('\n'));
            return Code(declstr, implstr);
        }
        case gbp::ContextType::Struct:
        {
            QStringList structsDecl;
            QStringList structsImpl;
            QStringList members;
            QString operators;
            QString extra;

            QStringList memberTypes;
            QStringList memberNames;

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::Member) {
                    members << contextToCode(child).decl;
                    memberTypes << contextToCode(getMemTypeContext(child)).decl;
                    memberNames << child->name();
                } else if (child->type() == gbp::ContextType::Struct || child->type() == gbp::ContextType::Enum || child->type() == gbp::ContextType::EnumClass) {
                    auto code = contextToCode(child);
                    if (!code.decl.isEmpty()) {
                        structsDecl << code.decl;
                    }
                    if (!code.impl.isEmpty()) {
                        structsImpl << code.impl;
                    }
                }
            }

            operators += genSerialize(memberNames);

            extra += QString("using types_as_tuple = std::tuple<%0>;\n").arg(memberTypes.join(", "));
            extra += genEqOperator(context->name(), memberNames);
            extra += QString("inline bool operator!=(const %0& other) const { return !operator==(other); }\n").arg(context->name());

            extra += QString("template <int N> typename std::tuple_element<N, types_as_tuple>::type& get_member();\n");
            extra += QString("template <int N> const typename std::tuple_element<N, types_as_tuple>::type& get_member() const;\n");

            extra += QString("template <int N> static const char* member_name();\n");

            extra += genApplyMethod(memberNames);
            extra += genCompareMethod(context->name(), memberNames);

            QString getMemberImpl = genMemberName(context->name(), memberNames);
            getMemberImpl += "\n" + genGetMember(context->name(), memberNames);
//            static QString genOutsideUpper;

//            if (!genOutsideUpper.isEmpty()) {
//                genOutside += genOutsideUpper;
//                genOutsideUpper = "";
//            }
            QString fullName = context->name();
            for (gbp::Context* currContext = context;
                 currContext->parent() && currContext->parent()->type() == gbp::ContextType::Struct;
                 currContext = currContext->parent())
            {
                getMemberImpl = getMemberImpl.replace(currContext->name() + "::", currContext->parent()->name() + "::" + currContext->name() + "::");
                fullName = currContext->parent()->name() + "::" + fullName;
            }

            Code ostreamOp = genOstreamOp(fullName, memberNames);

            return Code(formatString(codeTmpStruct
                               , context->name()
                               , structsDecl.join('\n') + "\n" + members.join('\n')
                               , operators
                               , extra
                               , context->parent()->type() == gbp::ContextType::Struct ? "friend " : ""
                               , ostreamOp.decl)
                       , QString(codeTmpGuardsAdditional).arg(getMemberImpl) + "\n" + structsImpl.join("\n") + ostreamOp.impl);
        }
        case gbp::ContextType::Enum:
        {
            QStringList members;

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::EnumItem) {
                    members << contextToCode(child).decl;
                }
            }

            QString fullName = context->name();
            for (gbp::Context* currContext = context;
                 currContext->parent() && currContext->parent()->type() == gbp::ContextType::Struct;
                 currContext = currContext->parent())
            {
                fullName = currContext->parent()->name() + "::" + fullName;
            }

            Code ostreamOp = genOstreamOpEnum(fullName, members);

            return Code(formatString(codeTmpSimpleEnum
                                   , context->name()
                                   , members.join(",\n")
                                   , context->parent()->type() == gbp::ContextType::Struct ? "friend " : ""
                                   , ostreamOp.decl)
                    , ostreamOp.impl);
        }
        case gbp::ContextType::EnumClass:
        {
            QString underlyingType("gbp_u8");
            QStringList members;

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::EnumItem) {
                    members << contextToCode(child).decl;
                } else if (child->type() == gbp::ContextType::UnderlyingType) {
                    underlyingType = child->content().toString();
                }
            }

            QString fullName = context->name();
            for (gbp::Context* currContext = context;
                 currContext->parent() && currContext->parent()->type() == gbp::ContextType::Struct;
                 currContext = currContext->parent())
            {
                fullName = currContext->parent()->name() + "::" + fullName;
            }

            Code ostreamOp = genOstreamOpEnum(fullName, members);

            return Code(formatString(codeTmpEnumClass
                                   , context->name()
                                   , underlyingType
                                   , members.join(",\n")
                                   , context->parent()->type() == gbp::ContextType::Struct ? "friend " : ""
                                   , ostreamOp.decl)
                    , ostreamOp.impl);
        }
        case gbp::ContextType::Member:
        {
            QString memType;
            QString memVal(";");

            for (gbp::Context* child: context->children()) {
                if (child->type() == gbp::ContextType::MemberType) {
                    memType = contextToCode(child).decl;
                } else if (child->type() == gbp::ContextType::MemberValue) {
                    memVal = " = " + contextToCode(child).decl + ";";
                } else {
                    Q_UNREACHABLE();
                }
            }

            return QString(codeTmpDeclMember).arg(context->name()).arg(memType).arg(memVal).simplified();
        }
        case gbp::ContextType::EnumItem:
            return context->content().toString().replace(",", "=");
        case gbp::ContextType::UnderlyingType:
        case gbp::ContextType::MemberType:
        case gbp::ContextType::MemberValue:
        case gbp::ContextType::ExtraCode:
            return context->content().toString();
        case gbp::ContextType::Comment:
            return QString("/*%0*/").arg(context->content());
        case gbp::ContextType::LineComment:
            return QString("//%0\n").arg(context->content());
        case gbp::ContextType::None:
        default:
            return Code();
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

        if (m_impl->m_rootIndex.isValid() && m_impl->m_rootIndex.model() != m) {
            m_impl->m_rootIndex = QModelIndex();
        }

        generateCode();
    }
}

ContextModel *CodeGen::model() const {
    return m_impl->m_model;
}

void CodeGen::setRootIndex(const QModelIndex &index)
{
    if (m_impl->m_rootIndex != index) {
        m_impl->m_rootIndex = index;
        generateCode();
    }
}

QModelIndex CodeGen::rootIndex() const {
    return m_impl->m_rootIndex;
}

const Code& CodeGen::code() const {
    return m_impl->m_code;
}


void CodeGen::generateCode()
{
    Code newCode;

    if (m_impl->m_model)
    {
        if (rootIndex().isValid()) {
            if (gbp::Context* context = m_impl->m_model->contextForIndex(rootIndex())) {
                newCode = m_impl->contextToCode(context);
            }
        } else if (gbp::Context* context = m_impl->m_model->context()) {
            newCode = m_impl->contextToCode(context);
        }
    }

    if (newCode.decl != m_impl->m_code.decl)
    {
        m_impl->m_code.decl = newCode.decl;
        emit declCodeChanged(newCode.decl);
    }
    if (newCode.impl != m_impl->m_code.impl)
    {
        m_impl->m_code.impl = newCode.impl;
        emit implCodeChanged(newCode.impl);
    }
}
