#include <lux/cxx/dref/runtime/MetaRegistry.hpp>
#include <lux/cxx/algotithm/hash.hpp>

namespace lux::cxx::dref::runtime
{
    void RuntimeMetaRegistry::registerMeta(FundamentalRuntimeMeta* meta) {
		if (hasMeta(meta->basic_info.hash)) {
			return;
		}
        fundamental_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        pointer_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(ReferenceRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        reference_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerToDataMemberRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        ptr_to_data_member_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerToMethodRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        ptr_to_method_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(ArrayRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        array_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(FunctionRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        function_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(MethodRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        method_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(FieldRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        field_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(RecordRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        record_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(EnumRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        enum_metas_.push_back(meta);
        MetaPtr mp{ meta->basic_info.kind, meta };
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    //=====================
    // Has / Find
    //=====================

    /// 通过字符串 name (如 meta->basic_info.name) 判断是否已存在
    bool RuntimeMetaRegistry::hasMeta(std::string_view name) const {
        size_t h = lux::cxx::algorithm::fnv1a(name);
        return (meta_map_.find(h) != meta_map_.end());
    }

    /// 通过 hash 判断是否已存在
    bool RuntimeMetaRegistry::hasMeta(size_t hash) const {
        return (meta_map_.find(hash) != meta_map_.end());
    }

    /// 返回指向 MetaPtr 的指针，可判断其 kind，然后再强转
    const MetaPtr* RuntimeMetaRegistry::findMeta(std::string_view name) const {
        size_t h = lux::cxx::algorithm::fnv1a(name);
        return findMeta(h);
    }

    const MetaPtr* RuntimeMetaRegistry::findMeta(size_t hash) const {
        auto it = meta_map_.find(hash);
        if (it != meta_map_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    //=====================
    // findXxx by index
    //=====================
    // 这些接口用于按照“注册顺序”获取具体类型的指针
    const FundamentalRuntimeMeta* RuntimeMetaRegistry::findFundamental(size_t index) const {
        return (index < fundamental_metas_.size()) ? fundamental_metas_[index] : nullptr;
    }

    const PointerRuntimeMeta* RuntimeMetaRegistry::findPointer(size_t index) const {
        return (index < pointer_metas_.size()) ? pointer_metas_[index] : nullptr;
    }

    const ReferenceRuntimeMeta* RuntimeMetaRegistry::findReference(size_t index) const {
        return (index < reference_metas_.size()) ? reference_metas_[index] : nullptr;
    }

    const PointerToDataMemberRuntimeMeta* RuntimeMetaRegistry::findPointerToDataMember(size_t index) const {
        return (index < ptr_to_data_member_metas_.size()) ? ptr_to_data_member_metas_[index] : nullptr;
    }

    const PointerToMethodRuntimeMeta* RuntimeMetaRegistry::findPointerToMethod(size_t index) const {
        return (index < ptr_to_method_metas_.size()) ? ptr_to_method_metas_[index] : nullptr;
    }

    const ArrayRuntimeMeta* RuntimeMetaRegistry::findArray(size_t index) const {
        return (index < array_metas_.size()) ? array_metas_[index] : nullptr;
    }

    const FunctionRuntimeMeta* RuntimeMetaRegistry::findFunction(size_t index) const {
        return (index < function_metas_.size()) ? function_metas_[index] : nullptr;
    }

    const MethodRuntimeMeta* RuntimeMetaRegistry::findMethod(size_t index) const {
        return (index < method_metas_.size()) ? method_metas_[index] : nullptr;
    }

    const FieldRuntimeMeta* RuntimeMetaRegistry::findField(size_t index) const {
        return (index < field_metas_.size()) ? field_metas_[index] : nullptr;
    }

    const RecordRuntimeMeta* RuntimeMetaRegistry::findRecord(size_t index) const {
        return (index < record_metas_.size()) ? record_metas_[index] : nullptr;
    }

    const EnumRuntimeMeta* RuntimeMetaRegistry::findEnum(size_t index) const {
        return (index < enum_metas_.size()) ? enum_metas_[index] : nullptr;
    }
} // namespace lux::cxx::dref::runtime
