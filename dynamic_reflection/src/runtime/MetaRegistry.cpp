#include <lux/cxx/dref/runtime/MetaRegistry.hpp>
#include <lux/cxx/algotithm/hash.hpp>

namespace lux::cxx::dref::runtime
{
	static constexpr auto fnv1a = lux::cxx::algorithm::fnv1a;

#define GENERATE_FUNDAMENTAL_META(type_name, type_hash, type_kind) \
	static FundamentalRuntimeMeta s_fundamental_meta_##type_hash{ \
		.basic_info = { \
			.name = #type_name, \
			.qualified_name = #type_name, \
			.hash = fnv1a(#type_name), \
			.kind = type_kind \
		}, \
		.object_info = { \
			.size = sizeof(type_name), \
			.alignment = alignof(type_name) \
		}, \
		.cv_qualifier = { \
			.is_const    = false, \
			.is_volatile = false \
		} \
	};\
	static FundamentalRuntimeMeta s_fundamental_meta_const_##type_hash{ \
		.basic_info = { \
			.name = "const " #type_name, \
			.qualified_name = "const " #type_name, \
			.hash = fnv1a("const " #type_name), \
			.kind = type_kind \
		}, \
		.object_info = { \
			.size = sizeof(type_name), \
			.alignment = alignof(type_name) \
		}, \
		.cv_qualifier = { \
			.is_const    = true, \
			.is_volatile = false \
		} \
	};\
	static FundamentalRuntimeMeta s_fundamental_meta_volatile##type_hash{ \
		.basic_info = { \
			.name = "volatile " #type_name, \
			.qualified_name = "volatile " #type_name, \
			.hash = fnv1a("volatile " #type_name), \
			.kind = type_kind \
		}, \
		.object_info = { \
			.size = sizeof(type_name), \
			.alignment = alignof(type_name) \
		}, \
		.cv_qualifier = { \
			.is_const    = false, \
			.is_volatile = true \
		} \
	};\
	static FundamentalRuntimeMeta s_fundamental_meta_const_volatile_##type_hash{ \
		.basic_info = { \
			.name = "const volatile " #type_name, \
			.qualified_name = "const volatile " #type_name, \
			.hash = fnv1a("const volatile " #type_name), \
			.kind = type_kind \
		}, \
		.object_info = { \
			.size = sizeof(type_name), \
			.alignment = alignof(type_name) \
		}, \
		.cv_qualifier = { \
			.is_const    = true, \
			.is_volatile = true \
		} \
	};

	static FundamentalRuntimeMeta builtin_void_meta{
		.basic_info = {
			.name = "void",
			.qualified_name = "void",
			.hash = fnv1a("void"),
			.kind = ETypeKinds::Void
		},
		.object_info = {
			.size = 0,
			.alignment = 0
		},
		.cv_qualifier = {
			.is_const = false,
			.is_volatile = false
		}
	};

    GENERATE_FUNDAMENTAL_META(std::nullptr_t,     16008755778439131648, ETypeKinds::Nullptr_t)
	GENERATE_FUNDAMENTAL_META(bool,               14785269867199075517, ETypeKinds::Bool)
	GENERATE_FUNDAMENTAL_META(char,               17483980429552467645, ETypeKinds::Char)
	GENERATE_FUNDAMENTAL_META(signed char,        15848939444642836855, ETypeKinds::SignedChar)
	GENERATE_FUNDAMENTAL_META(unsigned char,      17993809043015555046, ETypeKinds::UnsignedChar)
	GENERATE_FUNDAMENTAL_META(short,              17767075776831802709, ETypeKinds::Short)
	GENERATE_FUNDAMENTAL_META(unsigned short,     2856468399610700180,  ETypeKinds::UnsignedShort)
	GENERATE_FUNDAMENTAL_META(int,                3143511548502526014,  ETypeKinds::Int)
	GENERATE_FUNDAMENTAL_META(unsigned int,       13451932124814009803, ETypeKinds::UnsignedInt)
	GENERATE_FUNDAMENTAL_META(long,               14837330719131395891, ETypeKinds::Long)
	GENERATE_FUNDAMENTAL_META(unsigned long,      12153588569745951996, ETypeKinds::UnsignedLong)
	GENERATE_FUNDAMENTAL_META(long long,          18439726635740412007, ETypeKinds::LongLong)
	GENERATE_FUNDAMENTAL_META(unsigned long long, 14026568137649808738, ETypeKinds::UnsignedLongLong)
	GENERATE_FUNDAMENTAL_META(float,              11532138274943533413, ETypeKinds::Float)
	GENERATE_FUNDAMENTAL_META(double,             11567507311810436776, ETypeKinds::Double)

	struct StaticMetaInfos
	{
		using FundamentalMap = std::unordered_map<size_t, FundamentalRuntimeMeta*, RuntimeMetaRegistry::IdentityHash>;
		std::vector<FundamentalRuntimeMeta> fundamentals;
        FundamentalMap                      meta_map;
	};

#define FUNDAMENTAL_REGISTER_HELPER(type_hash) \
    infos.fundamentals.push_back(s_fundamental_meta_##type_hash); \
    infos.fundamentals.push_back(s_fundamental_meta_const_##type_hash); \
    infos.fundamentals.push_back(s_fundamental_meta_volatile##type_hash); \
    infos.fundamentals.push_back(s_fundamental_meta_const_volatile_##type_hash);\
    infos.meta_map[s_fundamental_meta_##type_hash.basic_info.hash] = &s_fundamental_meta_##type_hash; \
    infos.meta_map[s_fundamental_meta_const_##type_hash.basic_info.hash] = &s_fundamental_meta_const_##type_hash; \
    infos.meta_map[s_fundamental_meta_volatile##type_hash.basic_info.hash] = &s_fundamental_meta_volatile##type_hash; \
    infos.meta_map[s_fundamental_meta_const_volatile_##type_hash.basic_info.hash] = &s_fundamental_meta_const_volatile_##type_hash;


    static void registerFundamentals(StaticMetaInfos& infos)
    {
		FUNDAMENTAL_REGISTER_HELPER(16008755778439131648)
		FUNDAMENTAL_REGISTER_HELPER(14785269867199075517)
		FUNDAMENTAL_REGISTER_HELPER(17483980429552467645)
		FUNDAMENTAL_REGISTER_HELPER(15848939444642836855)
		FUNDAMENTAL_REGISTER_HELPER(17993809043015555046)
		FUNDAMENTAL_REGISTER_HELPER(17767075776831802709)
		FUNDAMENTAL_REGISTER_HELPER(2856468399610700180)
		FUNDAMENTAL_REGISTER_HELPER(3143511548502526014)
		FUNDAMENTAL_REGISTER_HELPER(13451932124814009803)
		FUNDAMENTAL_REGISTER_HELPER(14837330719131395891)
		FUNDAMENTAL_REGISTER_HELPER(12153588569745951996)
		FUNDAMENTAL_REGISTER_HELPER(18439726635740412007)
		FUNDAMENTAL_REGISTER_HELPER(14026568137649808738)
		FUNDAMENTAL_REGISTER_HELPER(11532138274943533413)
		FUNDAMENTAL_REGISTER_HELPER(11567507311810436776)
    }

	static StaticMetaInfos& GetSstaticMetaInfos()
	{
        static StaticMetaInfos infos = []()
        {
			StaticMetaInfos infos;
			registerFundamentals(infos);
            return infos;
        }();
		return infos;
	}

	RuntimeMetaRegistry::RuntimeMetaRegistry()
	{
		auto& info = GetSstaticMetaInfos();
		for (auto& meta : info.fundamentals)
		{
			fundamental_metas_.push_back(&meta);
			meta_map_[meta.basic_info.hash] = &meta;
		}
	}

    FundamentalRuntimeMeta* RuntimeMetaRegistry::fetchFundamental(size_t index)
    {
        auto& info = GetSstaticMetaInfos();
		if (index < info.fundamentals.size())
		{
			return &info.fundamentals[index];
		}
		return nullptr;
    }

    FundamentalRuntimeMeta* RuntimeMetaRegistry::fetchFundamental(std::string_view name)
    {
		auto& info = GetSstaticMetaInfos();
		auto hash = fnv1a(name);
		auto it = info.meta_map.find(hash);
        if (it != info.meta_map.end())
        {
			return it->second;
        }
		return nullptr;
    }

    void RuntimeMetaRegistry::registerMeta(FundamentalRuntimeMeta* meta) {
		if (hasMeta(meta->basic_info.hash)) {
			return;
		}
        fundamental_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        pointer_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(ReferenceRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        reference_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerToDataMemberRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        ptr_to_data_member_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(PointerToMethodRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        ptr_to_method_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(ArrayRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        array_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(FunctionRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        function_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(MethodRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        method_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(FieldRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        field_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(RecordRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        record_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    void RuntimeMetaRegistry::registerMeta(EnumRuntimeMeta* meta) {
        if (hasMeta(meta->basic_info.hash)) {
            return;
        }
        enum_metas_.push_back(meta);
        VRuntimeMetaPtr mp = meta;
        meta_map_[getHashFromMeta(meta)] = mp;
    }

    //=====================
    // Has / Find
    //=====================
    bool RuntimeMetaRegistry::hasMeta(std::string_view name) const {
        size_t h = lux::cxx::algorithm::fnv1a(name);
        return (meta_map_.find(h) != meta_map_.end());
    }

    bool RuntimeMetaRegistry::hasMeta(size_t hash) const {
        return (meta_map_.find(hash) != meta_map_.end());
    }

    const VRuntimeMetaPtr RuntimeMetaRegistry::findMeta(std::string_view name) const {
        size_t h = lux::cxx::algorithm::fnv1a(name);
        return findMeta(h);
    }

    const VRuntimeMetaPtr RuntimeMetaRegistry::findMeta(size_t hash) const {
        auto it = meta_map_.find(hash);
        if (it != meta_map_.end()) {
            return it->second;
        }
        return std::monostate{};
    }

    //=====================
    // findXxx by index
    //=====================
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
