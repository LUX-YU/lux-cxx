#pragma once
/**
 * @file runtime_meta.hpp
 * @brief Runtime metadata structures and registry for dynamic reflection.
 *
 * This header defines the runtime metadata for functions, fields, methods, records (classes/structs),
 * and enums. It also implements a global registry for storing and retrieving these metadata objects.
 *
 * @copyright
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

#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cstddef>

#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref::runtime
{
    /**
     * @struct FunctionRuntimeMeta
     * @brief Stores metadata for a standalone function.
     *
     * This structure holds the necessary information to invoke a function at runtime,
     * including its name, an invoker function pointer, parameter types, and return type.
     */
    struct FunctionRuntimeMeta
    {
        /// @brief Type alias for the function invoker.
        using InvokerFn = void(*)(void** args, void* retVal);

        /// @brief Name of the function.
        std::string_view name;
        /// @brief Pointer to the invoker function.
        InvokerFn invoker = nullptr;
        /// @brief List of parameter type names for the function.
        std::vector<std::string> param_types;
        /// @brief Return type name of the function.
        std::string return_type;
    };

    /**
     * @struct FieldRuntimeMeta
     * @brief Stores metadata for a class or struct field.
     *
     * This structure holds the metadata for a field, including its name, offset within an object,
     * type information, access visibility, and whether it is static.
     */
    struct FieldRuntimeMeta
    {
        /// @brief Name of the field.
        std::string_view name;
        /// @brief Offset of the field relative to the object's base address.
        std::ptrdiff_t offset = 0;
        /// @brief Type name of the field (e.g., "int", "MyClass").
        std::string_view type_name;
        /// @brief Visibility of the field (e.g., public, private, or protected).
        EVisibility visibility = EVisibility::INVALID;
        /// @brief Flag indicating whether the field is static.
        bool is_static = false;
    };

    /**
     * @struct MethodRuntimeMeta
     * @brief Stores metadata for a member method.
     *
     * This structure holds the runtime metadata for a class or struct member method,
     * including its name, an invoker pointer, parameter types, return type, and flags indicating
     * if the method is virtual, const, or static.
     */
    struct MethodRuntimeMeta
    {
        /// @brief Type alias for the method invoker function.
        using MethodInvokerFn = void(*)(void* obj, void** args, void* retVal);

        /// @brief Name of the method.
        std::string_view name;
        /// @brief Pointer to the method invoker.
        MethodInvokerFn invoker = nullptr;
        /// @brief List of parameter type names for the method.
        std::vector<std::string> param_types;
        /// @brief Return type name of the method.
        std::string return_type;
        /// @brief Flag indicating whether the method is virtual.
        bool is_virtual = false;
        /// @brief Flag indicating whether the method is a const method.
        bool is_const = false;
        /// @brief Flag indicating whether the method is static.
        bool is_static = false;
    };

    /**
     * @struct RecordRuntimeMeta
     * @brief Stores metadata for a record (class or struct).
     *
     * This structure contains all metadata required for dynamic reflection of a record,
     * including its name, unique hash, constructor and destructor functions, fields, member methods,
     * and static methods.
     */
    struct RecordRuntimeMeta
    {
        /// @brief Type alias for a constructor function pointer.
        using ConstructorFn = void* (*)(void**);  ///< Returns a pointer to the constructed object
        /// @brief Type alias for a destructor function pointer.
        using DestructorFn = void  (*)(void*);     ///< Accepts a pointer to the object to destruct

        /// @brief Name of the record.
        std::string_view name;
        /// @brief Unique hash representing the record.
        size_t hash;
        /// @brief List of constructor function pointers.
        std::vector<ConstructorFn> ctor{};
        /// @brief Destructor function pointer.
        DestructorFn dtor = nullptr;
        /// @brief Metadata for the fields of the record.
        std::vector<FieldRuntimeMeta> fields;
        /// @brief Metadata for the member methods of the record.
        std::vector<MethodRuntimeMeta> methods;
        /// @brief Metadata for the static methods of the record.
        std::vector<FunctionRuntimeMeta> static_methods;
    };

    /**
     * @struct EnumRuntimeMeta
     * @brief Stores metadata for an enumeration.
     *
     * This structure holds metadata for an enum, including its name, unique hash,
     * whether it is scoped (enum class), its underlying type, and a list of its enumerators.
     */
    struct EnumRuntimeMeta
    {
        /**
         * @struct Enumerator
         * @brief Represents a single enumerator within an enum.
         */
        struct Enumerator
        {
            /// @brief Name of the enumerator.
            std::string_view name;
            /// @brief Value of the enumerator.
            long long value;  ///< Typically can also be represented as int64_t.
        };

        /// @brief Name of the enum.
        std::string_view name;
        /// @brief Unique hash representing the enum.
        size_t hash;
        /// @brief Flag indicating if the enum is scoped (e.g., enum class).
        bool is_scoped = false;
        /// @brief Underlying type name of the enum (e.g., "int", "unsigned int").
        std::string_view underlying_type_name;
        /// @brief List of enumerators in the enum.
        std::vector<Enumerator> enumerators;
    };

    /**
     * @class RuntimeRegistry
     * @brief Global registry for runtime metadata of records and enums.
     *
     * This singleton class provides an interface for registering and retrieving metadata for records
     * (classes/structs) and enums. It uses unique hash values as keys in its internal maps.
     */
    class LUX_CXX_PUBLIC RuntimeRegistry
    {
        /**
         * @struct IdentityHash
         * @brief Custom hash functor for size_t keys.
         *
         * This functor performs an identity hash, simply returning the key itself.
         */
        struct IdentityHash {
            /**
             * @brief Computes the hash for a given key.
             * @param key The key to hash.
             * @return The same key, acting as its own hash.
             */
            std::size_t operator()(std::size_t key) const noexcept {
                return key;
            }
        };

        /// @brief Template alias for a metadata hash map.
        template<typename T> using MetaHashMap =
            std::unordered_map<size_t, T, IdentityHash>;
    public:
		/**
         * @brief Registers enum or record metadata.
         * @param meta Pointer to the RecordRuntimeMeta to be registered.
         */
        void registerMeta(RecordRuntimeMeta* meta);
        void registerMeta(EnumRuntimeMeta* meta);

        /**
         * @brief Finds record metadata by record name or hash.
         *
         * Note: The lookup currently converts the name to a string to search the map,
         * which uses a hash as the key. It is assumed that the record's unique hash corresponds to its name.
         *
         * @param name The name of the record.
         * @return Pointer to the corresponding RecordRuntimeMeta if found; nullptr otherwise.
         */
        RecordRuntimeMeta* findRecord(std::string_view name);
		RecordRuntimeMeta* findRecord(size_t hash);

        /**
         * @brief Finds enum metadata by enum name.
         *
         * Similar to findRecord, the lookup assumes that the enum's unique hash corresponds to its name.
         *
         * @param name The name of the enum.
         * @return Pointer to the corresponding EnumRuntimeMeta if found; nullptr otherwise.
         */
        EnumRuntimeMeta* findEnum(std::string_view name);
		EnumRuntimeMeta* findEnum(size_t hash);

    private:
        /// @brief Map storing record metadata, keyed by unique hash.
        MetaHashMap<RecordRuntimeMeta*> record_map_;
        /// @brief Map storing enum metadata, keyed by unique hash.
        MetaHashMap<EnumRuntimeMeta*> enum_map_;
    };

} // namespace lux::cxx::dref::runtime
