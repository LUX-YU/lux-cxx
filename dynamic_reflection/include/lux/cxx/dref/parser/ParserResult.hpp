#pragma once
#include <lux/cxx/visibility.h>
#include <memory>
#include <vector>
#include <string_view>

#include <lux/cxx/dref/runtime/MetaType.hpp>
#include <lux/cxx/dref/runtime/Class.hpp>

namespace lux::cxx::dref
{
    class ParserResultImpl;

    class ParserResult
    {
        friend class CxxParser;
    public:
        ParserResult(ParserResult&&) = default;
        ParserResult& operator=(ParserResult&&) = default;

        LUX_CXX_PUBLIC ~ParserResult();

        [[nodiscard]] LUX_CXX_PUBLIC std::vector<runtime::ClassMeta> findMarkedClassMetaByName(const std::string& mark);

        [[nodiscard]] LUX_CXX_PUBLIC std::vector<runtime::ClassMeta> allMarkedClassMeta();

    private:
        LUX_CXX_PUBLIC ParserResult();

        LUX_CXX_PUBLIC void appendClassMeta(runtime::ClassMeta mark);

        std::unique_ptr<ParserResultImpl> _impl;
    };
}