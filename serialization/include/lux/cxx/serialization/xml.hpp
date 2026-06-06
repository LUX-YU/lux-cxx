#pragma once
/*
 * XML backend — public header.
 *
 * Like json.hpp, this pulls in NO third-party XML library: all tinyxml2 usage is
 * hidden inside xml.cpp, so including this header cannot clash with a consumer's
 * own tinyxml2. The archive types are opaque handles compiled in serialization_xml.
 *
 *   to_xml(v, "root")  -> std::string
 *   from_xml<T>(text)  -> result<T>   (lux::cxx::expected<T, ser::error>)
 *
 * Mapping conventions:
 *   - reflected object -> <Elem> with one child element per field (named by key)
 *   - sequence/tuple   -> repeated <item> child elements
 *   - string-keyed map -> child elements named by key
 *   - scalar           -> element text
 *   - disengaged optional -> the field element is omitted
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
    // ---- OutputArchive: opaque XML builder (impl in xml.cpp) -----------------
    class XmlOutputArchive
    {
    public:
        explicit XmlOutputArchive(std::string_view root_name);
        ~XmlOutputArchive();
        XmlOutputArchive(XmlOutputArchive&&) noexcept;
        XmlOutputArchive& operator=(XmlOutputArchive&&) noexcept;
        XmlOutputArchive(const XmlOutputArchive&) = delete;
        XmlOutputArchive& operator=(const XmlOutputArchive&) = delete;

        void begin_object(std::size_t = 0);
        void key(std::string_view, bool as_attribute = false);   // as_attribute: scalar -> XML attribute
        void end_object();
        void begin_array(std::size_t = 0);
        void end_array();
        void null_value();
        void value(bool);
        void value(std::int64_t);
        void value(std::uint64_t);
        void value(double);
        void value(std::string_view);

        std::string str(bool pretty = true) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    // ---- InputArchive: opaque read cursor over an XML element -----------------
    class XmlInputArchive
    {
    public:
        XmlInputArchive() noexcept : node_(nullptr) {}
        explicit XmlInputArchive(const void* node) noexcept : node_(node) {}

        explicit operator bool() const noexcept { return node_ != nullptr || attr_ != nullptr; }

        bool is_null()   const noexcept;
        bool is_object() const noexcept;
        bool is_array()  const noexcept;

        XmlInputArchive member(std::string_view, bool as_attribute = false) const;   // child element, or attribute
        std::size_t     size() const noexcept;            // count of <item> children
        XmlInputArchive element(std::size_t) const;       // i-th <item> child

        std::size_t     member_count() const noexcept;    // count of all child elements
        std::string_view member_key(std::size_t) const;   // i-th child's tag name
        XmlInputArchive member_value(std::size_t) const;  // i-th child element

        bool read(bool&)         const;
        bool read(std::int64_t&)  const;
        bool read(std::uint64_t&) const;
        bool read(double&)       const;
        bool read(std::string&)  const;

    private:
        struct from_attr_t {};
        XmlInputArchive(const char* attr, from_attr_t) noexcept : node_(nullptr), attr_(attr) {}

        const void* node_;
        const char* attr_ = nullptr;   // non-null => this cursor reads an XML attribute value
    };

    // ---- owns a parsed XML document; hands out a root cursor -----------------
    class XmlDocument
    {
    public:
        XmlDocument();
        ~XmlDocument();
        XmlDocument(XmlDocument&&) noexcept;
        XmlDocument& operator=(XmlDocument&&) noexcept;
        XmlDocument(const XmlDocument&) = delete;
        XmlDocument& operator=(const XmlDocument&) = delete;

        static result<XmlDocument> parse(std::string_view text);
        XmlInputArchive root() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    static_assert(OutputArchive<XmlOutputArchive>);
    static_assert(InputArchive<XmlInputArchive>);

    // ---- public API ----------------------------------------------------------
    template <class T>
    std::string to_xml(const T& v, std::string_view root_name = "root", bool pretty = true)
    {
        XmlOutputArchive ar(root_name);
        save(ar, v);
        return ar.str(pretty);
    }

    template <class T>
    result<T> from_xml(std::string_view text)
    {
        auto doc = XmlDocument::parse(text);
        if (!doc) return lux::cxx::unexpected<error>(doc.error());
        T out{};
        if (!load(doc->root(), out))
            return make_error(error_code::load_failed, "XML deserialization failed");
        return out;
    }
} // namespace lux::cxx::ser
