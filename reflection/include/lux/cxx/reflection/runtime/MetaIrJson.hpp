#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <lux/cxx/compile_time/expected.hpp>
#include <lux/cxx/reflection/runtime/MetaIr.hpp>
#include <lux/cxx/visibility.h>

namespace lux::cxx::reflection::ir
{
    enum class EMetaIrJsonError : std::uint8_t
    {
        INVALID_JSON,
        INVALID_SHAPE,
        IR_LIMIT
    };

    /// Builds the canonical compact, contiguous metadata unit from the JSON
    /// projection consumed by generator templates. The JSON is retained as a
    /// projection attribute; parser/generator transport and binary storage use
    /// MetaIr exclusively.
    [[nodiscard]] LUX_CXX_PUBLIC expected<MetaUnit, EMetaIrJsonError>
    makeMetaUnitFromTemplateJson(std::string_view json_text);

    /// Reconstructs the template-facing JSON projection. This is not a second
    /// runtime AST and has no independent identity or mutation API.
    [[nodiscard]] LUX_CXX_PUBLIC expected<std::string, EMetaIrJsonError>
    templateJson(const MetaUnit& unit);
}
