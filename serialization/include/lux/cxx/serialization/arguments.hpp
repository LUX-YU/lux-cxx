#pragma once
/*
 * Arguments backend — bind a reflected struct to/from command-line options via
 * the project's own lux::cxx::Parser (no third-party type, so header-only).
 *
 * Unlike JSON/XML this is a FLAT target: nested structs are flattened into
 * dotted option names (--db.host, --db.port). It therefore has its own recursive
 * walk over meta_info rather than the streaming Archive, and reuses the Arguments
 * module's value_parser<T> for all scalar/sequence conversions (zero duplication).
 *
 *   from_args<T>(argc, argv)        -> result<T>
 *   from_args<T>(cmdline_string)    -> result<T>
 *   usage<T>(prog)                  -> std::string
 *   make_arg_parser<T>(prog)        -> lux::cxx::Parser   (pre-populated)
 *
 * Supported leaf fields: arithmetic, bool (flag), string, enum (as underlying
 * integer), std::optional<scalar>, and push_back sequences (vector/list/deque).
 * Nested serializable structs are flattened. Other shapes (maps, tuples,
 * containers of structs) are a compile error — they have no flat CLI form.
 */
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <lux/cxx/arguments/Arguments.hpp>
#include <lux/cxx/serialization/core.hpp>
#include <lux/cxx/serialization/error.hpp>

namespace lux::cxx::ser
{
    namespace detail
    {
        template <class T> struct is_std_optional : std::false_type {};
        template <class U> struct is_std_optional<std::optional<U>> : std::true_type {};

        inline std::string join_path(std::string_view prefix, std::string_view name)
        {
            if (prefix.empty()) return std::string(name);
            std::string s;
            s.reserve(prefix.size() + 1 + name.size());
            s.assign(prefix); s.push_back('.'); s.append(name);
            return s;
        }

        template <class FT>
        void register_leaf(lux::cxx::Parser& parser, const std::string& name,
                           const std::string& short_name, const field_options& o, const FT& defval)
        {
            if constexpr (ReflectedEnum<FT>)
            {
                auto b = parser.add<std::string>(name, short_name);   // accept enum NAME on the CLI
                if (o.required) b.required(); else b.def(std::string(enum_to_string(defval)));
            }
            else if constexpr (std::is_enum_v<FT>)
            {
                using U = std::underlying_type_t<FT>;
                auto b = parser.add<U>(name, short_name);
                if (o.required) b.required(); else b.def(static_cast<U>(defval));
            }
            else if constexpr (is_std_optional<FT>::value)
            {
                using X = typename FT::value_type;
                auto b = parser.add<X>(name, short_name);   // optional => never required
                if (defval.has_value()) b.def(*defval);
            }
            else if constexpr (lux::cxx::Sequence<FT>)
            {
                auto b = parser.add<FT>(name, short_name);
                b.multi();
                if (o.required) b.required();               // no def() — builder can't take a sequence default
            }
            else if constexpr (std::same_as<FT, bool>)
            {
                auto b = parser.add<bool>(name, short_name);
                if (o.required) b.required(); else b.def(defval);
            }
            else if constexpr (lux::cxx::Arithmetic<FT> || lux::cxx::StringLike<FT>)
            {
                auto b = parser.add<FT>(name, short_name);
                if (o.required) b.required(); else b.def(defval);
            }
            else
            {
                static_assert(always_false<FT>,
                    "Arguments backend: unsupported leaf field type — provide a "
                    "lux::cxx::value_parser<FT> or make the type a serializable struct.");
            }
        }

        template <class FT>
        bool read_leaf(const lux::cxx::ParsedOptions& opts, const std::string& name, FT& out)
        {
            if (!opts.contains(name)) return true;          // absent (no value, no default) -> keep default
            const auto ref = opts.get(name);
            if constexpr (ReflectedEnum<FT>)
            {
                auto s = ref.template as<std::string>();          // try enum name first
                if (s && enum_from_string(*s, out)) return true;
                auto u = ref.template as<std::underlying_type_t<FT>>();  // fall back to integer
                if (!u) return false;
                out = static_cast<FT>(*u);
                return true;
            }
            else if constexpr (std::is_enum_v<FT>)
            {
                auto v = ref.template as<std::underlying_type_t<FT>>();
                if (!v) return false;
                out = static_cast<FT>(*v);
                return true;
            }
            else if constexpr (is_std_optional<FT>::value)
            {
                using X = typename FT::value_type;
                auto v = ref.template as<X>();
                if (!v) return false;
                out = *v;
                return true;
            }
            else
            {
                auto v = ref.template as<FT>();
                if (!v) return false;
                out = std::move(*v);
                return true;
            }
        }
    } // namespace detail

    // Recursively register one parser option per leaf field (dotted, flattened).
    template <class T>
    void register_args(lux::cxx::Parser& parser, const T& defaults, std::string prefix = "")
    {
        static_assert(Reflected<T>, "register_args requires a LUX_META(serializable) type");
        meta_info<T>::for_each_field([&](auto fd) {
            if (fd.options.skip) return;
            using FT = std::remove_cvref_t<typename decltype(fd)::member_type>;
            const auto& fval = defaults.*(fd.pointer);
            if constexpr (Reflected<FT>)
            {
                register_args(parser, fval, fd.options.flatten ? prefix : detail::join_path(prefix, fd.name));
            }
            else
            {
                const std::string name = detail::join_path(prefix, fd.name);
                const std::string sn   = prefix.empty() ? std::string(fd.options.short_name) : std::string{};
                detail::register_leaf<FT>(parser, name, sn, fd.options, fval);
            }
        });
    }

    // Recursively bind parsed options back into a reflected T.
    template <class T>
    bool read_args(const lux::cxx::ParsedOptions& opts, T& out, std::string prefix = "")
    {
        static_assert(Reflected<T>, "read_args requires a LUX_META(serializable) type");
        bool ok = true;
        meta_info<T>::for_each_field([&](auto fd) {
            if (fd.options.skip) return;
            using FT = std::remove_cvref_t<typename decltype(fd)::member_type>;
            auto& fref = out.*(fd.pointer);
            if constexpr (Reflected<FT>)
            {
                if (!read_args(opts, fref, fd.options.flatten ? prefix : detail::join_path(prefix, fd.name))) ok = false;
            }
            else
            {
                if (!detail::read_leaf<FT>(opts, detail::join_path(prefix, fd.name), fref)) ok = false;
            }
        });
        return ok;
    }

    template <class T>
    lux::cxx::Parser make_arg_parser(std::string prog)
    {
        T defaults{};
        lux::cxx::Parser parser(std::move(prog));
        register_args(parser, defaults);
        return parser;
    }

    template <class T>
    std::string usage(std::string prog)
    {
        T defaults{};
        lux::cxx::Parser parser(std::move(prog));
        register_args(parser, defaults);
        return parser.usage();
    }

    template <class T>
    result<T> from_args(int argc, char** argv, std::string prog = "app")
    {
        T out{};
        lux::cxx::Parser parser(std::move(prog));
        register_args(parser, out);
        auto parsed = parser.parse(argc, argv);
        if (!parsed) return make_error(error_code::load_failed, std::string(lux::cxx::to_string(parsed.error())));
        if (!read_args(parsed.value(), out)) return make_error(error_code::load_failed, "argument binding failed");
        return out;
    }

    template <class T>
    result<T> from_args(const std::string& cmdline, std::string prog = "app")
    {
        T out{};
        lux::cxx::Parser parser(prog);
        register_args(parser, out);
        auto parsed = parser.parse(prog + " " + cmdline);   // prepend argv0
        if (!parsed) return make_error(error_code::load_failed, std::string(lux::cxx::to_string(parsed.error())));
        if (!read_args(parsed.value(), out)) return make_error(error_code::load_failed, "argument binding failed");
        return out;
    }
} // namespace lux::cxx::ser
