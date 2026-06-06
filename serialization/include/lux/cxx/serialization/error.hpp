#pragma once
#include <string>
#include <lux/cxx/compile_time/expected.hpp>

namespace lux::cxx::ser
{
    enum class error_code
    {
        ok = 0,
        parse_error,        ///< backend failed to parse the input text
        type_mismatch,      ///< node kind didn't match the target type
        missing_required,   ///< a field marked `required` was absent
        load_failed,        ///< generic deserialization failure
    };

    struct error
    {
        error_code  code = error_code::ok;
        std::string message;
        std::string path;   ///< dotted path of the offending field (best-effort)
    };

    template <class T>
    using result = lux::cxx::expected<T, error>;

    inline lux::cxx::unexpected<error> make_error(error_code code, std::string msg, std::string path = {})
    {
        return lux::cxx::unexpected<error>{ error{ code, std::move(msg), std::move(path) } };
    }
} // namespace lux::cxx::ser
