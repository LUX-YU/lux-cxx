#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstddef>

#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>

namespace lux::cxx::dref::runtime
{
    class RuntimeMeta
    {
    public:
        virtual ~RuntimeMeta() = default;
        std::string_view name; // “描述对象”的名称，比如类名、枚举名等
    };

    //=====================================
    // 1) RecordRuntimeMeta (用于 class/struct)
    //=====================================
    class RecordRuntimeMeta : public RuntimeMeta
    {
    public:
        // 构造与析构
        using ConstructorFn = void* (*)();       // 返回指针
        using DestructorFn  = void  (*)(void*);  // 传入指针

        ConstructorFn ctor = nullptr;
        DestructorFn  dtor = nullptr;

        // 字段元信息
        std::vector<class FieldRuntimeMeta>   fields;
        // 成员方法元信息
        std::vector<class MethodRuntimeMeta*> methods;
        // 静态方法元信息
        std::vector<class MethodRuntimeMeta*> static_methods;

        // 可加：基类信息
        // std::vector<RecordRuntimeMeta*> bases;

        // 也可加：构造函数、析构函数信息（如果你要区分不同参数签名的 ctor）
        // std::vector<MethodRuntimeMeta*> constructors;
        // MethodRuntimeMeta* destructor = nullptr;
    };

    //=====================================
    // 2) FieldRuntimeMeta
    //=====================================
    class FieldRuntimeMeta
    {
    public:
        std::string_view name;
        std::ptrdiff_t   offset = 0;       // 字段相对对象首地址的偏移
        // 字段类型元信息，或者仅用字符串标识，如 "int"/"MyClass" 等
        std::string_view type_name;
        // 可加：指向真正的 “TypeRuntimeMeta*” 以做更强的类型检查
        // TypeRuntimeMeta* type_meta = nullptr;

        // 访问控制
        EVisibility visibility = EVisibility::INVALID;
        // 是否是静态字段（如果需要）
        bool is_static = false;
    };

    //=====================================
    // 3) MethodRuntimeMeta
    //=====================================
    class MethodRuntimeMeta : public RuntimeMeta
    {
    public:
        // 通用的 “桥接函数” 签名 (实例方法)
        using MethodInvokerFn = void(*)(void* obj, void** args, void* retVal);

        // 对于静态方法，可能签名少一个 obj:
        //   using StaticInvokerFn = void(*)(void** args, void* retVal);

        MethodInvokerFn invoker = nullptr;

        std::vector<std::string> param_type_names; // 每个参数的类型名（可做检查用）
        std::string return_type_name;

        bool is_virtual = false;
        bool is_const   = false;
        bool is_static  = false;

        // 也可以增加：参数个数 param_count, 
        // 以及更详细的 ParamMeta 结构
    };

    //=====================================
    // 4) EnumRuntimeMeta
    //=====================================
    class EnumRuntimeMeta : public RuntimeMeta
    {
    public:
        struct Enumerator
        {
            std::string_view name;
            long long        value;  // 也可以用 int64_t
        };

        bool is_scoped = false;
        std::string_view underlying_type_name;
        std::vector<Enumerator> enumerators;
    };

    //=====================================
    // 5) 全局注册器
    //=====================================
    class RuntimeRegistry
    {
    public:
        static RuntimeRegistry& instance()
        {
            static RuntimeRegistry s;
            return s;
        }

        void registerRecord(RecordRuntimeMeta* meta)
        {
            record_map_[std::string(meta->name)] = meta;
        }

        RecordRuntimeMeta* findRecord(std::string_view name)
        {
            auto it = record_map_.find(std::string(name));
            if (it != record_map_.end()) {
                return it->second;
            }
            return nullptr;
        }

        // 同理：registerEnum, findEnum, ...
        void registerEnum(EnumRuntimeMeta* meta)
        {
            enum_map_[std::string(meta->name)] = meta;
        }
        EnumRuntimeMeta* findEnum(std::string_view name)
        {
            auto it = enum_map_.find(std::string(name));
            return (it != enum_map_.end()) ? it->second : nullptr;
        }

    private:
        // 存放 Record
        std::unordered_map<std::string, RecordRuntimeMeta*> record_map_;
        // 存放 Enum
        std::unordered_map<std::string, EnumRuntimeMeta*>   enum_map_;
    };

} // namespace lux::cxx::dref::runtime