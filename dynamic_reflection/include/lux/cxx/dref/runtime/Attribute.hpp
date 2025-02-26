#pragma once
#include <cstddef>
#define LUX_REF_MARK_PREFIX "LUX::META;"

#if defined __LUX_PARSE_TIME__
#define LUX_REFL(...) __attribute__((annotate(LUX_REF_MARK_PREFIX #__VA_ARGS__)))
#else
#define LUX_REFL(...)
#endif

namespace lux::cxx::dref
{
    enum class EMetaType
    {
        ClASS,
        STRUCT,
        ENUMERATOR
    };

    enum class EVisibility
    {
        INVALID,
        PUBLIC,
        PROTECTED,
        PRIVATE
    };

    struct FieldInfo
    {
        const char* name;
        size_t      offset;
        EVisibility visibility;
        size_t      index;
    };

    template <typename T> class type_meta;
}
