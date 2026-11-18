#pragma once
// Shared grammar, compile-time name index, cardinality and duplicate policies.
// Neither public result strategy implements its own parser loop.

#include <lux/cxx/arguments/Option.hpp>
#include <lux/cxx/arguments/Sources.hpp>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lux::cxx::args::detail
{

    template<class... Options>
    class Engine
    {
        static constexpr std::size_t option_count_ = sizeof...(Options);
        static_assert(option_count_ != 0, "args::parse requires at least one option");
        static_assert(option_count_ < no_option, "too many options");
        static constexpr option_id invalid_id_ = no_option;
        static constexpr bool all_static_ = (Options::named_at_compile_time && ...);
        using descriptors_type = std::tuple<const Options&...>;
        static constexpr std::array<std::string_view, option_count_> static_names_{
            Options::static_name.view()...
        };
        static constexpr std::array<std::string_view, option_count_> static_shorts_{
            Options::static_short.view()...
        };
        static constexpr std::size_t table_size_ = std::bit_ceil(option_count_ * 2);

        struct HashEntry
        {
            std::string_view name{};
            std::uint64_t hash = 0;
            option_id index = invalid_id_;
        };

        static consteval bool staticNamesValid()
        {
            for (std::size_t i = 0; i < option_count_; ++i)
            {
                if (static_names_[i] == "help" || static_shorts_[i] == "h")
                    return false;
                for (std::size_t j = 0; j < i; ++j)
                {
                    if (!static_names_[i].empty() && static_names_[i] == static_names_[j])
                        return false;
                    if (!static_shorts_[i].empty() && static_shorts_[i] == static_shorts_[j])
                        return false;
                }
            }
            return true;
        }
        static_assert(staticNamesValid(), "duplicate/reserved compile-time option name");

        static consteval auto buildIndex()
        {
            std::array<HashEntry, table_size_> table{};
            for (option_id i = 0; i < option_count_; ++i)
            {
                if (static_names_[i].empty())
                    continue;
                const auto h = hash(static_names_[i]);
                auto k = static_cast<std::size_t>(h) & (table_size_ - 1);
                while (table[k].index != invalid_id_)
                    k = (k + 1) & (table_size_ - 1);
                table[k] = HashEntry{static_names_[i], h, i};
            }
            return table;
        }
        static constexpr auto name_index_ = buildIndex();

        static consteval auto buildShortIndex()
        {
            std::array<option_id, 256> table{};
            table.fill(invalid_id_);
            for (option_id i = 0; i < option_count_; ++i)
                if (static_shorts_[i].size() == 1)
                    table[static_cast<unsigned char>(static_shorts_[i][0])] = i;
            return table;
        }
        static constexpr auto short_index_ = buildShortIndex();

        // Stateless function table: one instantiation per option type, no RTTI,
        // std::function, variant visitation or heap allocation in the hot path.
        template<class Sink>
        using Apply = ValueError (*)(Sink&, const descriptors_type&, std::string_view, bool, bool);

        template<std::size_t I, class Sink>
        static ValueError apply(
            Sink& sink, const descriptors_type& descriptions, std::string_view text, bool store, bool reset
        )
        {
            using Opt = std::tuple_element_t<I, std::tuple<Options...>>;
            using T = typename Opt::value_type;
            const auto& opt = std::get<I>(descriptions);
            static_assert(
                converter_for<typename Opt::converter_type, typename Opt::element_type>,
                "converter must accept (string_view, parsed_type&) and return errc or ValueError"
            );
            static_assert(
                validator_for<typename Opt::validator_type, typename Opt::element_type>,
                "validator must accept the parsed value type and return bool"
            );
            if constexpr (Opt::element_mode)
            {
                using Element = typename Opt::element_type;
                Element value = [&]() -> Element {
                    if constexpr (requires { SequenceTraits<T>::makeElement(sink.template value<I>()); })
                        return SequenceTraits<T>::makeElement(sink.template value<I>());
                    else
                        return Element{};
                }();
                if (ValueError e = opt.converter()(text, value); e)
                    return e;
                if (!opt.validator()(value))
                    return EErrorCode::VALIDATION_FAILED;
                if (store)
                {
                    auto& container = sink.template value<I>();
                    if (reset)
                        SequenceTraits<T>::clear(container);
                    return SequenceTraits<T>::append(container, std::move(value));
                }
            }
            else if (store)
            {
                T& target = sink.template value<I>();
                if (ValueError e = opt.converter()(text, target); e)
                    return e;
                if (!opt.validator()(target))
                    return EErrorCode::VALIDATION_FAILED;
            }
            else
            {
                T discard{};
                if (ValueError e = opt.converter()(text, discard); e)
                    return e;
                if (!opt.validator()(discard))
                    return EErrorCode::VALIDATION_FAILED;
            }
            return EErrorCode::OK;
        }

        template<class Sink, std::size_t... I>
        static consteval auto makeApply(std::index_sequence<I...>)
        {
            return std::array<Apply<Sink>, option_count_>{&apply<I, Sink>...};
        }

    public:
        static constexpr std::size_t option_count = option_count_;

        template<class Sink, TokenSource Source>
        [[nodiscard]] static Error run(
            Source& source, Workspace& work, Sink& sink, const Options&... options
        )
        {
            const descriptors_type descriptors{options...};
            const std::array<std::string_view, option_count_> names{options.name()...};
            const std::array<std::string_view, option_count_> shorts{options.shortName()...};
            const std::array<bool, option_count_> required{options.isRequired()...};
            const std::array<bool, option_count_> multi{options.isMulti()...};
            const std::array<bool, option_count_> positional{options.isPositional()...};
            const std::array<char, option_count_> separators{options.separator()...};
            const std::array<std::size_t, option_count_> minima{options.minValues()...};
            const std::array<std::size_t, option_count_> maxima{options.maxValues()...};
            const std::array<ERepeat, option_count_> repeats{options.repeats()...};
            const std::array<bool, option_count_> flags{Options::is_flag...};
            static constexpr std::array<bool, option_count_> structured{Options::structured_syntax...};

            // Runtime-only descriptor validation is cold work. The statically named
            // path has its duplicate/reserved checks at compile time.
            std::array<option_id, option_count_> positional_ids{};
            std::size_t positional_count = 0;
            for (option_id i = 0; i < option_count_; ++i)
            {
                if ((multi[i] && minima[i] == 0) || minima[i] > maxima[i] ||
                    (flags[i] && (multi[i] || positional[i])) ||
                    (!multi[i] && (minima[i] != 1 || maxima[i] != unlimited)) ||
                    (repeats[i] == ERepeat::APPEND && !elementModeAt(i)) || (multi[i] && !elementModeAt(i)))
                    return {EErrorCode::INVALID_SCHEMA, i};
                if (positional[i])
                    positional_ids[positional_count++] = i;
                if constexpr (!all_static_)
                {
                    if (!validName(names[i]) || names[i] == "help" || shorts[i].size() > 1 ||
                        shorts[i] == "h" || (!shorts[i].empty() && !validName(shorts[i])))
                        return {EErrorCode::INVALID_SCHEMA, i};
                    for (option_id j = 0; j < i; ++j)
                        if (names[i] == names[j] || (!shorts[i].empty() && shorts[i] == shorts[j]))
                            return {EErrorCode::INVALID_SCHEMA, i};
                }
            }
            // A greedy positional collection can only be the final positional.
            for (std::size_t p = 0; p + 1 < positional_count; ++p)
                if (multi[positional_ids[p]])
                    return {EErrorCode::INVALID_SCHEMA, positional_ids[p]};

            // Defaults are already typed. Validate without stringify/parse round trips.
            Error default_error{};
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (([&] {
                     using Opt = std::tuple_element_t<I, std::tuple<Options...>>;
                     if constexpr (Opt::has_default)
                     {
                         if (default_error)
                             return;
                         const auto& o = std::get<I>(descriptors);
                         if constexpr (Opt::element_mode)
                         {
                             // Append-only custom traits need iteration only for defaults.
                             if constexpr (std::ranges::input_range<const typename Opt::value_type>)
                             {
                                 for (const auto& v : *o.defaultValue())
                                     if (!o.validator()(v))
                                     {
                                         default_error = {EErrorCode::INVALID_SCHEMA, I};
                                         break;
                                     }
                             }
                             else
                                 default_error = {EErrorCode::INVALID_SCHEMA, I};
                         }
                         else if (!o.validator()(*o.defaultValue()))
                             default_error = {EErrorCode::INVALID_SCHEMA, I};
                     }
                 }()),
                 ...);
            }(std::index_sequence_for<Options...>{});
            if (default_error)
                return default_error;
            if (auto error = sink.seed(descriptors); error)
                return error;
            static constexpr auto apply_table = makeApply<Sink>(std::index_sequence_for<Options...>{});

            auto find = [&](std::string_view name) noexcept -> option_id {
                const auto h = hash(name);
                auto k = static_cast<std::size_t>(h) & (table_size_ - 1);
                while (name_index_[k].index != invalid_id_)
                {
                    const auto& e = name_index_[k];
                    if (e.hash == h && e.name == name)
                        return e.index;
                    k = (k + 1) & (table_size_ - 1);
                }
                if constexpr (!all_static_)
                {
                    for (option_id i = 0; i < option_count_; ++i)
                        if (static_names_[i].empty() && names[i] == name)
                            return i;
                }
                return invalid_id_;
            };
            auto find_short = [&](char ch) noexcept -> option_id {
                const auto id = short_index_[static_cast<unsigned char>(ch)];
                if (id != invalid_id_)
                    return id;
                if constexpr (!all_static_)
                {
                    for (option_id i = 0; i < option_count_; ++i)
                        if (static_shorts_[i].empty() && shorts[i].size() == 1 && shorts[i][0] == ch)
                            return i;
                }
                return invalid_id_;
            };

            std::array<bool, option_count_> seen{};
            option_id pending = invalid_id_;
            std::size_t pending_values = 0;
            bool pending_store = true;
            bool pending_reset = false;
            Token pending_token{};
            bool options_enabled = true;
            std::size_t positional_cursor = 0;
            std::size_t positional_values = 0;
            bool positional_store = true;
            bool positional_reset = false;

            auto fail = [](EErrorCode code, option_id id, const Token& token) -> Error {
                return {code, id, token.position, token.location};
            };
            auto finish = [&]() -> Error {
                if (pending != invalid_id_ && pending_values < minima[pending])
                    return fail(EErrorCode::MISSING_VALUE, pending, pending_token);
                pending = invalid_id_;
                return {};
            };
            auto accept_value = [&](option_id id,
                                    std::string_view text,
                                    const Token& token,
                                    std::size_t& amount,
                                    bool store,
                                    bool& reset) -> Error {
                std::size_t segment_offset = 0;
                const auto append_one = [&](std::string_view part) -> Error {
                    if (amount >= (multi[id] ? maxima[id] : 1))
                        return fail(EErrorCode::TOO_MANY_VALUES, id, token);
                    if (const auto e = apply_table[id](sink, descriptors, part, store, reset); e)
                    {
                        auto error = fail(e.code, id, token);
                        error.value_offset =
                            e.offset == no_position ? no_position : segment_offset + e.offset;
                        return error;
                    }
                    reset = false;
                    ++amount;
                    return {};
                };
                if (separators[id] == 0)
                    return append_one(text);
                while (true)
                {
                    auto split = text.find(separators[id]);
                    if (structured[id])
                    {
                        split = std::string_view::npos;
                        for (std::size_t p = 0; p < text.size();)
                        {
                            if (text[p] == '[' || text[p] == '{')
                            {
                                if (auto e = skipStructure(text, p))
                                {
                                    auto error = fail(e.code, id, token);
                                    error.value_offset = segment_offset + e.offset;
                                    return error;
                                }
                            }
                            else if (text[p] == separators[id])
                            {
                                split = p;
                                break;
                            }
                            else
                                ++p;
                        }
                    }
                    if (auto e = append_one(text.substr(0, split)))
                        return e;
                    if (split == std::string_view::npos)
                        return {};
                    segment_offset += split + 1;
                    text.remove_prefix(split + 1);
                }
            };
            auto begin =
                [&](option_id id, const Token& token, bool has_value, std::string_view value, bool negated
                ) -> Error {
                const bool duplicate = seen[id];
                if (duplicate && repeats[id] == ERepeat::REJECT)
                    return fail(EErrorCode::DUPLICATE_OPTION, id, token);
                const bool store = !(duplicate && repeats[id] == ERepeat::FIRST);
                const bool reset = elementModeAt(id) && (!duplicate || repeats[id] == ERepeat::LAST);
                seen[id] = true;
                sink.supplied(id);
                if (flags[id])
                {
                    const std::string_view value_text =
                        has_value ? value
                                  : (negated ? std::string_view{"false"} : std::string_view{"true"});
                    return acceptFlag(id, value_text, token, store, apply_table, sink, descriptors, fail);
                }
                if (negated)
                    return fail(EErrorCode::UNKNOWN_OPTION, id, token);
                pending = id;
                pending_values = 0;
                pending_store = store;
                pending_reset = reset;
                pending_token = token;
                if (has_value)
                {
                    if (auto error =
                            accept_value(id, value, token, pending_values, pending_store, pending_reset))
                        return error;
                    if (pending_values == (multi[id] ? maxima[id] : 1))
                        pending = invalid_id_;
                }
                return {};
            };

            Token token{};
            Error source_error{};
            while (source.next(token, source_error, work))
            {
                const auto text = token.text;
                const bool classify = options_enabled && !token.protected_word;
                const bool dashed =
                    classify && text.size() > 1 && text.front() == '-' && !negativeNumber(text);
                const bool stop = dashed && text == "--";
                const auto equal = classify && !dashed ? text.find('=') : std::string_view::npos;
                const auto assignment_id = equal != std::string_view::npos && equal != 0
                                               ? find(text.substr(0, equal))
                                               : invalid_id_;
                const bool assignment = assignment_id != invalid_id_;
                const bool boundary = dashed || assignment;

                if (pending != invalid_id_)
                {
                    if (!boundary)
                    {
                        if (auto error = accept_value(
                                pending, text, token, pending_values, pending_store, pending_reset
                            ))
                            return error;
                        if (pending_values == (multi[pending] ? maxima[pending] : 1))
                            pending = invalid_id_;
                        continue;
                    }
                    if (auto error = finish())
                        return error;
                }
                if (stop)
                {
                    options_enabled = false;
                    continue;
                }
                if (dashed && (text == "-h" || text == "--help"))
                    return fail(EErrorCode::HELP_REQUESTED, invalid_id_, token);

                if (assignment || (dashed && text.starts_with("--")))
                {
                    std::string_view key = assignment ? text : text.substr(2);
                    const auto split = key.find('=');
                    const bool has_value = split != std::string_view::npos;
                    const auto value = has_value ? key.substr(split + 1) : std::string_view{};
                    key = key.substr(0, split);
                    option_id id = assignment ? assignment_id : find(key);
                    bool negated = false;
                    if (id == invalid_id_ && !assignment && !has_value && key.starts_with("no-"))
                    {
                        id = find(key.substr(3));
                        if (id != invalid_id_ && flags[id])
                            negated = true;
                        else
                            id = invalid_id_;
                    }
                    if (id == invalid_id_)
                        return fail(EErrorCode::UNKNOWN_OPTION, invalid_id_, token);
                    if (auto error = begin(id, token, has_value, value, negated))
                        return error;
                    continue;
                }
                if (dashed)
                {
                    for (std::size_t k = 1; k < text.size(); ++k)
                    {
                        const auto id = find_short(text[k]);
                        if (id == invalid_id_)
                        {
                            if (text[k] == 'h')
                                return fail(EErrorCode::HELP_REQUESTED, invalid_id_, token);
                            return fail(EErrorCode::UNKNOWN_OPTION, invalid_id_, token);
                        }
                        auto remainder = text.substr(k + 1);
                        const bool explicit_equal = remainder.starts_with('=');
                        if (explicit_equal)
                            remainder.remove_prefix(1);
                        const bool has_value = explicit_equal || (!flags[id] && !remainder.empty());
                        if (auto error = begin(id, token, has_value, remainder, false))
                            return error;
                        if (!flags[id] || explicit_equal)
                            break;
                    }
                    continue;
                }

                if (positional_cursor >= positional_count)
                    return fail(EErrorCode::UNEXPECTED_POSITIONAL, invalid_id_, token);
                const auto id = positional_ids[positional_cursor];
                if (positional_values == 0)
                {
                    const bool duplicate = seen[id];
                    if (duplicate && repeats[id] == ERepeat::REJECT)
                        return fail(EErrorCode::DUPLICATE_OPTION, id, token);
                    positional_store = !(duplicate && repeats[id] == ERepeat::FIRST);
                    positional_reset = elementModeAt(id) && (!duplicate || repeats[id] == ERepeat::LAST);
                    seen[id] = true;
                    sink.supplied(id);
                }
                if (auto error = accept_value(
                        id, text, token, positional_values, positional_store, positional_reset
                    ))
                    return error;
                if (positional_values == (multi[id] ? maxima[id] : 1))
                {
                    ++positional_cursor;
                    positional_values = 0;
                }
            }
            if (source_error)
                return source_error;
            if (auto error = finish())
                return error;
            if (positional_cursor < positional_count && positional_values != 0 &&
                positional_values < minima[positional_ids[positional_cursor]])
                return {EErrorCode::MISSING_VALUE, positional_ids[positional_cursor]};
            for (option_id i = 0; i < option_count_; ++i)
                if (required[i] && !seen[i])
                    return {EErrorCode::MISSING_REQUIRED, i};
            return {};
        }

    private:
        static constexpr std::array<bool, option_count_> element_flags_{Options::element_mode...};
        [[nodiscard]] static constexpr bool elementModeAt(std::size_t id) noexcept
        {
            return element_flags_[id];
        }
        template<class Sink, class Table, class Descriptions, class Fail>
        static Error acceptFlag(
            option_id id, std::string_view value, const Token& token, bool store, const Table& table,
            Sink& sink, const Descriptions& descriptions, Fail fail
        )
        {
            const auto e = table[id](sink, descriptions, value, store, false);
            if (!e)
                return {};
            auto error = fail(e.code, id, token);
            error.value_offset = e.offset;
            return error;
        }
    };

} // namespace lux::cxx::args::detail
