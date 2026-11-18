#pragma once
// Token sources shared by typed and direct outputs. Text parsing is deliberately
// small: CLI-like whitespace, single/double quotes and backslash escaping.
// There is no shell interpolation, globbing or environment expansion.

#include <lux/cxx/arguments/Value.hpp>
#include <lux/cxx/arguments/detail/StructureScan.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace lux::cxx::args
{

    // Not thread-shared. Only quoted/escaped tokens need temporary storage;
    // ordinary tokens refer directly to caller input and are consumed immediately.
    class Workspace
    {
    public:
        static constexpr std::size_t inline_capacity = 256;

        void reserve(std::size_t bytes)
        {
            spill_.reserve(bytes);
        }
        void reset() noexcept
        {
            size_ = 0;
            spilled_ = false;
            spill_.clear();
        }
        void append(std::string_view text)
        {
            if (!spilled_ && text.size() <= inline_capacity - size_)
            {
                std::copy(text.begin(), text.end(), small_.data() + size_);
                size_ += text.size();
                return;
            }
            if (!spilled_)
            {
                spill_.assign(small_.data(), size_);
                spilled_ = true;
            }
            spill_.append(text);
        }
        void push(char c)
        {
            append(std::string_view{&c, 1});
        }
        [[nodiscard]] std::string_view view() const noexcept
        {
            return spilled_ ? std::string_view{spill_} : std::string_view{small_.data(), size_};
        }

    private:
        std::array<char, inline_capacity> small_; // uninitialized because it is scratch only
        std::string spill_;
        std::size_t size_ = 0;
        bool spilled_ = false;
    };

    struct Token
    {
        std::string_view text{};
        std::size_t position = no_position;
        ELocationKind location = ELocationKind::NONE;
        // An entirely quoted option-looking word is data, not an option.
        bool protected_word = false;
    };

    template<class S>
    concept TokenSource = requires(S& src, Token& token, Error& e, Workspace& w) {
        { src.next(token, e, w) } -> std::same_as<bool>;
    };

    template<bool Structured>
    class BasicTextSource
    {
    public:
        explicit BasicTextSource(std::string_view input) noexcept
            : input_(input)
        {
        }

        bool next(Token& out, Error& error, Workspace& work)
        {
            while (pos_ < input_.size() && detail::space(input_[pos_]))
                ++pos_;
            if (pos_ == input_.size())
                return false;
            const std::size_t start = pos_;
            while (pos_ < input_.size())
            {
                const char c = input_[pos_];
                if (detail::space(c))
                    break;
                if constexpr (Structured)
                {
                    if (c == '[' || c == '{')
                    {
                        if (!group(error))
                            return false;
                        continue;
                    }
                }
                if (c == '\'' || c == '"' || c == '\\')
                    return decode(out, error, work, start);
                ++pos_;
            }
            out = {input_.substr(start, pos_ - start), start, ELocationKind::TEXT_OFFSET, false};
            return true;
        }

    private:
        bool group(Error& error)
        {
            if (auto e = detail::skipStructure(input_, pos_))
            {
                error = {e.code, no_option, e.offset, ELocationKind::TEXT_OFFSET};
                return false;
            }
            return true;
        }
        bool decode(Token& out, Error& error, Workspace& work, std::size_t start)
        {
            const bool protected_word = input_[start] == '\'' || input_[start] == '"';
            // Fast quoted token: a single quote pair without escape or concatenation.
            if (pos_ == start && protected_word)
            {
                const char q = input_[start];
                std::size_t end = start + 1;
                while (end < input_.size())
                {
                    if (input_[end] == q)
                        break;
                    if (input_[end] == '\\' && q == '"')
                        break;
                    ++end;
                }
                if (end < input_.size() && input_[end] == q &&
                    (end + 1 == input_.size() || detail::space(input_[end + 1])))
                {
                    pos_ = end + 1;
                    out = {
                        input_.substr(start + 1, end - start - 1), start, ELocationKind::TEXT_OFFSET, true
                    };
                    return true;
                }
            }

            work.reset();
            work.append(input_.substr(start, pos_ - start));
            char quote = 0;
            while (pos_ < input_.size())
            {
                const char c = input_[pos_];
                if (quote == 0 && detail::space(c))
                    break;
                if constexpr (Structured)
                {
                    if (quote == 0 && (c == '[' || c == '{'))
                    {
                        const auto begin = pos_;
                        if (!group(error))
                            return false;
                        work.append(input_.substr(begin, pos_ - begin));
                        continue;
                    }
                }
                if (quote != 0 && c == quote)
                {
                    quote = 0;
                    ++pos_;
                    continue;
                }
                if (quote == 0 && (c == '\'' || c == '"'))
                {
                    quote = c;
                    ++pos_;
                    continue;
                }
                if (c == '\\' && quote != '\'')
                {
                    if (pos_ + 1 == input_.size())
                    {
                        error = {EErrorCode::INVALID_ESCAPE, no_option, pos_, ELocationKind::TEXT_OFFSET};
                        return false;
                    }
                    const char next = input_[pos_ + 1];
                    // Outside quotes: escape the next byte. Inside double quotes:
                    // only quote/backslash are escapes; other backslashes stay raw.
                    if (quote == 0 || next == '"' || next == '\\')
                    {
                        work.push(next);
                        pos_ += 2;
                        continue;
                    }
                }
                work.push(c);
                ++pos_;
            }
            if (quote != 0)
            {
                error = {EErrorCode::UNCLOSED_QUOTE, no_option, start, ELocationKind::TEXT_OFFSET};
                return false;
            }
            out = {work.view(), start, ELocationKind::TEXT_OFFSET, protected_word};
            return true;
        }

        std::string_view input_;
        std::size_t pos_ = 0;
    };

    using TextSource = BasicTextSource<false>;
    using StructuredTextSource = BasicTextSource<true>;

    // Tokens are already separated and quote-decoded. No argv0 is expected here.
    class ViewSource
    {
    public:
        explicit ViewSource(std::span<const std::string_view> tokens) noexcept
            : tokens_(tokens)
        {
        }
        bool next(Token& out, Error&, Workspace&) noexcept
        {
            if (index_ == tokens_.size())
                return false;
            out = {tokens_[index_], index_, ELocationKind::ARGUMENT_INDEX, false};
            ++index_;
            return true;
        }

    private:
        std::span<const std::string_view> tokens_;
        std::size_t index_ = 0;
    };

    // Expects the conventional argv[0] program name. Input strings are not copied.
    class ArgvSource
    {
    public:
        ArgvSource(int argc, const char* const* argv) noexcept
            : argc_(argc),
              argv_(argv)
        {
        }
        bool next(Token& out, Error& error, Workspace&) noexcept
        {
            if (argc_ < 0 || (argc_ > 0 && argv_ == nullptr))
            {
                error = {EErrorCode::INVALID_INPUT};
                return false;
            }
            if (index_ >= argc_)
                return false;
            if (argv_[index_] == nullptr)
            {
                error = {
                    EErrorCode::INVALID_INPUT,
                    no_option,
                    static_cast<std::size_t>(index_),
                    ELocationKind::ARGUMENT_INDEX
                };
                return false;
            }
            out = {argv_[index_], static_cast<std::size_t>(index_), ELocationKind::ARGUMENT_INDEX, false};
            ++index_;
            return true;
        }

    private:
        int argc_;
        const char* const* argv_;
        int index_ = 1;
    };

} // namespace lux::cxx::args
