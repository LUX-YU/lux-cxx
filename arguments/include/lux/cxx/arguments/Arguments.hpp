/*************************************************
*  argument parser ‑ exception‑free ver.
*  Author : Chenhui Yu, 2025  (refactored 2025‑06)
**************************************************/
#pragma once
#include <charconv>
#include <concepts>
#include <cctype>
#include <cstdint>
#include <functional>
#include <optional>
#include <ranges>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <cassert>

#include <lux/cxx/compile_time/expected.hpp>

namespace lux::cxx
{
    // ---------- transparent hash / equal ----------------------------------------
    using sv = std::string_view;

    struct sv_hash {
        using is_transparent = void;
        std::size_t operator()(sv s)                   const noexcept { return std::hash<sv>{}(s); }
        std::size_t operator()(const std::string& s)   const noexcept { return std::hash<sv>{}(s); }
    };
    using sv_equal = std::equal_to<>;

    using token_id = std::uint32_t;

    // ---------- error code -------------------------------------------------------
    enum class errc {
        ok = 0,
        invalid_integer, invalid_float, invalid_boolean,
        unknown_option, value_missing, missing_required_option, no_tokens,
        option_not_present,
    };

    constexpr std::string_view to_string(errc e) noexcept
    {
        using enum errc;
        switch (e) {
        case ok:                        return "no error";
        case invalid_integer:           return "invalid integer value";
        case invalid_float:             return "invalid float value";
        case invalid_boolean:           return "invalid boolean value";
        case unknown_option:            return "unknown option";
        case value_missing:             return "value missing for option";
        case missing_required_option:   return "missing required option";
        case no_tokens:                 return "no command tokens";
        case option_not_present:        return "option not present";
        }
        return "unknown error";
    }

    template <typename T>
    using expected_t = lux::cxx::expected<T, errc>;

    // ---------- concepts ---------------------------------------------------------
    template <typename T>
    concept Arithmetic = std::integral<T> || std::floating_point<T>;

    template <typename T>
    concept StringLike =
        std::same_as<std::remove_cvref_t<T>, std::string> ||
        std::same_as<std::remove_cvref_t<T>, sv>;

    template <typename T>
    concept Sequence =
        std::ranges::input_range<T> && (!StringLike<T>) &&
        requires(T c, typename T::value_type v) { c.push_back(v); };

    // ---------- value_parser (extensible) ---------------------------------------
    template <typename T, typename = void> struct value_parser;

    /* arithmetic */
    template <Arithmetic T>
    struct value_parser<T>
    {
        static expected_t<T> parse(sv s)
        {
            T v{};
            if constexpr (std::integral<T>)
            {
                auto r = std::from_chars(s.data(), s.data() + s.size(), v);
                if (r.ec != std::errc{}) return lux::cxx::unexpected(errc::invalid_integer);
            }
            else
            {
                auto r = std::from_chars(s.data(), s.data() + s.size(), v);
                if (r.ec != std::errc{}) return lux::cxx::unexpected(errc::invalid_float);
            }
            return v;
        }
    };

    /* bool */
    template <> struct value_parser<bool>
    {
        static expected_t<bool> parse(sv s)
        {
            if (s.empty() || s == "1" || s == "true" || s == "yes")  return true;
            if (s == "0" || s == "false" || s == "no")               return false;
            return lux::cxx::unexpected(errc::invalid_boolean);
        }
    };

    /* string */
    template <StringLike T>
    struct value_parser<T> { 
        static expected_t<T> parse(sv s) noexcept { return T{ s }; } 
    };

    /* sequences */
    template <Sequence Seq>
    struct value_parser<Seq>
    {
        using elem_t = typename Seq::value_type;
        static expected_t<Seq> parse(std::span<const sv> parts)
        {
            Seq seq{};
            for (sv t : parts) {
                auto r = value_parser<elem_t>::parse(t);
                if (!r) return lux::cxx::unexpected(r.error());
                seq.push_back(*r);
            }
            return seq;
        }
    };

    // ---------- option_spec ------------------------------------------------------
    struct option_spec
    {
        std::string              long_name, short_name, description;
        bool                     required = false;
        bool                     multi_value = false;
        bool                     is_flag = false;
        std::vector<std::string> defaults;
    };

    class ParsedOptions;            // fwd‑decl

    // ---------- Parser -----------------------------------------------------------
    class Parser
    {
    public:
        explicit Parser(std::string prog, bool allow_unknown = false)
            : prog_(std::move(prog)), allow_unknown_(allow_unknown) {
        }
        Parser() : prog_(""), allow_unknown_(false) {
        }

        // ---- builder -----------------------------------------------------------
        template <typename T>
        class builder
        {
        public:
            explicit builder(option_spec& s) : spec_(s) {}
            builder& desc(std::string d) { spec_.description = std::move(d);  return *this; }
            builder& required(bool v = true) { spec_.required = v;            return *this; }
            builder& multi(bool v = true) { spec_.multi_value = v;            return *this; }

            builder& def(const T& v) { add_one(v); return *this; }
            builder& def(const Sequence auto& seq)
                requires std::same_as<T, decltype(seq)>
            {
                spec_.defaults.reserve(spec_.defaults.size() + std::ranges::size(seq));
                for (auto&& e : seq) add_one(e);
                return *this;
            }

        private:
            option_spec& spec_;
            template<typename X>
            static std::string to_str(const X& x)
            {
                if constexpr (StringLike<X>)      return std::string{ x };
                else if constexpr (Arithmetic<X>) return std::to_string(x);
                else                              return "<custom>";
            }
            template<typename X>
            void add_one(const X& v) { spec_.defaults.emplace_back(to_str(v)); }
        };

        template <typename T>
        builder<T> add(std::string long_name, std::string short_name = {})
        {
            // 重复注册是编程期错误，直接 assert（不进入运行时 errc）
            assert(!specs_.contains(long_name) && "duplicate option long name");
            option_spec s;
            s.long_name = std::move(long_name);
            s.short_name = std::move(short_name);
            s.is_flag = std::same_as<T, bool>;

            auto& ref = specs_.emplace(s.long_name, std::move(s)).first->second;
            if (!ref.short_name.empty()) short2long_[ref.short_name] = ref.long_name;
            return builder<T>{ ref };
        }

        expected_t<ParsedOptions> parse(int argc, char* argv[])      const;
        expected_t<ParsedOptions> parse(const std::string& cmdline) const;

        std::string         usage()      const;
        const std::string& prog_name()  const noexcept { return prog_; }

    private:
        expected_t<ParsedOptions> parse_tokens(std::vector<std::string>&& toks) const;

        using spec_map = std::unordered_map<std::string, option_spec, sv_hash, sv_equal>;
        using s2l_map = std::unordered_map<std::string, std::string, sv_hash, sv_equal>;

        std::string prog_;
        bool        allow_unknown_;
        spec_map    specs_;
        s2l_map     short2long_;
    };

    // ---------- ParsedOptions ----------------------------------------------------
    class ParsedOptions
    {
        using idx_map = std::unordered_map<std::string,
            std::vector<token_id>,
            sv_hash, sv_equal>;
        friend class Parser;

    public:
        bool contains(sv name) const noexcept { return indices_.contains(name); }

        class option_ref
        {
            friend class ParsedOptions;
        public:
            explicit operator bool() const noexcept { return present_; }

            template<typename T>
            expected_t<T> as() const
            {
                if (!present_) return lux::cxx::unexpected(errc::option_not_present);

                if constexpr (std::same_as<T, bool>)
                {
                    if (idxs_->empty())                     // 纯 flag，无显式值
                        return true;
                    return value_parser<bool>::parse((*pool_)[idxs_->front()]);
                }
                else if constexpr (Sequence<T>)
                {
                    std::vector<sv> views; views.reserve(idxs_->size());
                    for (auto id : *idxs_) views.emplace_back((*pool_)[id]);
                    return value_parser<T>::parse(std::span<const sv>(views));
                }
                else
                {
                    if (idxs_->empty()) return lux::cxx::unexpected(errc::value_missing);
                    return value_parser<T>::parse((*pool_)[idxs_->front()]);
                }
            }

        private:
            option_ref(const std::vector<token_id>* v,
                const std::vector<std::string>* pool,
                bool p)
                : idxs_(v), pool_(pool), present_(p) {
            }

            const std::vector<token_id>* idxs_{};
            const std::vector<std::string>* pool_{};
            bool                                     present_{ false };
        };

        option_ref get(sv name) const
        {
            auto it = indices_.find(name);
            bool pr = it != indices_.end();
            const std::vector<token_id>* vec = pr ? &it->second : nullptr;
            return option_ref{ vec, &pool_, pr };
        }

    private:
        ParsedOptions(std::vector<std::string>&& p, idx_map&& m)
            : pool_(std::move(p)), indices_(std::move(m)) {
        }

        std::vector<std::string>  pool_;
        idx_map                   indices_;
    };

    // ===== implementation ========================================================

    inline expected_t<ParsedOptions> Parser::parse(int argc, char* argv[]) const
    {
        std::vector<std::string> toks; toks.reserve(static_cast<std::size_t>(argc));
        for (int i = 0; i < argc; ++i) toks.emplace_back(argv[i]);
        return parse_tokens(std::move(toks));
    }

    inline expected_t<ParsedOptions> Parser::parse(const std::string& cmdline) const
    {
        std::vector<std::string> toks;
        std::string cur; 
        bool in_q = false;
        
        auto flush = [&] { 
            toks.push_back(std::move(cur)); 
            cur.clear(); 
        };
        
        for (char ch : cmdline) {
            if (ch == '"') { 
                in_q = !in_q; 
                continue; 
            }
            if (std::isspace(static_cast<unsigned char>(ch)) && !in_q) {
                if (!cur.empty()) flush();
            } else {
                cur.push_back(ch);
            }
        } 
        if (!cur.empty()) flush();
        
        if (toks.empty()) toks.emplace_back(prog_);
        return parse_tokens(std::move(toks));
    }

    // ------------- usage() -------------------------------------------------------
    inline std::string Parser::usage() const
    {
        constexpr sv indent = "  ";
        std::ostringstream os;
        os << "Usage: " << prog_ << " [options]\n\nOptions:\n";
        for (auto const& [_, s] : specs_) {
            os << indent;
            if (!s.short_name.empty()) os << '-' << s.short_name << ", ";
            os << "--" << s.long_name;
            int pad = 24 - int(s.long_name.size() + (s.short_name.empty() ? 2 : 4));
            if (pad > 0) os << std::string(pad, ' ');
            os << s.description;
            if (s.required)           os << " (required)";
            if (!s.defaults.empty())  os << " [default: " << s.defaults.front() << ']';
            os << '\n';
        }
        os << indent << "-h, --help              Show this help and exit\n";
        return os.str();
    }

    // ------------- parse_tokens() -----------------------------------------------
    inline expected_t<ParsedOptions> Parser::parse_tokens(std::vector<std::string>&& pool) const
    {
        if (pool.empty()) return lux::cxx::unexpected(errc::no_tokens);

        ParsedOptions::idx_map idxs;
        idxs.reserve(specs_.size());

        // ---------- helpers -----------------------------------------------------
        auto push_token = [&](sv s) -> token_id {
            pool.emplace_back(s);                         // 可能触发 reallocation
            return static_cast<token_id>(pool.size() - 1);
            };

        auto add_index = [&](const auto& key, token_id id)
            {
                auto it = idxs.find(key);
                if (it == idxs.end())
                    it = idxs.emplace(std::string{ key }, std::vector<token_id>{}).first;
                it->second.push_back(id);
            };

        auto mark_flag_present = [&](const auto& key)
            {
                if (!idxs.contains(key))
                    idxs.emplace(std::string{ key }, std::vector<token_id>{});
            };

        // ---- 判断 token 是否为已注册选项 --------------------------------------
        auto looks_option = [&](sv tok) -> bool {
            if (!tok.starts_with('-')) return false;

            bool longf = tok.starts_with("--");
            sv   key = tok.substr(longf ? 2 : 1);

            if (auto eq = key.find('='); eq != sv::npos)
                key = key.substr(0, eq);

            if (!longf)
                if (auto it = short2long_.find(key); it != short2long_.end())
                    key = it->second;

            return specs_.contains(key);
            };

        const std::size_t n_orig = pool.size();   // 初始 token 数

        for (std::size_t i = 1; i < n_orig; ++i)
        {
            sv tok = pool[i];
            if (tok == "-h" || tok == "--help") continue;
            if (!tok.starts_with('-'))           continue;

            bool longf = tok.starts_with("--");
            
            // 添加边界检查以避免越界
            if ((longf && tok.size() <= 2) || (!longf && tok.size() <= 1)) {
                continue; // 跳过无效的选项（如单独的 "-" 或 "--"）
            }
            
            sv key = tok.substr(longf ? 2 : 1);

            sv inline_val{};
            bool has_inline_val = false;
            if (auto eq = key.find('='); eq != sv::npos) {
                inline_val = key.substr(eq + 1);
                key = key.substr(0, eq);
                has_inline_val = true;
            }

            if (!longf)
                if (auto it = short2long_.find(key); it != short2long_.end())
                    key = it->second;

            auto spec_it = specs_.find(key);
            if (spec_it == specs_.end()) {
                if (!allow_unknown_) return lux::cxx::unexpected(errc::unknown_option);
                continue;
            }
            const option_spec& spec = spec_it->second;

            // ---- 带内联值 -----------------------------------------------------
            if (has_inline_val)
            {
                token_id id = push_token(inline_val);
                add_index(key, id);
                continue;
            }

            // ---- flag（无值） --------------------------------------------------
            if (spec.is_flag) { mark_flag_present(key); continue; }

            // ---- 普通 / multi-value 选项 --------------------------------------
            std::size_t j = i + 1;
            while (j < n_orig && !looks_option(pool[j]))
            {
                add_index(key, static_cast<token_id>(j));
                ++j;
                if (!spec.multi_value) break;
            }
            auto idx_it = idxs.find(key);
            if (idx_it == idxs.end() || idx_it->second.empty())
                return lux::cxx::unexpected(errc::value_missing);

            i = j - 1;      // for‑loop 末尾 ++i
        }

        // ---- 处理缺省值 / required -------------------------------------------
        for (auto const& [name, spec] : specs_)
        {
            if (!idxs.contains(name))
            {
                if (!spec.defaults.empty())
                    for (auto const& d : spec.defaults)
                        add_index(name, push_token(d));
                else if (spec.required)
                    return lux::cxx::unexpected(errc::missing_required_option);
            }
        }
        return ParsedOptions{ std::move(pool), std::move(idxs) };
    }

} // namespace lux::cxx
