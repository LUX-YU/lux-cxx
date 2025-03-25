#include <lux/cxx/dref/runtime/RuntimeMeta.hpp>
#include <lux/cxx/algotithm/hash.hpp>

namespace lux::cxx::dref::runtime
{
    static std::vector<FundamentalRuntimeMeta> builtin_types{
        { "void",             lux::cxx::algorithm::fnv1a("void"),             0,                        BuiltinType::EBuiltinKind::VOID },
        { "bool",             lux::cxx::algorithm::fnv1a("bool"),             sizeof(bool),             BuiltinType::EBuiltinKind::BOOL },
        { "char",             lux::cxx::algorithm::fnv1a("char"),             sizeof(char),             BuiltinType::EBuiltinKind::CHAR_U },
        { "unsigned char",    lux::cxx::algorithm::fnv1a("unsigned char"),    sizeof(unsigned char),    BuiltinType::EBuiltinKind::UCHAR },
        { "char16_t",         lux::cxx::algorithm::fnv1a("char16_t"),         sizeof(char16_t),         BuiltinType::EBuiltinKind::CHAR16_T },
        { "char32_t",         lux::cxx::algorithm::fnv1a("char32_t"),         sizeof(char32_t),         BuiltinType::EBuiltinKind::CHAR32_T },
        { "unsigned short",   lux::cxx::algorithm::fnv1a("unsigned short"),   sizeof(unsigned short),   BuiltinType::EBuiltinKind::UNSIGNED_SHORT_INT },
        { "unsigned int",     lux::cxx::algorithm::fnv1a("unsigned int"),     sizeof(unsigned int),     BuiltinType::EBuiltinKind::UNSIGNED_INT },
        { "unsigned long",    lux::cxx::algorithm::fnv1a("unsigned long"),    sizeof(unsigned long),    BuiltinType::EBuiltinKind::UNSIGNED_LONG_INT },
        { "unsigned long long", lux::cxx::algorithm::fnv1a("unsigned long long"), sizeof(unsigned long long), BuiltinType::EBuiltinKind::UNSIGNED_LONG_LONG_INT },
        { "unsigned __int128",lux::cxx::algorithm::fnv1a("unsigned __int128"),16,                       BuiltinType::EBuiltinKind::EXTENDED_UNSIGNED },
        { "signed char",      lux::cxx::algorithm::fnv1a("signed char"),      sizeof(signed char),      BuiltinType::EBuiltinKind::SIGNED_CHAR_S },
        { "signed char",      lux::cxx::algorithm::fnv1a("signed char"),      sizeof(signed char),      BuiltinType::EBuiltinKind::SIGNED_SIGNED_CHAR },
        { "wchar_t",          lux::cxx::algorithm::fnv1a("wchar_t"),          sizeof(wchar_t),          BuiltinType::EBuiltinKind::WCHAR_T },
        { "short",            lux::cxx::algorithm::fnv1a("short"),            sizeof(short),            BuiltinType::EBuiltinKind::SHORT_INT },
        { "int",              lux::cxx::algorithm::fnv1a("int"),              sizeof(int),              BuiltinType::EBuiltinKind::INT },
        { "long",             lux::cxx::algorithm::fnv1a("long"),             sizeof(long),             BuiltinType::EBuiltinKind::LONG_INT },
        { "long long",        lux::cxx::algorithm::fnv1a("long long"),        sizeof(long long),        BuiltinType::EBuiltinKind::LONG_LONG_INT },
        { "__int128",         lux::cxx::algorithm::fnv1a("__int128"),         16,                       BuiltinType::EBuiltinKind::EXTENDED_SIGNED },
        { "float",            lux::cxx::algorithm::fnv1a("float"),            sizeof(float),            BuiltinType::EBuiltinKind::FLOAT },
        { "double",           lux::cxx::algorithm::fnv1a("double"),           sizeof(double),           BuiltinType::EBuiltinKind::DOUBLE },
        { "long double",      lux::cxx::algorithm::fnv1a("long double"),      sizeof(long double),      BuiltinType::EBuiltinKind::LONG_DOUBLE },
        { "nullptr_t",        lux::cxx::algorithm::fnv1a("nullptr_t"),        sizeof(std::nullptr_t),   BuiltinType::EBuiltinKind::NULLPTR },
        { "overload",         lux::cxx::algorithm::fnv1a("overload"),         0,                        BuiltinType::EBuiltinKind::OVERLOAD },
        { "dependent",        lux::cxx::algorithm::fnv1a("dependent"),        0,                        BuiltinType::EBuiltinKind::DEPENDENT },
        { "ObjCId",           lux::cxx::algorithm::fnv1a("ObjCId"),           sizeof(void*),            BuiltinType::EBuiltinKind::OBJC_IDENTIFIER },
        { "ObjCClass",        lux::cxx::algorithm::fnv1a("ObjCClass"),        sizeof(void*),            BuiltinType::EBuiltinKind::OBJC_CLASS },
        { "ObjCSel",          lux::cxx::algorithm::fnv1a("ObjCSel"),          sizeof(void*),            BuiltinType::EBuiltinKind::OBJC_SEL },
        { "float128",         lux::cxx::algorithm::fnv1a("float128"),         16,                       BuiltinType::EBuiltinKind::FLOAT_128 },
        { "half",             lux::cxx::algorithm::fnv1a("half"),             0,                        BuiltinType::EBuiltinKind::HALF },
        { "float16",          lux::cxx::algorithm::fnv1a("float16"),          2,                        BuiltinType::EBuiltinKind::FLOAT16 },
        { "short accum",      lux::cxx::algorithm::fnv1a("short accum"),      0,                        BuiltinType::EBuiltinKind::SHORT_ACCUM },
        { "accum",            lux::cxx::algorithm::fnv1a("accum"),            0,                        BuiltinType::EBuiltinKind::ACCUM },
        { "long accum",       lux::cxx::algorithm::fnv1a("long accum"),       0,                        BuiltinType::EBuiltinKind::LONG_ACCUM },
        { "unsigned short accum", lux::cxx::algorithm::fnv1a("unsigned short accum"), 0,                BuiltinType::EBuiltinKind::UNSIGNED_SHORT_ACCUM },
        { "unsigned accum",   lux::cxx::algorithm::fnv1a("unsigned accum"),   0,                        BuiltinType::EBuiltinKind::UNSIGNED_ACCUM },
        { "unsigned long accum", lux::cxx::algorithm::fnv1a("unsigned long accum"), 0,                  BuiltinType::EBuiltinKind::UNSIGNED_LONG_ACCUM },
        { "bfloat16",         lux::cxx::algorithm::fnv1a("bfloat16"),         0,                        BuiltinType::EBuiltinKind::BFLOAT16 },
        { "ibm128",           lux::cxx::algorithm::fnv1a("ibm128"),           0,                        BuiltinType::EBuiltinKind::IMB_128 }
    };

    static RuntimeRegistry::MetaHashMap<FundamentalRuntimeMeta*> initial_static_builtin_map()
    {
        RuntimeRegistry::MetaHashMap<FundamentalRuntimeMeta*> map;
        for (auto& bt : builtin_types)
        {
            map[bt.hash] = &bt;
        }
    }

    static RuntimeRegistry::MetaHashMap<FundamentalRuntimeMeta*> static_builtin_map = initial_static_builtin_map();

    FundamentalRuntimeMeta* RuntimeRegistry::findFundamental(std::string_view name)
    {
        size_t hash = lux::cxx::algorithm::fnv1a(name);
        if (auto it = static_builtin_map.find(hash); it != static_builtin_map.end())
        {
            return it->second;
        }
        return nullptr;
    }

    FundamentalRuntimeMeta* RuntimeRegistry::findFundamental(size_t hash)
    {
        if (auto it = static_builtin_map.find(hash); it != static_builtin_map.end())
        {
            return it->second;
        }
        return nullptr;
    }

    const std::vector<FundamentalRuntimeMeta>& RuntimeRegistry::fundamentalMetaLists()
    {
		return builtin_types;
    }

    RuntimeRegistry::RuntimeRegistry(){}

    // -------------------------------------------
    //  Registration
    // -------------------------------------------
    void RuntimeRegistry::registerMeta(RecordRuntimeMeta* meta)
    {
        record_meta_list_.push_back(meta);
        size_t index = record_meta_list_.size() - 1;

        MetaInfo info{
            ERuntimeMetaType::RECORD,
            index,
            static_cast<void*>(meta)
        };
        meta_map_[meta->hash] = info;
    }

    void RuntimeRegistry::registerMeta(EnumRuntimeMeta* meta)
    {
        enum_meta_list_.push_back(meta);
        size_t index = enum_meta_list_.size() - 1;

        MetaInfo info{
            ERuntimeMetaType::ENUM,
            index,
            static_cast<void*>(meta)
        };
        meta_map_[meta->hash] = info;
    }

    void RuntimeRegistry::registerMeta(FunctionRuntimeMeta* meta)
    {
        function_meta_list_.push_back(meta);
        size_t index = function_meta_list_.size() - 1;

        MetaInfo info{
            ERuntimeMetaType::FUNCTION,
            index,
            static_cast<void*>(meta)
        };
        meta_map_[meta->hash] = info;
    }

    // -------------------------------------------
    //  Checking / Finding
    // -------------------------------------------
    bool RuntimeRegistry::hasMeta(std::string_view name)
    {
        size_t h = algorithm::fnv1a(name);
        return (meta_map_.find(h) != meta_map_.end());
    }

    bool RuntimeRegistry::hasMeta(size_t hash)
    {
		return (meta_map_.find(hash) != meta_map_.end());
    }

    const MetaInfo* RuntimeRegistry::findMeta(std::string_view name)
    {
        size_t h = algorithm::fnv1a(name);
        return findMeta(h);
    }

    const MetaInfo* RuntimeRegistry::findMeta(size_t hash)
    {
        auto it = meta_map_.find(hash);
        if (it != meta_map_.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    // -------------------------------------------
    //  Accessing specific typed metadata by index
    // -------------------------------------------
    RecordRuntimeMeta* RuntimeRegistry::findRecord(size_t index)
    {
        if (index < record_meta_list_.size())
        {
            return record_meta_list_[index];
        }
        return nullptr;
    }

    EnumRuntimeMeta* RuntimeRegistry::findEnum(size_t index)
    {
        if (index < enum_meta_list_.size())
        {
            return enum_meta_list_[index];
        }
        return nullptr;
    }

    FunctionRuntimeMeta* RuntimeRegistry::findFunction(size_t index)
    {
        if (index < function_meta_list_.size())
        {
            return function_meta_list_[index];
        }
        return nullptr;
    }

} // namespace lux::cxx::dref::runtime
