#pragma once
#include "RuntimeMeta.hpp"
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref
{
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
}