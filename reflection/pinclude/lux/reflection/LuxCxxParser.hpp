#pragma once
#include <memory>
#include <lux/cxx/visibility_control.h>

namespace lux::reflection
{
    class LuxCxxParserImpl;
    
    class LuxCxxParser
    {
    public:
        LUX_EXPORT LuxCxxParser();

        LUX_EXPORT ~LuxCxxParser();

    private:
        std::unique_ptr<LuxCxxParserImpl> _impl;
    };
} // namespace lux::reflection
