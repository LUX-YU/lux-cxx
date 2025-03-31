#pragma once
#include "RuntimeMeta.hpp"
#include <lux/cxx/visibility.h>

namespace lux::cxx::dref::runtime
{
    using VRuntimeMetaPtr = std::variant<
        std::monostate,
        FundamentalRuntimeMeta*,
        ReferenceRuntimeMeta*,
		PointerRuntimeMeta*,
		PointerToDataMemberRuntimeMeta*,
		PointerToMethodRuntimeMeta*,
		ArrayRuntimeMeta*,
		FunctionRuntimeMeta*,
		MethodRuntimeMeta*,
		FieldRuntimeMeta*,
		RecordRuntimeMeta*,
		EnumRuntimeMeta*
    >;

    /**
     * @class RuntimeMetaRegistry
     * @brief Global registry for runtime metadata.
     *
     * This singleton-like class provides an interface for registering and retrieving metadata for
     * records (classes/structs), enums, and standalone functions. It maintains a unified map
     * (`meta_map_`) keyed by a hash (usually generated from the name or mangling) and also keeps
     * separate lists (e.g., `record_meta_list_`) for easy indexing of specific metadata types.
     */
    class LUX_CXX_PUBLIC RuntimeMetaRegistry
    {    
    public:
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
        using MetaMap = std::unordered_map<size_t, VRuntimeMetaPtr, IdentityHash>;

    public:
        RuntimeMetaRegistry();

        //=====================
        // Register functions
        //=====================
        void registerMeta(FundamentalRuntimeMeta* meta);
        void registerMeta(PointerRuntimeMeta* meta);
        void registerMeta(ReferenceRuntimeMeta* meta);
        void registerMeta(PointerToDataMemberRuntimeMeta* meta);
        void registerMeta(PointerToMethodRuntimeMeta* meta);
        void registerMeta(ArrayRuntimeMeta* meta);
        void registerMeta(FunctionRuntimeMeta* meta);
        void registerMeta(MethodRuntimeMeta* meta);
        void registerMeta(FieldRuntimeMeta* meta);
        void registerMeta(RecordRuntimeMeta* meta);
        void registerMeta(EnumRuntimeMeta* meta);

        //=====================
        // Has / Find
        //=====================
        bool hasMeta(std::string_view name) const;
        bool hasMeta(size_t hash) const;
        const VRuntimeMetaPtr findMeta(std::string_view name) const;
        const VRuntimeMetaPtr findMeta(size_t hash) const;

        //=====================
        // findXxx by index
        //=====================
        // 这些接口用于按照“注册顺序”获取具体类型的指针
		static FundamentalRuntimeMeta* fetchFundamental(size_t index);
		static FundamentalRuntimeMeta* fetchFundamental(std::string_view);

        const FundamentalRuntimeMeta* findFundamental(size_t index) const;
        const PointerRuntimeMeta* findPointer(size_t index) const;
        const ReferenceRuntimeMeta* findReference(size_t index) const;
        const PointerToDataMemberRuntimeMeta* findPointerToDataMember(size_t index) const;
        const PointerToMethodRuntimeMeta* findPointerToMethod(size_t index) const;
        const ArrayRuntimeMeta* findArray(size_t index) const;
        const FunctionRuntimeMeta* findFunction(size_t index) const;
        const MethodRuntimeMeta* findMethod(size_t index) const;
        const FieldRuntimeMeta* findField(size_t index) const;
        const RecordRuntimeMeta* findRecord(size_t index) const;
        const EnumRuntimeMeta* findEnum(size_t index) const;

        //=====================
        // Access Lists
        //=====================
        const std::vector<FundamentalRuntimeMeta*>& fundamentals() const { return fundamental_metas_; }
        const std::vector<PointerRuntimeMeta*>& pointers() const { return pointer_metas_; }
        const std::vector<ReferenceRuntimeMeta*>& references() const { return reference_metas_; }
        const std::vector<PointerToDataMemberRuntimeMeta*>& pointersToDataMembers() const { return ptr_to_data_member_metas_; }
        const std::vector<PointerToMethodRuntimeMeta*>& pointersToMethods() const { return ptr_to_method_metas_; }
        const std::vector<ArrayRuntimeMeta*>& arrays() const { return array_metas_; }
        const std::vector<FunctionRuntimeMeta*>& functions() const { return function_metas_; }
        const std::vector<MethodRuntimeMeta*>& methods() const { return method_metas_; }
        const std::vector<FieldRuntimeMeta*>& fields() const { return field_metas_; }
        const std::vector<RecordRuntimeMeta*>& records() const { return record_metas_; }
        const std::vector<EnumRuntimeMeta*>& enums() const { return enum_metas_; }

    private:
        template<typename T>
        size_t getHashFromMeta(const T* meta) const {
            // 假设 T 里都有个 meta->basic_info.hash
            // 如果你的 struct 命名不一样，需要根据实际字段改写
            return meta->basic_info.hash;
        }

        // 统一存放到 meta_map_ 里（hash -> MetaPtr）
        MetaMap meta_map_;

        // 分类型保存：注册顺序查询
        std::vector<FundamentalRuntimeMeta*>           fundamental_metas_;
        std::vector<PointerRuntimeMeta*>               pointer_metas_;
        std::vector<ReferenceRuntimeMeta*>             reference_metas_;
        std::vector<PointerToDataMemberRuntimeMeta*>   ptr_to_data_member_metas_;
        std::vector<PointerToMethodRuntimeMeta*>       ptr_to_method_metas_;
        std::vector<ArrayRuntimeMeta*>                 array_metas_;
        std::vector<FunctionRuntimeMeta*>              function_metas_;
        std::vector<MethodRuntimeMeta*>                method_metas_;
        std::vector<FieldRuntimeMeta*>                 field_metas_;
        std::vector<RecordRuntimeMeta*>                record_metas_;
        std::vector<EnumRuntimeMeta*>                  enum_metas_;
    };
}