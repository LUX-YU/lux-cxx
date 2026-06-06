#pragma once
/*
 * Backend-agnostic serialization core.
 *
 *   L1  type classification  (the concepts below)
 *   L2  the *only* traversal  (save / load), dispatching by L1, recursing
 *
 * Backends provide the archive primitives (see archive.hpp); they never walk
 * an object themselves. New type support = one branch here or a serializer<T>
 * specialisation (the non-intrusive customisation point). Arbitrary arithmetic
 * is narrowed onto the archive's fixed scalar overloads (bool/i64/u64/double).
 */
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <lux/cxx/serialization/meta_info.hpp>

namespace lux::cxx::ser
{
    template <class>
    inline constexpr bool always_false = false;

    // ---- customisation point: specialise + set is_specialized = true ----------
    template <class T>
    struct serializer;   // primary left undefined

    template <class T>
    concept HasCustom = requires { serializer<std::remove_cvref_t<T>>::is_specialized; };

    // ---- L1: type classification ---------------------------------------------
    template <class T> concept BoolLike   = std::same_as<std::remove_cvref_t<T>, bool>;
    template <class T> concept Arithmetic = std::is_arithmetic_v<std::remove_cvref_t<T>> && !BoolLike<T>;
    template <class T> concept EnumType   = std::is_enum_v<std::remove_cvref_t<T>>;

    template <class T>
    concept StringLike =
        std::same_as<std::remove_cvref_t<T>, std::string> ||
        std::same_as<std::remove_cvref_t<T>, std::string_view> ||
        std::same_as<std::decay_t<T>, char*> ||
        std::same_as<std::decay_t<T>, const char*>;

    namespace detail
    {
        template <class T> struct nullable_traits;
        template <class U> struct nullable_traits<std::optional<U>>
        { using elem = U; static void assign(std::optional<U>& o, U v) { o.emplace(std::move(v)); } };
        template <class U, class D> struct nullable_traits<std::unique_ptr<U, D>>
        { using elem = U; static void assign(std::unique_ptr<U, D>& o, U v) { o = std::make_unique<U>(std::move(v)); } };
        template <class U> struct nullable_traits<std::shared_ptr<U>>
        { using elem = U; static void assign(std::shared_ptr<U>& o, U v) { o = std::make_shared<U>(std::move(v)); } };

        template <class T> struct is_nullable : std::false_type {};
        template <class U> struct is_nullable<std::optional<U>> : std::true_type {};
        template <class U, class D> struct is_nullable<std::unique_ptr<U, D>> : std::true_type {};
        template <class U> struct is_nullable<std::shared_ptr<U>> : std::true_type {};

        template <class C, class V>
        void seq_insert(C& c, V&& v)
        {
            if constexpr (requires { c.push_back(std::forward<V>(v)); })   c.push_back(std::forward<V>(v));
            else if constexpr (requires { c.insert(std::forward<V>(v)); }) c.insert(std::forward<V>(v));
            else static_assert(always_false<C>, "sequence type has no push_back/insert");
        }

        // -- base64 (used for byte sequences) ----------------------------------
        inline constexpr char b64_tab[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        template <class Bytes>
        std::string base64_encode(const Bytes& in)
        {
            std::string out;
            out.reserve(((in.size() + 2) / 3) * 4);
            std::size_t i = 0;
            const std::size_t n = in.size();
            auto byte_at = [&](std::size_t k) { return static_cast<unsigned>(static_cast<unsigned char>(in[k])); };
            for (; i + 3 <= n; i += 3)
            {
                const unsigned v = (byte_at(i) << 16) | (byte_at(i + 1) << 8) | byte_at(i + 2);
                out.push_back(b64_tab[(v >> 18) & 63]); out.push_back(b64_tab[(v >> 12) & 63]);
                out.push_back(b64_tab[(v >> 6) & 63]);  out.push_back(b64_tab[v & 63]);
            }
            if (n - i == 1)
            {
                const unsigned v = byte_at(i) << 16;
                out.push_back(b64_tab[(v >> 18) & 63]); out.push_back(b64_tab[(v >> 12) & 63]);
                out.push_back('='); out.push_back('=');
            }
            else if (n - i == 2)
            {
                const unsigned v = (byte_at(i) << 16) | (byte_at(i + 1) << 8);
                out.push_back(b64_tab[(v >> 18) & 63]); out.push_back(b64_tab[(v >> 12) & 63]);
                out.push_back(b64_tab[(v >> 6) & 63]);  out.push_back('=');
            }
            return out;
        }

        inline int b64_val(char c)
        {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        }

        template <class Bytes>
        bool base64_decode(std::string_view s, Bytes& out)
        {
            using V = std::ranges::range_value_t<Bytes>;
            out.clear();
            int buf = 0, bits = 0;
            for (char c : s)
            {
                if (c == '=' || c == '\n' || c == '\r' || c == ' ') continue;
                const int d = b64_val(c);
                if (d < 0) return false;
                buf = (buf << 6) | d;
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    out.push_back(static_cast<V>(static_cast<unsigned char>((buf >> bits) & 0xFF)));
                }
            }
            return true;
        }

        // Narrow any arithmetic type onto the archive's fixed scalar overloads.
        template <class Ar, class V>
        void put_arith(Ar& ar, V v)
        {
            if constexpr (std::is_floating_point_v<V>)   ar.value(static_cast<double>(v));
            else if constexpr (std::is_signed_v<V>)      ar.value(static_cast<std::int64_t>(v));
            else                                         ar.value(static_cast<std::uint64_t>(v));
        }
        template <class Cur, class V>
        bool get_arith(const Cur& c, V& out)
        {
            if constexpr (std::is_floating_point_v<V>) { double v{};        if (!c.read(v)) return false; out = static_cast<V>(v); return true; }
            else if constexpr (std::is_signed_v<V>)    { std::int64_t v{};  if (!c.read(v)) return false; out = static_cast<V>(v); return true; }
            else                                       { std::uint64_t v{}; if (!c.read(v)) return false; out = static_cast<V>(v); return true; }
        }
    } // namespace detail

    template <class T> concept Nullable = detail::is_nullable<std::remove_cvref_t<T>>::value;

    template <class T>
    concept TupleLike =
        !std::is_array_v<std::remove_cvref_t<T>> &&
        requires { std::tuple_size<std::remove_cvref_t<T>>::value; };

    template <class T>
    concept Map =
        std::ranges::input_range<T> &&
        requires {
            typename std::remove_cvref_t<T>::key_type;
            typename std::remove_cvref_t<T>::mapped_type;
        };

    template <class T>
    concept StringKeyMap = Map<T> &&
        std::same_as<typename std::remove_cvref_t<T>::key_type, std::string>;

    // std::vector<std::byte> (and the like) -> base64 string, automatically.
    template <class T>
    concept ByteSequence =
        std::ranges::input_range<T> &&
        std::same_as<std::ranges::range_value_t<T>, std::byte> &&
        requires(T c) { c.clear(); c.push_back(std::byte{}); };

    template <class T>
    concept Sequence =
        std::ranges::input_range<T> && !StringLike<T> && !Map<T> && !TupleLike<T> && !ByteSequence<T>;

    // ---- L2: serialize (the single traversal) --------------------------------
    template <class Ar, class T>
    void save(Ar& ar, const T& v)
    {
        using U = std::remove_cvref_t<T>;
        if constexpr (HasCustom<U>)
        {
            serializer<U>::save(ar, v);
        }
        else if constexpr (Reflected<U>)
        {
            ar.begin_object(field_count_v<U>);
            meta_info<U>::for_each_field([&](auto fd) {
                if (fd.options.skip) return;
                ar.key(fd.name, fd.options.xml_attribute);
                save(ar, v.*(fd.pointer));
            });
            ar.end_object();
        }
        else if constexpr (StringLike<U>)   { ar.value(std::string_view{ v }); }
        else if constexpr (BoolLike<U>)     { ar.value(static_cast<bool>(v)); }
        else if constexpr (ReflectedEnum<U>){ ar.value(enum_to_string(v)); }                              // by name
        else if constexpr (EnumType<U>)     { detail::put_arith(ar, static_cast<std::underlying_type_t<U>>(v)); } // by integer
        else if constexpr (Arithmetic<U>)   { detail::put_arith(ar, v); }
        else if constexpr (Nullable<U>)
        {
            if (static_cast<bool>(v)) save(ar, *v);
            else                      ar.null_value();
        }
        else if constexpr (ByteSequence<U>)
        {
            ar.value(std::string_view{ detail::base64_encode(v) });
        }
        else if constexpr (TupleLike<U>)
        {
            ar.begin_array(std::tuple_size_v<U>);
            std::apply([&](auto const&... xs) { (save(ar, xs), ...); }, v);
            ar.end_array();
        }
        else if constexpr (StringKeyMap<U>)
        {
            ar.begin_object(v.size());
            for (const auto& [k, val] : v) { ar.key(std::string_view{ k }); save(ar, val); }
            ar.end_object();
        }
        else if constexpr (Map<U>)
        {
            ar.begin_array(v.size());
            for (const auto& [k, val] : v) { ar.begin_array(2); save(ar, k); save(ar, val); ar.end_array(); }
            ar.end_array();
        }
        else if constexpr (Sequence<U>)
        {
            std::size_t n = 0;
            if constexpr (std::ranges::sized_range<U>) n = std::ranges::size(v);
            ar.begin_array(n);
            for (const auto& e : v) save(ar, e);
            ar.end_array();
        }
        else
        {
            static_assert(always_false<U>,
                "lux::cxx::ser::save: no serializer for T — specialise ser::serializer<T> "
                "or generate meta_info<T> (LUX_META(serializable)).");
        }
    }

    // ---- L2: deserialize ------------------------------------------------------
    template <class Cur, class T>
    bool load(const Cur& cur, T& out)
    {
        using U = std::remove_cvref_t<T>;
        if constexpr (HasCustom<U>)
        {
            return serializer<U>::load(cur, out);
        }
        else if constexpr (Reflected<U>)
        {
            if (!cur.is_object()) return false;
            bool ok = true;
            meta_info<U>::for_each_field([&](auto fd) {
                if (fd.options.skip) return;
                const auto child = cur.member(fd.name, fd.options.xml_attribute);
                if (child) { if (!load(child, out.*(fd.pointer))) ok = false; }
                else if (fd.options.required) ok = false;
            });
            return ok;
        }
        else if constexpr (BoolLike<U>)               { return cur.read(out); }
        else if constexpr (std::same_as<U, std::string>) { return cur.read(out); }
        else if constexpr (ReflectedEnum<U>)
        {
            std::string s;
            if (cur.read(s) && enum_from_string(s, out)) return true;   // by name
            std::underlying_type_t<U> u{};                              // tolerate legacy integer form
            if (detail::get_arith(cur, u)) { out = static_cast<U>(u); return true; }
            return false;
        }
        else if constexpr (EnumType<U>)
        {
            std::underlying_type_t<U> u{};
            if (!detail::get_arith(cur, u)) return false;
            out = static_cast<U>(u);
            return true;
        }
        else if constexpr (Arithmetic<U>) { return detail::get_arith(cur, out); }
        else if constexpr (Nullable<U>)
        {
            if (cur.is_null()) { out = U{}; return true; }
            typename detail::nullable_traits<U>::elem tmp{};
            if (!load(cur, tmp)) return false;
            detail::nullable_traits<U>::assign(out, std::move(tmp));
            return true;
        }
        else if constexpr (ByteSequence<U>)
        {
            std::string s;
            if (!cur.read(s)) return false;
            return detail::base64_decode(s, out);
        }
        else if constexpr (TupleLike<U>)
        {
            if (!cur.is_array() || cur.size() < std::tuple_size_v<U>) return false;
            bool ok = true;
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                ((ok = load(cur.element(I), std::get<I>(out)) && ok), ...);
            }(std::make_index_sequence<std::tuple_size_v<U>>{});
            return ok;
        }
        else if constexpr (StringKeyMap<U>)
        {
            if (!cur.is_object()) return false;
            out.clear();
            using V = typename U::mapped_type;
            bool ok = true;
            const std::size_t n = cur.member_count();
            for (std::size_t i = 0; i < n; ++i)
            {
                V val{};
                if (load(cur.member_value(i), val)) out.emplace(std::string(cur.member_key(i)), std::move(val));
                else ok = false;
            }
            return ok;
        }
        else if constexpr (Map<U>)
        {
            if (!cur.is_array()) return false;
            out.clear();
            using K = typename U::key_type;
            using V = typename U::mapped_type;
            bool ok = true;
            for (std::size_t i = 0; i < cur.size(); ++i)
            {
                const auto e = cur.element(i);
                if (e.size() < 2) { ok = false; continue; }
                K k{}; V val{};
                if (load(e.element(0), k) && load(e.element(1), val)) out.emplace(std::move(k), std::move(val));
                else ok = false;
            }
            return ok;
        }
        else if constexpr (Sequence<U>)
        {
            if (!cur.is_array()) return false;
            out.clear();
            using V = std::ranges::range_value_t<U>;
            bool ok = true;
            for (std::size_t i = 0; i < cur.size(); ++i)
            {
                V e{};
                if (load(cur.element(i), e)) detail::seq_insert(out, std::move(e));
                else ok = false;
            }
            return ok;
        }
        else
        {
            static_assert(always_false<U>,
                "lux::cxx::ser::load: no deserializer for T — specialise ser::serializer<T> "
                "or generate meta_info<T> (LUX_META(serializable)).");
            return false;
        }
    }
} // namespace lux::cxx::ser
