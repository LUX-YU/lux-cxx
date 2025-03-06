/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
    struct FunctionRuntimeMeta
    {
		using InvokerFn = void(*)(void** args, void* retVal);
        
        std::string_view name;
		InvokerFn invoker = nullptr;
		std::vector<std::string> param_types;
		std::string return_type;
    };

    struct FieldRuntimeMeta
    {
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

    struct MethodRuntimeMeta
    {
        // 通用的 “桥接函数” 签名 (实例方法)
        using MethodInvokerFn = void(*)(void* obj, void** args, void* retVal);

        std::string_view name;
        MethodInvokerFn invoker = nullptr;

        std::vector<std::string> param_types; // 每个参数的类型名（可做检查用）
        std::string return_type;

        bool is_virtual = false;
        bool is_const   = false;
        bool is_static  = false;
    };

    struct RecordRuntimeMeta
    {
        // 构造与析构
        using ConstructorFn = void* (*)(void**); // 返回指针
        using DestructorFn  = void  (*)(void*);  // 传入指针

        std::string_view name;

        std::vector<ConstructorFn> ctor{};
        DestructorFn  dtor = nullptr;

        // 字段元信息
        std::vector<FieldRuntimeMeta>    fields;
        // 成员方法元信息
        std::vector<MethodRuntimeMeta>   methods;
        // 静态方法元信息
        std::vector<FunctionRuntimeMeta> static_methods;
    };

    //=====================================
    // 4) EnumRuntimeMeta
    //=====================================
    struct EnumRuntimeMeta
    {
        struct Enumerator
        {
            std::string_view name;
            long long        value;  // 也可以用 int64_t
        };

        std::string_view name;
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