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
        JsonOutputArchive();
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

        std::string str(int indent = -1) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    // ---- InputArchive: opaque read cursor into a JsonDocument's tree ----------
    class JsonInputArchive
    {
    public:
        JsonInputArchive() noexcept : node_(nullptr) {}
        explicit JsonInputArchive(const void* node) noexcept : node_(node) {}

        explicit operator bool() const noexcept { return node_ != nullptr; }

        bool is_null()   const noexcept;
        bool is_object() const noexcept;
        bool is_array()  const noexcept;

        JsonInputArchive member(std::string_view, bool as_attribute = false) const;   // invalid cursor if absent
        std::size_t      size() const noexcept;
        JsonInputArchive element(std::size_t) const;

        std::size_t      member_count() const noexcept;
        std::string_view member_key(std::size_t) const;
        JsonInputArchive member_value(std::size_t) const;

        bool read(bool&)         const;
        bool read(std::int64_t&)  const;
        bool read(std::uint64_t&) const;
        bool read(double&)       const;
        bool read(std::string&)  const;

    private:
        const void* node_;
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
        JsonInputArchive root() const;

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
        JsonOutputArchive ar;
        save(ar, v);
        return ar.str(indent);
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
} // namespace lux::cxx::ser
