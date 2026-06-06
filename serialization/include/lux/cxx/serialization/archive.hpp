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

namespace lux::cxx::ser
{
    // ---- function_ref: a non-owning, allocation-free view of a callable --------
    // Used to pass a visitor lambda from the templated traversal (core.hpp) into a
    // backend's for_each_* method, whose body lives in a .cpp (so it cannot be a
    // template). Lifetime: the referenced callable must outlive the call — always
    // true here (the lambda is a local at the call site).
    template <class Sig> class function_ref;

    template <class R, class... Args>
    class function_ref<R(Args...)>
    {
    public:
        function_ref() noexcept = default;

        template <class F,
                  std::enable_if_t<!std::is_same_v<std::remove_cvref_t<F>, function_ref>
                                   && std::is_invocable_r_v<R, F&, Args...>, int> = 0>
        function_ref(F&& f) noexcept
            : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
              call_([](void* o, Args... a) -> R {
                  return (*static_cast<std::remove_reference_t<F>*>(o))(std::forward<Args>(a)...);
              })
        {}

        R operator()(Args... a) const { return call_(obj_, std::forward<Args>(a)...); }
        explicit operator bool() const noexcept { return call_ != nullptr; }

    private:
        void* obj_ = nullptr;
        R (*call_)(void*, Args...) = nullptr;
    };

    template <class A>
    concept OutputArchive = requires(A a, std::string_view k,
                                     std::int64_t i, std::uint64_t u, double d, bool b) {
        a.begin_object(std::size_t{}); a.key(k); a.end_object();
        a.begin_array(std::size_t{}); a.end_array();
        a.null_value();
        a.value(b); a.value(i); a.value(u); a.value(d); a.value(k);
    };

    template <class C>
    concept InputArchive = requires(const C c, std::string_view k, std::size_t idx) {
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
        c.for_each_member(function_ref<void(std::string_view, const C&)>{});
        c.for_each_element(function_ref<void(std::size_t, const C&)>{});
    } && requires(const C c, bool b, std::int64_t i, std::uint64_t u, double d, std::string s) {
        { c.read(b) } -> std::convertible_to<bool>;
        { c.read(i) } -> std::convertible_to<bool>;
        { c.read(u) } -> std::convertible_to<bool>;
        { c.read(d) } -> std::convertible_to<bool>;
        { c.read(s) } -> std::convertible_to<bool>;
    };
} // namespace lux::cxx::ser
