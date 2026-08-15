#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <source_location>
#include <string_view>

#include <lux/cxx/core/FixedText.hpp>
#include <lux/cxx/core/StableNameId.hpp>
#include <lux/cxx/time/Timestamp.hpp>

namespace lux::cxx
{
    enum class EDiagnosticSeverity : std::uint8_t
    {
        TRACE,
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    struct DiagnosticCategoryTag final
    {
    };

    using DiagnosticCategory = StableNameIdView<DiagnosticCategoryTag>;

    struct SteadyDiagnosticClock final
    {
        using timestamp_type = Timestamp<SteadyTimeDomain>;

        [[nodiscard]] static timestamp_type now() noexcept
        {
            return timestamp_type{
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count()
            };
        }
    };

    template<typename Clock>
    concept diagnostic_clock = requires
    {
        typename Clock::timestamp_type;
        { Clock::now() } -> std::same_as<typename Clock::timestamp_type>;
    };

    [[nodiscard]] inline std::uint64_t diagnosticThreadId() noexcept
    {
        static std::atomic<std::uint64_t> next_id{1};
        thread_local const std::uint64_t thread_id =
            next_id.fetch_add(1, std::memory_order_relaxed);
        return thread_id;
    }

    template<
        std::size_t MessageCapacity,
        diagnostic_clock Clock = SteadyDiagnosticClock,
        typename Category = DiagnosticCategory,
        typename Code = std::uint32_t
    >
    struct BasicDiagnosticRecord final
    {
        using clock_type = Clock;
        using timestamp_type = typename Clock::timestamp_type;
        using category_type = Category;
        using code_type = Code;

        EDiagnosticSeverity severity = EDiagnosticSeverity::INFO;
        Category category{};
        Code code{};
        timestamp_type timestamp{};
        std::uint64_t sequence = 0;
        std::uint64_t thread_id = 0;
        FixedText<MessageCapacity> message{};
        std::source_location source{};
        bool message_truncated = false;

        constexpr BasicDiagnosticRecord() noexcept = default;

        constexpr BasicDiagnosticRecord(
            EDiagnosticSeverity record_severity,
            Category record_category,
            Code record_code,
            timestamp_type record_timestamp,
            std::uint64_t record_sequence,
            std::uint64_t record_thread_id,
            std::string_view record_message,
            std::source_location record_source = std::source_location::current()
        ) noexcept
            : severity(record_severity),
              category(record_category),
              code(record_code),
              timestamp(record_timestamp),
              sequence(record_sequence),
              thread_id(record_thread_id),
              source(record_source)
        {
            message_truncated = !message.assign(record_message);
        }

        [[nodiscard]] static BasicDiagnosticRecord capture(
            EDiagnosticSeverity record_severity,
            Category record_category,
            Code record_code,
            std::uint64_t record_sequence,
            std::string_view record_message,
            std::source_location record_source = std::source_location::current()
        ) noexcept
        {
            return BasicDiagnosticRecord{
                record_severity,
                record_category,
                record_code,
                Clock::now(),
                record_sequence,
                diagnosticThreadId(),
                record_message,
                record_source
            };
        }
    };

    using DiagnosticRecord = BasicDiagnosticRecord<192>;
} // namespace lux::cxx
