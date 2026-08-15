#pragma once
/*
 * Archive contracts — the small primitive surface each backend implements.
 *
 *   - OutputArchive: a streaming sink (begin/key/value/end), built top-down.
 *   - InputArchive : a random-access cursor over the backend's already-parsed
 *     representation (member by key, element by index, scalar read).
 *
 * The surface is intentionally a FIXED, non-template overload set (bool / i64 /
 * u64 / double / string_view). That lets a backend hide its third-party types
 * (nlohmann_json, tinyxml2, …) entirely inside a .cpp — the public header leaks
 * none of them, so it can never clash with a consumer's own copy. The generic
 * traversal (`save`/`load` in core.hpp) narrows arbitrary scalars onto this set.
 */
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <lux/cxx/core/function_ref.hpp>

namespace lux::cxx::ser
{
    // Serialization keeps this namespace-level name as a source-compatible alias
    // while the implementation is shared with the core component.
    template<class Signature>
    using function_ref = ::lux::cxx::function_ref<Signature>;

    template <class A>
    concept OutputArchive = requires(A a, std::string_view k,
                                     std::int64_t i, std::uint64_t u, double d, bool b) {
        a.begin_object(std::size_t{}); a.key(k); a.end_object();
        a.begin_array(std::size_t{}); a.end_array();
        a.null_value();
        a.value(b); a.value(i); a.value(u); a.value(d); a.value(k);
    };

    template <class C>
    concept InputArchive = requires(
        const C c,
        std::string_view k,
        std::size_t idx,
        function_ref<void(std::string_view, const C&)> member_visitor,
        function_ref<void(std::size_t, const C&)> element_visitor
    ) {
        { static_cast<bool>(c) } -> std::same_as<bool>;
        { c.is_null() }   -> std::convertible_to<bool>;
        { c.is_object() } -> std::convertible_to<bool>;
        { c.is_array() }  -> std::convertible_to<bool>;
        { c.member(k) }   -> std::same_as<C>;
        { c.size() }      -> std::convertible_to<std::size_t>;
        { c.element(idx) }-> std::same_as<C>;
        { c.member_count() }   -> std::convertible_to<std::size_t>;
        { c.member_key(idx) }  -> std::convertible_to<std::string_view>;
        { c.member_value(idx) }-> std::same_as<C>;
        // Single-pass visitors — O(n) traversal of an object's members / an array's
        // elements. Prefer these over the indexed accessors above for whole-container
        // load: on backends whose native representation is a forward sequence (XML
        // child list, ordered-map JSON object) the indexed form is O(n^2).
        c.for_each_member(member_visitor);
        c.for_each_element(element_visitor);
    } && requires(const C c, bool b, std::int64_t i, std::uint64_t u, double d, std::string s) {
        { c.read(b) } -> std::convertible_to<bool>;
        { c.read(i) } -> std::convertible_to<bool>;
        { c.read(u) } -> std::convertible_to<bool>;
        { c.read(d) } -> std::convertible_to<bool>;
        { c.read(s) } -> std::convertible_to<bool>;
    };
} // namespace lux::cxx::ser
