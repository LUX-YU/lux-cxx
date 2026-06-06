#pragma once
/*
 * lux::cxx::ser::meta_info<T>
 *
 * The compile-time serialization contract. Unlike the reflection module's
 * demo `test_meta<T>`, `meta_info<T>` is purpose-built for (de)serialization:
 * each reflected field is exposed as a `field_descriptor` carrying
 *   - the serialized key (member name, or a `ser name=` override),
 *   - a member-object pointer for type-safe read/write access, and
 *   - per-field `field_options` parsed from `LUX_META(serializable, ...)`.
 *
 * Full specialisations are emitted by the code generator using
 * `templates/serializable.template.inja`. The primary template below is the
 * "no metadata" fallback.
 */
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace lux::cxx::ser
{
    /// Per-field options, resolved from `LUX_META(serializable, ...)` at codegen time.
    struct field_options
    {
        bool             skip          = false;  ///< serializable, skip          — never (de)serialize this field
        bool             required      = false;  ///< serializable, required      — error if missing on load
        bool             flatten       = false;  ///< serializable, flatten       — inline nested object/flatten path
        bool             omit_empty    = false;  ///< serializable, omit_empty    — skip emitting empty values
        bool             xml_attribute = false;  ///< serializable, xml_attribute — XML: emit scalar as attribute, not element
        std::string_view short_name    = {};     ///< serializable, short="p"     — short option name (Arguments backend)
    };

    /// One reflected field: serialized key + member pointer (read/write) + options.
    template <class Class, class Member>
    struct field_descriptor
    {
        std::string_view  name;     ///< serialization key
        Member Class::*   pointer;  ///< &Class::member
        field_options     options;

        using class_type  = Class;
        using member_type = Member;
    };

    /// Primary template. An unspecialised `meta_info<T>` means "T has no
    /// serialization metadata". The generator emits full specialisations.
    template <class T>
    struct meta_info
    {
        static constexpr bool reflected = false;
    };

    /// True when `T` has generated serialization metadata.
    template <class T>
    concept Reflected = meta_info<std::remove_cvref_t<T>>::reflected;

    /// Number of reflected fields of `T` (computed from `for_each_field`).
    template <Reflected T>
    inline constexpr std::size_t field_count_v = []
    {
        std::size_t n = 0;
        meta_info<std::remove_cvref_t<T>>::for_each_field([&n](auto const&) { ++n; });
        return n;
    }();

    // ------------------------------------------------------------------------
    // enum_meta<E> — name<->value table for an enum. Generated specialisations
    // provide `reflected`, `type`, `count`, and a constexpr `entries` array of
    // {name, value}. Unspecialised => the enum is serialized as its underlying
    // integer instead.
    // ------------------------------------------------------------------------
    template <class E>
    struct enum_meta
    {
        static constexpr bool reflected = false;
    };

    template <class E>
    concept ReflectedEnum =
        std::is_enum_v<std::remove_cvref_t<E>> &&
        enum_meta<std::remove_cvref_t<E>>::reflected;

    template <ReflectedEnum E>
    constexpr std::string_view enum_to_string(E v) noexcept
    {
        for (const auto& e : enum_meta<std::remove_cvref_t<E>>::entries)
            if (e.second == v) return e.first;
        return {};
    }

    template <ReflectedEnum E>
    constexpr bool enum_from_string(std::string_view s, E& out) noexcept
    {
        for (const auto& e : enum_meta<std::remove_cvref_t<E>>::entries)
            if (e.first == s) { out = e.second; return true; }
        return false;
    }

} // namespace lux::cxx::ser
