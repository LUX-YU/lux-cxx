#pragma once
// Typed structured values, without a DOM, token vector, RTTI or virtual dispatch.
// Grammar and failure/allocator contracts are specified in docs/GRAMMAR.md.
#include <lux/cxx/arguments/ContainerTraits.hpp>
#include <lux/cxx/arguments/detail/StructureScan.hpp>

namespace lux::cxx::args
{

    struct StructuredSettings
    {
        EDuplicateKeys keys = EDuplicateKeys::REJECT;
        std::size_t max_depth = detail::structure_depth_limit;
        std::size_t max_elements = 1u << 20;
    };

    template<class T>
    [[nodiscard]] ValueError parseStructured(
        std::string_view input, T& output, StructuredSettings settings = {}
    );

    namespace detail
    {
        struct StructuredParserTag
        {
            static constexpr bool native_structured = true;
        };
    } // namespace detail

    // An explicit value_parser<MyType> specialization overrides this fallback, even
    // when MyType also has container traits. No probe for "has_custom_parser" needed.
    template<class T>
        requires(detail::sequence<T> || detail::mapping<T> || detail::tuple_like<T>)
    struct ValueParser<T> : detail::StructuredParserTag
    {
        static constexpr bool structured = true;
        static ValueError parse(std::string_view input, T& output)
        {
            return parseStructured(input, output);
        }
    };

    namespace detail
    {

        class ValueCursor
        {
        public:
            ValueCursor(std::string_view input, StructuredSettings settings) noexcept
                : input_(input),
                  settings_(settings)
            {
            }

            template<class T>
            ValueError parse(T& output)
            {
                if (settings_.max_depth > structure_depth_limit)
                    return {EErrorCode::INVALID_INPUT, 0};
                if (auto e = read(output))
                    return e;
                whitespace();
                return position_ == input_.size() ? ValueError{} : error(EErrorCode::INVALID_STRUCTURE);
            }

        private:
            [[nodiscard]] ValueError error(EErrorCode code) const noexcept
            {
                return {code, position_};
            }
            void whitespace() noexcept
            {
                while (position_ < input_.size() && space(input_[position_]))
                    ++position_;
            }
            [[nodiscard]] char peek() const noexcept
            {
                return position_ < input_.size() ? input_[position_] : '\0';
            }
            bool consume(char c) noexcept
            {
                if (position_ < input_.size() && input_[position_] == c)
                {
                    ++position_;
                    return true;
                }
                return false;
            }
            static bool boundary(char c) noexcept
            {
                return space(c) || c == ',' || c == ':' || c == '[' || c == ']' || c == '{' || c == '}' ||
                       c == '"' || c == '\'';
            }
            std::string_view atom() noexcept
            {
                const auto start = position_;
                while (position_ < input_.size() && !boundary(input_[position_]))
                    ++position_;
                return input_.substr(start, position_ - start);
            }
            bool nullValue() noexcept
            {
                if (input_.substr(position_, 4) != "null")
                    return false;
                const auto end = position_ + 4;
                if (end != input_.size() && !boundary(input_[end]))
                    return false;
                position_ = end;
                return true;
            }
            ValueError enter(char open) noexcept
            {
                if (depth_ == settings_.max_depth)
                    return error(EErrorCode::DEPTH_LIMIT);
                if (!consume(open))
                    return error(EErrorCode::INVALID_STRUCTURE);
                ++depth_;
                whitespace();
                return {};
            }
            ValueError tail(char close, bool& done) noexcept
            {
                whitespace();
                if (consume(close))
                {
                    --depth_;
                    done = true;
                    return {};
                }
                if (!consume(','))
                    return error(EErrorCode::INVALID_STRUCTURE);
                whitespace();
                if (peek() == close)
                    return error(EErrorCode::INVALID_STRUCTURE); // no trailing comma
                done = false;
                return {};
            }

            template<class T>
            ValueError read(T& output)
            {
                whitespace();
                if (position_ == input_.size())
                    return error(EErrorCode::INVALID_STRUCTURE);
                if (elements_ == settings_.max_elements)
                    return error(EErrorCode::ELEMENT_LIMIT);
                ++elements_;
                using Parser = ValueParser<T>;
                if constexpr (requires { Parser::builtin_string; })
                {
                    return string(output);
                }
                else if constexpr (requires { Parser::builtin_optional; })
                {
                    if (nullValue())
                    {
                        output.reset();
                        return {};
                    }
                    if (!output)
                        output.emplace();
                    return read(*output);
                }
                else if constexpr (requires { Parser::native_structured; })
                {
                    constexpr int categories = int(sequence<T>) + int(mapping<T>) + int(tuple_like<T>);
                    static_assert(
                        categories == 1, "a type must have exactly one structural trait category"
                    );
                    if constexpr (sequence<T>)
                        return sequenceValue(output);
                    else if constexpr (mapping<T>)
                        return mapValue(output);
                    else
                        return tupleValue(output);
                }
                else
                {
                    return leaf(output);
                }
            }

            template<class T>
            ValueError leaf(T& output)
            {
                const auto start = position_;
                // A custom parser may accept a complete balanced structure. It gets an
                // exact source slice, not a reconstructed string with altered quotes.
                if (peek() == '[' || peek() == '{')
                {
                    if (auto e = skipStructure(input_, position_, settings_.max_depth - depth_))
                        return e;
                }
                else if (peek() == '"' || peek() == '\'')
                {
                    const char quote = input_[position_++];
                    bool closed = false;
                    while (position_ < input_.size())
                    {
                        const char c = input_[position_++];
                        if (c == quote)
                        {
                            closed = true;
                            break;
                        }
                        if (c == '\\')
                        {
                            if (position_ == input_.size())
                                return error(EErrorCode::INVALID_ESCAPE);
                            ++position_;
                        }
                    }
                    if (!closed)
                        return error(EErrorCode::UNCLOSED_QUOTE);
                }
                else
                {
                    (void)atom();
                }
                if (position_ == start)
                    return error(EErrorCode::INVALID_STRUCTURE);
                const auto text = input_.substr(start, position_ - start);
                static_assert(
                    requires {
                        { ParserFor<T>::parse(text, output) } -> conversion_status;
                    }, "provide value_parser<T>::parse(string_view, T&) returning errc or ValueError"
                );
                ValueError e = ValueParser<T>::parse(text, output);
                if (e)
                    e.offset = start + (e.offset == no_position ? 0 : e.offset);
                return e;
            }
            template<class T>
            using ParserFor = ValueParser<T>;

            static int hex(char c) noexcept
            {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            }
            ValueError codeUnit(std::uint32_t& out) noexcept
            {
                out = 0;
                for (int i = 0; i != 4; ++i)
                {
                    if (position_ == input_.size())
                        return error(EErrorCode::INVALID_ESCAPE);
                    const int v = hex(input_[position_]);
                    if (v < 0)
                        return error(EErrorCode::INVALID_ESCAPE);
                    ++position_;
                    out = (out << 4) | static_cast<std::uint32_t>(v);
                }
                return {};
            }
            template<class S>
            ValueError unicode(S& out)
            {
                std::uint32_t cp;
                if (auto e = codeUnit(cp))
                    return e;
                if (cp >= 0xd800 && cp <= 0xdbff)
                {
                    if (!consume('\\') || !consume('u'))
                        return error(EErrorCode::INVALID_ESCAPE);
                    std::uint32_t low;
                    if (auto e = codeUnit(low))
                        return e;
                    if (low < 0xdc00 || low > 0xdfff)
                        return error(EErrorCode::INVALID_ESCAPE);
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                }
                else if (cp >= 0xdc00 && cp <= 0xdfff)
                    return error(EErrorCode::INVALID_ESCAPE);
                if (cp <= 0x7f)
                    out.push_back(static_cast<char>(cp));
                else if (cp <= 0x7ff)
                {
                    out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
                else if (cp <= 0xffff)
                {
                    out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
                else
                {
                    out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
                return {};
            }
            template<class S>
            ValueError string(S& out)
            {
                const char quote = peek();
                if (quote != '"' && quote != '\'')
                {
                    const auto text = atom();
                    if (text.empty())
                        return error(EErrorCode::INVALID_STRUCTURE);
                    for (char c : text)
                        if (static_cast<unsigned char>(c) < 32)
                            return error(EErrorCode::INVALID_STRUCTURE);
                    out.assign(text.data(), text.size());
                    return {};
                }
                ++position_;
                out.clear();
                auto segment = position_;
                while (position_ < input_.size())
                {
                    const char c = input_[position_++];
                    if (c == quote)
                    {
                        out.append(input_.data() + segment, position_ - segment - 1);
                        return {};
                    }
                    if (static_cast<unsigned char>(c) < 32)
                        return {EErrorCode::INVALID_STRUCTURE, position_ - 1};
                    if (c != '\\')
                        continue;
                    out.append(input_.data() + segment, position_ - segment - 1);
                    if (position_ == input_.size())
                        return error(EErrorCode::INVALID_ESCAPE);
                    const char escaped = input_[position_++];
                    switch (escaped)
                    {
                    case '"':
                        out.push_back('"');
                        break;
                    case '\'':
                        out.push_back('\'');
                        break;
                    case '\\':
                        out.push_back('\\');
                        break;
                    case '/':
                        out.push_back('/');
                        break;
                    case 'n':
                        out.push_back('\n');
                        break;
                    case 'r':
                        out.push_back('\r');
                        break;
                    case 't':
                        out.push_back('\t');
                        break;
                    case 'b':
                        out.push_back('\b');
                        break;
                    case 'f':
                        out.push_back('\f');
                        break;
                    case 'u':
                        if (auto e = unicode(out))
                            return e;
                        break;
                    default:
                        return {EErrorCode::INVALID_ESCAPE, position_ - 1};
                    }
                    segment = position_;
                }
                return error(EErrorCode::UNCLOSED_QUOTE);
            }

            template<class C>
            ValueError sequenceValue(C& out)
            {
                using Traits = SequenceTraits<C>;
                using E = typename Traits::value_type;
                constexpr bool fixed = Traits::extent != dynamic_extent;
                constexpr bool indexed = !fixed && indexed_dynamic_sequence<C>;
                if (auto e = enter('['))
                    return e;
                if constexpr (!fixed && !indexed)
                    Traits::clear(out);
                std::size_t count = 0;
                bool done = consume(']');
                if (done)
                    --depth_;
                while (!done)
                {
                    if constexpr (fixed)
                    {
                        if (count == Traits::extent)
                            return error(EErrorCode::SIZE_MISMATCH);
                        if (auto e = read(Traits::at(out, count)))
                            return e;
                    }
                    else if constexpr (indexed)
                    {
                        if (count == Traits::size(out))
                        {
                            if (auto e = Traits::resize(out, count + 1); e != EErrorCode::OK)
                                return error(e);
                        }
                        if (auto e = read(Traits::at(out, count)))
                            return e;
                    }
                    else
                    {
                        E value = [&]() -> E {
                            if constexpr (requires { Traits::makeElement(out); })
                                return Traits::makeElement(out);
                            else
                                return E{};
                        }();
                        if (auto e = read(value))
                            return e;
                        if (auto e = Traits::append(out, std::move(value)); e != EErrorCode::OK)
                            return error(e);
                    }
                    ++count;
                    if (auto e = tail(']', done))
                        return e;
                }
                if constexpr (fixed)
                {
                    if (count != Traits::extent)
                        return error(EErrorCode::SIZE_MISMATCH);
                }
                else if constexpr (indexed)
                {
                    if (auto e = Traits::resize(out, count); e != EErrorCode::OK)
                        return error(e);
                }
                return {};
            }
            template<class M>
            ValueError mapValue(M& out)
            {
                using Traits = MapTraits<M>;
                if (auto e = enter('{'))
                    return e;
                Traits::clear(out);
                bool done = consume('}');
                if (done)
                    --depth_;
                while (!done)
                {
                    const auto key_start = position_;
                    auto key = [&]() -> typename Traits::key_type {
                        if constexpr (requires { Traits::makeKey(out); })
                            return Traits::makeKey(out);
                        else
                            return {};
                    }();
                    auto value = [&]() -> typename Traits::mapped_type {
                        if constexpr (requires { Traits::makeMapped(out); })
                            return Traits::makeMapped(out);
                        else
                            return {};
                    }();
                    if (auto e = read(key))
                        return e;
                    whitespace();
                    if (!consume(':'))
                        return error(EErrorCode::INVALID_STRUCTURE);
                    if (auto e = read(value))
                        return e;
                    if (auto e = Traits::insert(out, std::move(key), std::move(value), settings_.keys);
                        e != EErrorCode::OK)
                        return {e, key_start};
                    if (auto e = tail('}', done))
                        return e;
                }
                return {};
            }
            template<class T>
            ValueError tupleValue(T& out)
            {
                using Traits = TupleTraits<T>;
                if (auto e = enter('['))
                    return e;
                ValueError failure;
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (([&] {
                         if (failure)
                             return;
                         if constexpr (I != 0)
                         {
                             whitespace();
                             if (!consume(','))
                             {
                                 failure = error(EErrorCode::SIZE_MISMATCH);
                                 return;
                             }
                         }
                         whitespace();
                         if (peek() == ']')
                         {
                             failure = error(EErrorCode::SIZE_MISMATCH);
                             return;
                         }
                         failure = read(Traits::template get<I>(out));
                     }()),
                     ...);
                }(std::make_index_sequence<Traits::size>{});
                if (failure)
                    return failure;
                whitespace();
                if (!consume(']'))
                    return error(EErrorCode::SIZE_MISMATCH);
                --depth_;
                return {};
            }

            std::string_view input_;
            StructuredSettings settings_;
            std::size_t position_ = 0;
            std::size_t depth_ = 0;
            std::size_t elements_ = 0;
        };
    } // namespace detail

    template<class T>
    [[nodiscard]] ValueError parseStructured(std::string_view input, T& output, StructuredSettings settings)
    {
        static_assert(!BorrowsInput<T>::value, "structured outputs must own their strings");
        detail::ValueCursor cursor(input, settings);
        return cursor.parse(output);
    }

    // .convert(structured({...})) configures duplicate-key/depth/element policy for
    // this option only; settings are passed down the whole tree, not global state.
    struct StructuredConverter
    {
        static constexpr bool structured = true;
        StructuredSettings settings{};
        template<class T>
        ValueError operator()(std::string_view input, T& output) const
        {
            return parseStructured(input, output, settings);
        }
    };
    [[nodiscard]] constexpr StructuredConverter structured(StructuredSettings settings = {}) noexcept
    {
        return {settings};
    }

} // namespace lux::cxx::args
