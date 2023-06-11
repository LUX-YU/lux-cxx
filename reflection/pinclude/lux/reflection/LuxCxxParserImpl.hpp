#pragma once
#include <lux/reflection/libclang.hpp>
#include <lux/cxx/visibility.h> // TODO remove LUX_EXPORT

namespace lux::reflection
{
    class LuxCxxParserImpl
    {
    public:
        LUX_CXX_PUBLIC LuxCxxParserImpl();

        LUX_CXX_PUBLIC ~LuxCxxParserImpl();

        LUX_CXX_PUBLIC TranslationUnit translate(const std::string& file, const std::vector<std::string>& commands);

    private:
        // shared context for creating translation units
        CXIndex clang_index;
    };
} // namespace lux::reflection
