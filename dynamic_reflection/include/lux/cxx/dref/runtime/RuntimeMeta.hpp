#pragma once
/**
 * @file runtime_meta.hpp
 * @brief Runtime metadata structures and registry for dynamic reflection.
 *
 * This header defines the runtime metadata for functions, fields, methods, records (classes/structs),
 * and enums. It also implements a global registry for storing and retrieving these metadata objects
 * via a unified MetaInfo mechanism.
 *
 * You can register `RecordRuntimeMeta`, `EnumRuntimeMeta`, or `FunctionRuntimeMeta` objects
 * into the registry. Each registered metadata is associated with a unique hash, which can come from
 * either a name (for classes/enums) or a mangled symbol (for functions). The registry supports
 * looking up metadata by either string or hash, as well as enumerating all registered metadata.
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
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref::runtime
{
    /**
     * @enum ERuntimeMetaType
     * @brief Represents the kind of metadata stored in MetaInfo.
     */
    enum class ERuntimeMetaType
    {
        BUILTIN,  ///< Not currently used in this example; placeholder for built-in types.
        RECORD,   ///< A class or struct metadata.
        ENUM,     ///< An enum metadata.
        FUNCTION  ///< A standalone (free) function metadata.
    };

    /**
     * @struct MetaInfo
     * @brief A unified metadata descriptor.
     *
     * This structure holds the type of the metadata, its index in the corresponding vector,
     * and a pointer to the actual metadata object (e.g., RecordRuntimeMeta, EnumRuntimeMeta, etc.).
     */
    struct MetaInfo
    {
        /// @brief The meta type (record/enum/function/etc.).
        ERuntimeMetaType type;
        /// @brief The index in the corresponding metadata vector.
        size_t   index;
        /// @brief Pointer to the underlying metadata (RecordRuntimeMeta*, EnumRuntimeMeta*, etc.).
        void* data;

        /**
         * @brief Cast the `data` pointer to the desired metadata type.
         * @tparam T The pointer type (e.g., `RecordRuntimeMeta`).
         * @return T* Pointer to the metadata object, or nullptr if cast is invalid.
         */
        template<typename T>
        constexpr inline T* castTo()
        {
            return static_cast<T*>(data);
        }
    };

    /*
	 * @struct BuiltinTypeRuntimeMeta
	 * @brief Stores metadata for a built-in type.
     * 
	 * This structure holds the metadata for a built-in type, including its name, unique hash, size in bytes,
	 */
    struct BuiltinTypeRuntimeMeta
    {
        std::string_view name;
        size_t           hash;
		size_t 		     size;
        BuiltinType::EBuiltinKind builtin_kind;
    };

    /**
     * @struct FunctionRuntimeMeta
     * @brief Stores metadata for a standalone function.
     *
     * This structure holds the necessary information to invoke a function at runtime,
     * including its name, an invoker function pointer, parameter types, and return type.
     *
     * For reflection or dynamic calls, use `invoker` by passing arguments in `args` and
     * retrieving return values from `retVal`.
     */
    struct FunctionRuntimeMeta
    {
        /// @brief Type alias for the function invoker.
        using InvokerFn = void(*)(void** args, void* retVal);

        /// @brief Human-readable name of the function.
        std::string_view name;
        /// @brief Qualified name of the function (if available).
        std::string_view qualified_name;
        /// @brief Mangled name (unique symbol) of the function.
        std::string_view mangling;
        /// @brief Unique hash representing the function (often hashed from mangling).
        size_t           hash;
        /// @brief Pointer to the invoker function.
        InvokerFn        invoker = nullptr;
        /// @brief List of parameter type names for the function.
        std::vector<std::string> param_types;
        /// @brief Return type name of the function.
        std::string      return_type;
    };

	struct ConstructorRuntimeMeta
	{
        using InvokerFn = void* (*)(void** args);

		std::string_view name;
		std::string_view qualified_name;
        InvokerFn invoker = nullptr;
		std::vector<std::string> param_types;
		std::string      return_type;
	};

    /**
     * @struct FieldRuntimeMeta
     * @brief Stores metadata for a class or struct field.
     *
     * This structure holds the metadata for a field, including its name, offset within an object,
     * type information, access visibility, and getter/setter function pointers for dynamic access.
     */
    struct FieldRuntimeMeta
    {
        /// @brief Type alias for a field getter function pointer.
        using GetterFn = void(*)(void* obj, void* outVal);
        /// @brief Type alias for a field setter function pointer.
        using SetterFn = void(*)(void* obj, void* inVal);

        /// @brief Field name.
        std::string_view name;
        /// @brief Byte offset of the field relative to the object's base address.
        std::ptrdiff_t   offset = 0;
        /// @brief Size of the field in bytes.
        size_t           size = 0;
        /// @brief Type name of the field (e.g., "int", "MyClass").
        std::string_view type_name;
        /// @brief Pointer to field getter function, if available.
        GetterFn getter = nullptr;
        /// @brief Pointer to field setter function, if available.
        SetterFn setter = nullptr;
        /// @brief Visibility of the field (public, private, protected, or invalid).
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
        std::string_view qualified_name;
        std::string_view mangling;
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
        /// @brief Type alias for a constructor function pointer (returns a pointer to the object).
        using ConstructorFn = void* (*)(void**);
        /// @brief Type alias for a destructor function pointer (accepts a pointer to the object).
        using DestructorFn = void  (*)(void*);

        /// @brief Name of the record (class or struct).
        std::string_view name;
        /// @brief Unique hash representing the record.
        size_t hash;
        /// @brief List of constructor function pointers.
        std::vector<ConstructorRuntimeMeta> ctor;
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
            /// @brief Value of the enumerator (stored as a long long).
            long long value;
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
     * @brief Global registry for runtime metadata.
     *
     * This singleton-like class provides an interface for registering and retrieving metadata for
     * records (classes/structs), enums, and standalone functions. It maintains a unified map
     * (`meta_map_`) keyed by a hash (usually generated from the name or mangling) and also keeps
     * separate lists (e.g., `record_meta_list_`) for easy indexing of specific metadata types.
     */
    class LUX_CXX_PUBLIC RuntimeRegistry
    {
        /**
         * @struct IdentityHash
         * @brief Custom hash functor for size_t keys (identity).
         */
        struct IdentityHash {
            /**
             * @brief Computes the hash for a given key by returning it directly.
             * @param key The key to hash.
             * @return The same key, acting as its own hash.
             */
            std::size_t operator()(std::size_t key) const noexcept {
                return key;
            }
        };

        /// @brief Template alias for a metadata hash map (keyed by size_t).
        template<typename T>
        using MetaHashMap = std::unordered_map<size_t, T, IdentityHash>;

    public:
		// register all built-in types
        RuntimeRegistry();

        /**
         * @brief Registers a record metadata.
         * @param meta Pointer to the RecordRuntimeMeta to be registered.
         *
         * The hash of the record is used as the key. The record is also appended
         * to `record_meta_list_`.
         */
        void registerMeta(RecordRuntimeMeta* meta);

        /**
         * @brief Registers an enum metadata.
         * @param meta Pointer to the EnumRuntimeMeta to be registered.
         *
         * The hash of the enum is used as the key. The enum is also appended
         * to `enum_meta_list_`.
         */
        void registerMeta(EnumRuntimeMeta* meta);

        /**
         * @brief Registers a function metadata.
         * @param meta Pointer to the FunctionRuntimeMeta to be registered.
         *
         * The hash of the function (often from its mangled name) is used as the key.
         * The function is also appended to `function_meta_list_`.
         */
        void registerMeta(FunctionRuntimeMeta* meta);

        /**
         * @brief Checks if metadata exists by name or mangling.
         * @param name The name (record/enum) or mangling (function).
         * @return True if the metadata exists; false otherwise.
         */
        bool hasMeta(std::string_view name);
		bool hasMeta(size_t hash);

        /**
         * @brief Finds metadata by name or mangling.
         * @param name The name (record/enum) or mangling (function).
         * @return Pointer to the corresponding MetaInfo if found; nullptr otherwise.
         */
        const MetaInfo* findMeta(std::string_view name);

        /**
         * @brief Finds metadata by hash.
         * @param hash The unique hash associated with the metadata.
         * @return Pointer to the corresponding MetaInfo if found; nullptr otherwise.
         */
        const MetaInfo* findMeta(size_t hash);

        /**
         * @brief Retrieves record metadata by its index in the internal list.
         * @param index Index within `record_meta_list_`.
         * @return Pointer to RecordRuntimeMeta if valid; nullptr otherwise.
         */
        RecordRuntimeMeta* findRecord(size_t index);

        /**
         * @brief Retrieves enum metadata by its index in the internal list.
         * @param index Index within `enum_meta_list_`.
         * @return Pointer to EnumRuntimeMeta if valid; nullptr otherwise.
         */
        EnumRuntimeMeta* findEnum(size_t index);

        /**
         * @brief Retrieves function metadata by its index in the internal list.
         * @param index Index within `function_meta_list_`.
         * @return Pointer to FunctionRuntimeMeta if valid; nullptr otherwise.
         */
        FunctionRuntimeMeta* findFunction(size_t index);

        /// @brief Returns the list of all registered `RecordRuntimeMeta*`.
        const std::vector<RecordRuntimeMeta*>& recordMetaList()   const { return record_meta_list_; }
        /// @brief Returns the list of all registered `EnumRuntimeMeta*`.
        const std::vector<EnumRuntimeMeta*>& enumMetaList()     const { return enum_meta_list_; }
        /// @brief Returns the list of all registered `FunctionRuntimeMeta*`.
        const std::vector<FunctionRuntimeMeta*>& functionMetaList() const { return function_meta_list_; }

    private:
        void registerMeta(BuiltinTypeRuntimeMeta* meta);

        /// @brief List of pointers to all registered record metadata objects.
        std::vector<RecordRuntimeMeta*>         record_meta_list_;
        /// @brief List of pointers to all registered enum metadata objects.
        std::vector<EnumRuntimeMeta*>           enum_meta_list_;
        /// @brief List of pointers to all registered function metadata objects.
        std::vector<FunctionRuntimeMeta*>       function_meta_list_;
        std::vector<BuiltinTypeRuntimeMeta*>    builtin_meta_list_;
        /// @brief A unified map for all metadata, keyed by their unique hashes.
        MetaHashMap<MetaInfo>                   meta_map_;
    };

} // namespace lux::cxx::dref::runtime
