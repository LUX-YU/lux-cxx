#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstddef>

#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>

/*
 * 使用者无法标记一个BuiltinType, 可以被标记的最基础的声明是TagDecl以及DeclaratorDecl
 * 其中TagDecl对应的是类/结构体/联合体/枚举等类型的声明
 * DeclaratorDecl对应的是变量/函数等值的声明
 * 
 * 也就是说，动态反射，反射的是用户的**声明**而不是**类型**，所以我们要存储的是声明的信息，而声明的信息包含了类型的信息
 */

namespace lux::cxx::dref::runtime
{
    struct ValueDeclRuntimeMeta;
    struct DeclarationRuntimeMeta : public
    {
		lux::cxx::dref::TagDecl* decl;
		void* (*ctor)();
		void  (*dtor)(void*);
    };

    struct RecordRuntimeMeta : public DeclarationRuntimeMeta
    {
		std::vector<ValueDeclRuntimeMeta*> fields;
		std::vector<ValueDeclRuntimeMeta*> methods;
        std::vector<ValueDeclRuntimeMeta*> static_methods;
    };

    class FunctionMeta : public ValueDeclRuntimeMeta
    {
    public:
        using InvokerFn = void(*)(void** args, void* retVal);
        // 通用的“桥接函数”签名
        InvokerFn m_invoker = nullptr;
        std::string name;
    };

    class MethodMeta : public ValueDeclRuntimeMeta
    {
    public:
        using InvokerFn = void(*)(void* obj, void** args, void* retVal);

        InvokerFn invoker = nullptr;
        bool isVirtual;
    };

    //------------------------------------------------------------
    // 3) ClassMeta
    //------------------------------------------------------------
    class ClassMeta
    {
    public:
        // 构造时带一个 lan_model::ClassDeclaration 指针 
        ClassMeta(const CXXRecordDecl* declPtr)
            : m_decl(declPtr)
        {
        }

        // 供外部把 FieldMeta/MethodMeta push_back 进来
        std::vector<FieldMeta>  fields;
        std::vector<MethodMeta> methods;

        // 可以加：constructor metas / static methods / etc.

        // 辅助函数
        std::string name() const {
            return m_decl ? m_decl->name : "";
        }

        // 可能要通过 m_decl->full_qualified_name 做“唯一标识”
        std::string fullQualifiedName() const {
            return m_decl ? m_decl->full_qualified_name : "";
        }

        const CXXRecordDecl* declaration() const {
            return m_decl;
        }

        const FieldMeta* findField(const std::string& fieldName) const {
            for (auto& f : fields) {
                if (f.name() == fieldName) {
                    return &f;
                }
            }
            return nullptr;
        }

        const MethodMeta* findMethod(const std::string& methodName) const {
            for (auto& m : methods) {
                if (m.name() == methodName) {
                    return &m;
                }
            }
            return nullptr;
        }

    private:
        const CXXRecordDecl* m_decl = nullptr;
    };


    //------------------------------------------------------------
    // 4) MetaRegistry
    //------------------------------------------------------------
    class MetaRegistry
    {
    public:
        // 注册类型 (TypeMeta)
        // 这个方法对应TypeDecl以及其子类的声明的原函数
        // 目前包含：
        // TypeDecl
        //    └── TagDecl
        //         ├── EnumDecl
        //         └── RecordDecl
        //              └── CXXRecordDecl
        void registerTypeMeta(ClassMeta* cls)
        {
            if (!cls || !cls->declaration()) {
                return;
            }
            auto fqName = cls->fullQualifiedName();
            m_classMap[fqName] = cls;
        }

        // 注册各种值类型
		// 对应ValueDecl以及其子类的声明的原函数
        // 目前包含:
        // ValueDecl
        //   └── DeclaratorDecl
        //          ├── FieldDecl
        //          ├── FunctionDecl
        //          │    └── CXXMethodDecl
        //          │         ├── CXXConstructorDecl
        //          │         ├── CXXConversionDecl
        //          │         └── CXXDestructorDecl
        //          └── VarDecl
        //               ├── ImplicitParamDecl
        //               ├── NonTypeTemplateParmDecl
        //               └── ParmVarDecl
        void registerValueMeta()
        {

        }

        // 查询 
        ClassMeta* queryType(const std::string& fqName) const
        {
            auto it = m_classMap.find(fqName);
            if (it != m_classMap.end()) {
                return it->second;
            }
            return nullptr;
        }

    private:
        std::unordered_map<std::string, ClassMeta*> m_classMap;
    };


    // 一个全局单例
    inline MetaRegistry& GlobalRegistry()
    {
        static MetaRegistry instance;
        return instance;
    }

} // namespace lux::cxx::dref::runtime
