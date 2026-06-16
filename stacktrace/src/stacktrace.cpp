// stacktrace backport implementation — the ONLY translation unit that sees
// cpptrace. Keeps the third-party type out of the public header (stacktrace.hpp
// exposes a plain, std::stacktrace-shaped interface). Compiled only when the
// standard <stacktrace> is unavailable (pre-C++23); on C++23 this file is empty.
#include <lux/cxx/stacktrace/stacktrace.hpp>

#if !LUX_HAS_STD_STACKTRACE

#include <cpptrace/cpptrace.hpp>

#include <ostream>
#include <string>
#include <utility>

namespace lux::cxx
{
    stacktrace stacktrace::current(size_type skip, size_type max_depth) noexcept
    {
        stacktrace out;
        try
        {
            // +1 so this frame (stacktrace::current itself) is not reported.
            const cpptrace::stacktrace t = cpptrace::generate_trace(skip + 1, max_depth);
            out.frames_.reserve(t.frames.size());
            for (const cpptrace::stacktrace_frame& f : t.frames)
            {
                stacktrace_entry e;
                e.addr_ = static_cast<stacktrace_entry::native_handle_type>(f.raw_address);
                e.desc_ = f.symbol;
                e.file_ = f.filename;
                e.line_ = f.line.has_value() ? static_cast<std::uint_least32_t>(f.line.value()) : 0u;
                out.frames_.push_back(std::move(e));
            }
        }
        catch (...)
        {
            // Capturing a trace must never propagate (mirrors std's noexcept current()).
        }
        return out;
    }

    std::string to_string(const stacktrace_entry& e)
    {
        std::string s = e.description().empty() ? std::string{ "??" } : e.description();
        if (const std::string file = e.source_file(); !file.empty())
        {
            s += " at ";
            s += file;
            if (e.source_line() != 0)
            {
                s += ':';
                s += std::to_string(e.source_line());
            }
        }
        return s;
    }

    std::string to_string(const stacktrace& s)
    {
        std::string out;
        for (stacktrace::size_type i = 0; i < s.size(); ++i)
        {
            out += std::to_string(i);
            out += "# ";
            out += to_string(s[i]);
            out += '\n';
        }
        return out;
    }

    std::ostream& operator<<(std::ostream& os, const stacktrace& s)
    {
        return os << to_string(s);
    }
}

#endif // !LUX_HAS_STD_STACKTRACE
