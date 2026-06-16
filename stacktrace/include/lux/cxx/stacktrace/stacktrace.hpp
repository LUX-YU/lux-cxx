#pragma once
/*
 * lux::cxx::stacktrace — std::stacktrace on C++23, a cpptrace-backed shim mapped
 * to the same interface otherwise.
 *
 * Mirrors the lux::cxx::expected pattern: when the standard facility is available
 * (feature-test macro, not __cplusplus — see expected.hpp for why), alias it;
 * otherwise provide a drop-in with the same public surface. Backing the fallback
 * with cpptrace is hidden entirely in stacktrace.cpp — this header pulls in no
 * cpptrace type, so consumers never see it.
 *
 *   auto st = lux::cxx::stacktrace::current();
 *   for (auto& f : st)
 *       std::cout << f.description() << " @ " << f.source_file() << ':' << f.source_line() << '\n';
 *   std::cout << lux::cxx::to_string(st);
 */

// <version> defines the __cpp_lib_* feature-test macros without dragging in a
// heavy header. It MUST precede the sniff below: those macros are undefined until
// a standard header exposes them, so without this the check silently fails on a
// conforming C++23 toolchain and we fall back to cpptrace even when std has it.
#include <version>

#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L && __has_include(<stacktrace>)
#   define LUX_HAS_STD_STACKTRACE 1
#   include <stacktrace>
#   include <string>

namespace lux::cxx
{
    using stacktrace_entry = std::stacktrace_entry;
    using stacktrace       = std::stacktrace;

    // Uniform free functions so call sites don't care which backend is active.
    [[nodiscard]] inline std::string to_string(const stacktrace_entry& e) { return std::to_string(e); }
    [[nodiscard]] inline std::string to_string(const stacktrace&       s) { return std::to_string(s); }
}

#else
#   define LUX_HAS_STD_STACKTRACE 0
#   include <cstddef>
#   include <cstdint>
#   include <iosfwd>
#   include <string>
#   include <vector>

namespace lux::cxx
{
    // ---- mirrors std::stacktrace_entry --------------------------------------
    // Eagerly resolved (cpptrace fills description/file/line at capture time),
    // so the accessors are cheap and never throw.
    class stacktrace_entry
    {
    public:
        using native_handle_type = std::uintptr_t;

        stacktrace_entry() noexcept = default;

        [[nodiscard]] native_handle_type native_handle() const noexcept { return addr_; }
        [[nodiscard]] explicit operator bool()          const noexcept { return addr_ != 0; }

        [[nodiscard]] std::string         description() const { return desc_; }
        [[nodiscard]] std::string         source_file() const { return file_; }
        [[nodiscard]] std::uint_least32_t source_line() const noexcept { return line_; }

        friend bool operator==(const stacktrace_entry&, const stacktrace_entry&) noexcept = default;

    private:
        friend class stacktrace;
        native_handle_type  addr_ = 0;
        std::string         desc_;
        std::string         file_;
        std::uint_least32_t line_ = 0;
    };

    // ---- mirrors std::stacktrace (the common subset) ------------------------
    class stacktrace
    {
    public:
        using value_type     = stacktrace_entry;
        using const_iterator = std::vector<stacktrace_entry>::const_iterator;
        using size_type      = std::size_t;

        stacktrace() = default;

        // Capture the current call stack. `skip` frames are dropped from the top
        // (this frame is always skipped); at most `max_depth` frames are kept.
        // Never throws — returns an empty trace on any failure (like std's noexcept current()).
        [[nodiscard]] static stacktrace current(size_type skip = 0,
                                                size_type max_depth = static_cast<size_type>(-1)) noexcept;

        [[nodiscard]] bool        empty() const noexcept { return frames_.empty(); }
        [[nodiscard]] size_type   size()  const noexcept { return frames_.size(); }
        [[nodiscard]] const stacktrace_entry& operator[](size_type i) const { return frames_[i]; }
        [[nodiscard]] const stacktrace_entry& at(size_type i)         const { return frames_.at(i); }
        [[nodiscard]] const_iterator begin() const noexcept { return frames_.begin(); }
        [[nodiscard]] const_iterator end()   const noexcept { return frames_.end(); }

        friend bool operator==(const stacktrace&, const stacktrace&) noexcept = default;

    private:
        std::vector<stacktrace_entry> frames_;
    };

    // std::to_string(stacktrace) / operator<< equivalents (defined in stacktrace.cpp).
    [[nodiscard]] std::string to_string(const stacktrace_entry& e);
    [[nodiscard]] std::string to_string(const stacktrace&       s);
    std::ostream& operator<<(std::ostream& os, const stacktrace& s);
}

#endif // std::stacktrace availability
