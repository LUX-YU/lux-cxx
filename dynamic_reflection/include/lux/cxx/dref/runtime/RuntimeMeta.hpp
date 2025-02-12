#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstddef>

#include <lux/cxx/lan_model/declaration.hpp>
#include <lux/cxx/lan_model/type.hpp>

namespace lux::cxx::dref::runtime
{

    //------------------------------------------------------------
    // 1) FieldMeta
    //------------------------------------------------------------
    class FieldMeta
    {
    public:
        // 构造时传入 lan_model::FieldDeclaration* 
        FieldMeta(const ::lux::cxx::lan_model::FieldDeclaration* declPtr)
            : m_decl(declPtr)
        {
        }

        // 读写字段值 (最简单的做法：通过 offset + memcpy)
        void setValue(void* obj, const void* src, std::size_t bytes) const
        {
            if (!m_decl) return;
            char* base = static_cast<char*>(obj);
            std::memcpy(base + m_decl->offset, src, bytes);
        }
        void getValue(const void* obj, void* dst, std::size_t bytes) const
        {
            if (!m_decl) return;
            const char* base = static_cast<const char*>(obj);
            std::memcpy(dst, base + m_decl->offset, bytes);
        }

        // 其它辅助函数
        std::string name() const {
            return m_decl ? m_decl->name : "";
        }
        const ::lux::cxx::lan_model::FieldDeclaration* declaration() const {
            return m_decl;
        }

    private:
        const ::lux::cxx::lan_model::FieldDeclaration* m_decl = nullptr;
    };


    //------------------------------------------------------------
    // 2) MethodMeta
    //------------------------------------------------------------
    class MethodMeta
    {
    public:
        // 通用的“桥接函数”签名
        using InvokerFn = void(*)(void* obj, void** args, void* retVal);

        MethodMeta(const ::lux::cxx::lan_model::MemberFunctionDeclaration* declPtr,
            InvokerFn fn)
            : m_decl(declPtr)
            , m_invoker(fn)
        {
        }

        // 运行时调用 
        void invoke(void* obj, void** args, void* retVal) const
        {
            if (m_invoker) {
                m_invoker(obj, args, retVal);
            }
        }

        std::string name() const {
            return m_decl ? m_decl->name : "";
        }
        const ::lux::cxx::lan_model::MemberFunctionDeclaration* declaration() const {
            return m_decl;
        }
        InvokerFn invoker() const {
            return m_invoker;
        }

    private:
        const ::lux::cxx::lan_model::MemberFunctionDeclaration* m_decl = nullptr;
        InvokerFn m_invoker = nullptr;
    };


    //------------------------------------------------------------
    // 3) ClassMeta
    //------------------------------------------------------------
    class ClassMeta
    {
    public:
        // 构造时带一个 lan_model::ClassDeclaration 指针 
        ClassMeta(const ::lux::cxx::lan_model::ClassDeclaration* declPtr)
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
        const ::lux::cxx::lan_model::ClassDeclaration* declaration() const {
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
        const ::lux::cxx::lan_model::ClassDeclaration* m_decl = nullptr;
    };


    //------------------------------------------------------------
    // 4) MetaRegistry
    //------------------------------------------------------------
    class MetaRegistry
    {
    public:
        // 注册类 (ClassMeta)
        void registerClass(ClassMeta* cls)
        {
            if (!cls || !cls->declaration()) {
                return;
            }
            auto fqName = cls->fullQualifiedName();
            m_classMap[fqName] = cls;
        }

        // 查询 
        ClassMeta* queryClass(const std::string& fqName) const
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
