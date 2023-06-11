#pragma once
#include <memory>
#include <lux/cxx/visibility.h>

namespace lux::reflection
{
    class LuxCxxParserImpl;
    
    class LuxCxxParser
    {
    public:
        LUX_CXX_PUBLIC LuxCxxParser();

        LUX_CXX_PUBLIC ~LuxCxxParser();

    private:
        std::unique_ptr<LuxCxxParserImpl> _impl;
    };
} // namespace lux::reflection
