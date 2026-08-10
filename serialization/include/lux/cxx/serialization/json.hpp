#pragma once
/*
 * JSON backend — public header.
 *
 * IMPORTANT: this header pulls in NO third-party JSON library. All nlohmann_json
 * usage is hidden inside json.cpp, so including this header can never clash with
 * a consumer's own copy of nlohmann_json. The archive types below are opaque
 * handles whose methods are compiled in the serialization_json library.
 *
 *   to_json(v)         -> std::string
 *   from_json<T>(text) -> result<T>   (lux::cxx::expected<T, ser::error>)
 *
 * Need a raw nlohmann::json DOM? Re-parse the string yourself, e.g.
 *   nlohmann::json dom = nlohmann::json::parse(lux::cxx::ser::to_json(v));
 */
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <lux/cxx/serialization/archive.hpp>
#include <lux/cxx/serialization/core.hpp>
#include <lux/cxx/serialization/error.hpp>

namespace lux::cxx::ser
{
    // ---- OutputArchive: opaque JSON builder (impl in json.cpp) ----------------
    class JsonOutputArchive
    {
    public:
        explicit JsonOutputArchive(int indent = -1);
        ~JsonOutputArchive();
        JsonOutputArchive(JsonOutputArchive&&) noexcept;
        JsonOutputArchive& operator=(JsonOutputArchive&&) noexcept;
        JsonOutputArchive(const JsonOutputArchive&) = delete;
        JsonOutputArchive& operator=(const JsonOutputArchive&) = delete;

        void begin_object(std::size_t = 0);
        void key(std::string_view, bool as_attribute = false);   // JSON ignores as_attribute
        void end_object();
        void begin_array(std::size_t = 0);
        void end_array();
        void null_value();
        void value(bool);
        void value(std::int64_t);
        void value(std::uint64_t);
        void value(double);
        void value(std::string_view);

        std::string str();   // moves out the built buffer

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    // ---- InputArchive: opaque read cursor into a JsonDocument's tree ----------
    class JsonInputArchive
    {
    public:
        JsonInputArchive() noexcept = default;   // invalid cursor

        explicit operator bool() const noexcept { return valid_; }

        bool is_null()   const noexcept;
        bool is_object() const noexcept;
        bool is_array()  const noexcept;

        JsonInputArchive member(std::string_view, bool as_attribute = false) const;   // invalid cursor if absent
        std::size_t      size() const noexcept;
        JsonInputArchive element(std::size_t) const;

        std::size_t      member_count() const noexcept;
        std::string_view member_key(std::size_t) const;
        JsonInputArchive member_value(std::size_t) const;

        // Single-pass O(n) visitors (see archive.hpp). for_each_member visits an
        // object's (key, value-cursor) pairs; for_each_element an array's
        // (index, element-cursor). No-op on a non-object / non-array cursor.
        void for_each_member (function_ref<void(std::string_view, const JsonInputArchive&)>) const;
        void for_each_element(function_ref<void(std::size_t, const JsonInputArchive&)>) const;

        bool read(bool&)         const;
        bool read(std::int64_t&)  const;
        bool read(std::uint64_t&) const;
        bool read(double&)       const;
        bool read(std::string&)  const;

    private:
        // Holds a simdjson::dom::element by value (the type stays hidden in
        // json.cpp). dom::element is a tiny trivially-copyable handle into the
        // document's tape, so copying a cursor is just a byte copy.
        friend struct JsonCursorAccess;
        alignas(16) unsigned char storage_[24] {};
        bool valid_ = false;
    };

    // ---- owns a parsed JSON tree; hands out a root cursor --------------------
    class JsonDocument
    {
    public:
        JsonDocument();
        ~JsonDocument();
        JsonDocument(JsonDocument&&) noexcept;
        JsonDocument& operator=(JsonDocument&&) noexcept;
        JsonDocument(const JsonDocument&) = delete;
        JsonDocument& operator=(const JsonDocument&) = delete;

        static result<JsonDocument> parse(std::string_view text);
        /// Parse JSON and reject duplicate object member names at every depth.
        /// The ordinary parse() API intentionally keeps its historical backend
        /// behavior; strict product/configuration boundaries should use this.
        static result<JsonDocument> parseStrict(std::string_view text);
        JsonInputArchive root() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    // ---- reusable parser: amortizes the parse buffers across many documents ---
    // Keep ONE instance and parse message after message. With the simdjson backend
    // the tape and scratch buffer grow once and are then reused in place, so a
    // steady-state loop allocates ~nothing per document — unlike from_json(text),
    // which spins up a fresh parser (and padded copy) every call. This is the form
    // for a high-throughput server loop.
    //
    // Lifetime: the cursor from parse() — and any string_view it yields — is valid
    // only until the NEXT parse() on the same JsonParser (buffers are reused in
    // place). from_json(parser, text) below is the safe path: it loads into a T,
    // which owns its data, before you can re-parse.
    //
    // nlohmann backend: there is no reusable parser state, so each parse() builds a
    // fresh DOM — identical results, just without the amortization. API is the same.
    class JsonParser
    {
    public:
        JsonParser();
        ~JsonParser();
        JsonParser(JsonParser&&) noexcept;
        JsonParser& operator=(JsonParser&&) noexcept;
        JsonParser(const JsonParser&) = delete;
        JsonParser& operator=(const JsonParser&) = delete;

        // Parse `text`, reusing this parser's buffers. The returned cursor is valid
        // until the next parse() on this parser. Prefer from_json(parser, text).
        [[nodiscard]] result<JsonInputArchive> parse(std::string_view text);
        [[nodiscard]] result<JsonInputArchive> parseStrict(
            std::string_view text
        );

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    static_assert(OutputArchive<JsonOutputArchive>);
    static_assert(InputArchive<JsonInputArchive>);

    // ---- public API (templated wrappers; no third-party types leak) ----------
    template <class T>
    [[nodiscard]] std::string to_json(const T& v, int indent = -1)
    {
        JsonOutputArchive ar(indent);
        save(ar, v);
        return ar.str();
    }

    template <class T>
    [[nodiscard]] result<T> from_json(std::string_view text)
    {
        auto doc = JsonDocument::parse(text);
        if (!doc) return lux::cxx::unexpected<error>(doc.error());
        T out{};
        error err;
        if (!load(doc->root(), out, &err))
        {
            if (err.code == error_code::ok)
                err = error{ error_code::load_failed, "JSON deserialization failed", {} };
            return lux::cxx::unexpected<error>(std::move(err));
        }
        return out;
    }

    // Reuse `parser` across calls to amortize the parse buffers — the hot-loop form
    // of from_json(text). `out` owns its data, so it stays valid after the next
    // parse() on `parser`.
    template <class T>
    [[nodiscard]] result<T> from_json(JsonParser& parser, std::string_view text)
    {
        auto cur = parser.parse(text);
        if (!cur) return lux::cxx::unexpected<error>(cur.error());
        T out{};
        error err;
        if (!load(cur.value(), out, &err))
        {
            if (err.code == error_code::ok)
                err = error{ error_code::load_failed, "JSON deserialization failed", {} };
            return lux::cxx::unexpected<error>(std::move(err));
        }
        return out;
    }
} // namespace lux::cxx::ser
