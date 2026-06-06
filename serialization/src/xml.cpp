// XML backend implementation — the ONLY translation unit that sees tinyxml2.
#include <lux/cxx/serialization/xml.hpp>

#include <charconv>
#include <string>
#include <vector>

#include <tinyxml2.h>

namespace lux::cxx::ser
{
    using namespace tinyxml2;

    static const XMLElement* as_elem(const void* p) noexcept { return static_cast<const XMLElement*>(p); }

    // ===================== XmlOutputArchive ==============================
    struct XmlOutputArchive::Impl
    {
        struct Frame { XMLElement* elem; bool is_array; };

        XMLDocument        doc;
        XMLElement*        root = nullptr;
        std::vector<Frame> stack;
        XMLElement*        pending = nullptr;   // object field <key> awaiting its value
        bool               pending_attr = false;
        std::string        pending_attr_name;

        // If an attribute key is pending, returns the element to set it on (and
        // consumes the flag, writing the name to `name`); otherwise nullptr.
        XMLElement* attr_target(std::string& name)
        {
            if (!pending_attr || stack.empty()) return nullptr;
            pending_attr = false;
            name = pending_attr_name;
            return stack.back().elem;
        }

        // The element the next value/object/array should be written into.
        XMLElement* acquire()
        {
            if (stack.empty()) return root;
            Frame& f = stack.back();
            if (f.is_array)
            {
                XMLElement* item = doc.NewElement("item");
                f.elem->InsertEndChild(item);
                return item;
            }
            return pending;   // object field element created by key()
        }
    };

    XmlOutputArchive::XmlOutputArchive(std::string_view root_name) : p_(std::make_unique<Impl>())
    {
        p_->root = p_->doc.NewElement(std::string(root_name).c_str());
        p_->doc.InsertEndChild(p_->root);
    }
    XmlOutputArchive::~XmlOutputArchive() = default;
    XmlOutputArchive::XmlOutputArchive(XmlOutputArchive&&) noexcept = default;
    XmlOutputArchive& XmlOutputArchive::operator=(XmlOutputArchive&&) noexcept = default;

    void XmlOutputArchive::begin_object(std::size_t) { p_->stack.push_back(Impl::Frame{ p_->acquire(), false }); }
    void XmlOutputArchive::end_object()              { p_->stack.pop_back(); }
    void XmlOutputArchive::begin_array(std::size_t)  { p_->stack.push_back(Impl::Frame{ p_->acquire(), true }); }
    void XmlOutputArchive::end_array()               { p_->stack.pop_back(); }

    void XmlOutputArchive::key(std::string_view name, bool as_attribute)
    {
        if (as_attribute)
        {
            p_->pending_attr = true;
            p_->pending_attr_name.assign(name);   // no child element; the value becomes an attribute
            return;
        }
        p_->pending = p_->doc.NewElement(std::string(name).c_str());
        p_->stack.back().elem->InsertEndChild(p_->pending);
    }

    void XmlOutputArchive::null_value()
    {
        if (p_->stack.empty()) return;                 // root null -> empty root
        Impl::Frame& f = p_->stack.back();
        if (f.is_array)
        {
            XMLElement* item = p_->doc.NewElement("item");
            f.elem->InsertEndChild(item);              // empty <item/>
        }
        else if (p_->pending)
        {
            f.elem->DeleteChild(p_->pending);          // omit disengaged optional field
            p_->pending = nullptr;
        }
    }

    void XmlOutputArchive::value(bool b)
    {
        std::string n;
        if (XMLElement* e = p_->attr_target(n)) e->SetAttribute(n.c_str(), b);
        else p_->acquire()->SetText(b);
    }
    void XmlOutputArchive::value(double d)
    {
        std::string n;
        if (XMLElement* e = p_->attr_target(n)) e->SetAttribute(n.c_str(), d);
        else p_->acquire()->SetText(d);
    }
    void XmlOutputArchive::value(std::int64_t i)
    {
        std::string n;
        if (XMLElement* e = p_->attr_target(n)) e->SetAttribute(n.c_str(), std::to_string(i).c_str());
        else p_->acquire()->SetText(std::to_string(i).c_str());
    }
    void XmlOutputArchive::value(std::uint64_t u)
    {
        std::string n;
        if (XMLElement* e = p_->attr_target(n)) e->SetAttribute(n.c_str(), std::to_string(u).c_str());
        else p_->acquire()->SetText(std::to_string(u).c_str());
    }
    void XmlOutputArchive::value(std::string_view s)
    {
        std::string n;
        if (XMLElement* e = p_->attr_target(n)) e->SetAttribute(n.c_str(), std::string(s).c_str());
        else p_->acquire()->SetText(std::string(s).c_str());
    }

    std::string XmlOutputArchive::str(bool pretty) const
    {
        XMLPrinter printer(nullptr, !pretty);
        p_->doc.Print(&printer);
        return std::string(printer.CStr());
    }

    // ===================== XmlInputArchive ===============================
    bool XmlInputArchive::is_null()   const noexcept { return node_ == nullptr && attr_ == nullptr; }
    bool XmlInputArchive::is_object() const noexcept { return node_ != nullptr; }   // attribute cursor is a scalar
    bool XmlInputArchive::is_array()  const noexcept { return node_ != nullptr; }

    XmlInputArchive XmlInputArchive::member(std::string_view k, bool as_attribute) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return XmlInputArchive(nullptr);
        if (as_attribute)
        {
            const char* v = e->Attribute(std::string(k).c_str());
            return v ? XmlInputArchive(v, from_attr_t{}) : XmlInputArchive();
        }
        return XmlInputArchive(e->FirstChildElement(std::string(k).c_str()));
    }

    std::size_t XmlInputArchive::size() const noexcept
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return 0;
        std::size_t n = 0;
        for (const XMLElement* c = e->FirstChildElement("item"); c; c = c->NextSiblingElement("item")) ++n;
        return n;
    }

    XmlInputArchive XmlInputArchive::element(std::size_t i) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return XmlInputArchive(nullptr);
        const XMLElement* c = e->FirstChildElement("item");
        for (std::size_t k = 0; c && k < i; ++k) c = c->NextSiblingElement("item");
        return XmlInputArchive(c);
    }

    std::size_t XmlInputArchive::member_count() const noexcept
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return 0;
        std::size_t n = 0;
        for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) ++n;
        return n;
    }

    std::string_view XmlInputArchive::member_key(std::size_t i) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return {};
        const XMLElement* c = e->FirstChildElement();
        for (std::size_t k = 0; c && k < i; ++k) c = c->NextSiblingElement();
        return c ? std::string_view{ c->Name() } : std::string_view{};
    }

    XmlInputArchive XmlInputArchive::member_value(std::size_t i) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return XmlInputArchive(nullptr);
        const XMLElement* c = e->FirstChildElement();
        for (std::size_t k = 0; c && k < i; ++k) c = c->NextSiblingElement();
        return XmlInputArchive(c);
    }

    namespace
    {
        // Scalar text of a cursor: an attribute value, or an element's text.
        const char* cursor_text(const void* node, const char* attr)
        {
            if (attr) return attr;
            const XMLElement* e = static_cast<const XMLElement*>(node);
            return e ? e->GetText() : nullptr;
        }

        template <class T>
        bool parse_number(const char* t, T& out)
        {
            if (!t) return false;
            std::string_view sv{ t };
            const auto* first = sv.data();
            const auto* last  = sv.data() + sv.size();
            const auto res = std::from_chars(first, last, out);
            return res.ec == std::errc() && res.ptr == last;
        }
    }

    bool XmlInputArchive::read(bool& o) const
    {
        const char* t = cursor_text(node_, attr_);
        if (!t) return false;
        std::string_view sv{ t };
        if (sv == "true" || sv == "1")  { o = true;  return true; }
        if (sv == "false" || sv == "0") { o = false; return true; }
        return false;
    }
    bool XmlInputArchive::read(std::int64_t& o)  const { return parse_number(cursor_text(node_, attr_), o); }
    bool XmlInputArchive::read(std::uint64_t& o) const { return parse_number(cursor_text(node_, attr_), o); }
    bool XmlInputArchive::read(double& o)        const { return parse_number(cursor_text(node_, attr_), o); }
    bool XmlInputArchive::read(std::string& o) const
    {
        const char* t = cursor_text(node_, attr_);
        o = t ? t : "";       // empty element/attribute -> empty string
        return true;
    }

    // ===================== XmlDocument ====================================
    struct XmlDocument::Impl { XMLDocument doc; };

    XmlDocument::XmlDocument() : p_(std::make_unique<Impl>()) {}
    XmlDocument::~XmlDocument() = default;
    XmlDocument::XmlDocument(XmlDocument&&) noexcept = default;
    XmlDocument& XmlDocument::operator=(XmlDocument&&) noexcept = default;

    result<XmlDocument> XmlDocument::parse(std::string_view text)
    {
        XmlDocument doc;
        const XMLError err = doc.p_->doc.Parse(text.data(), text.size());
        if (err != XML_SUCCESS)
            return make_error(error_code::parse_error, doc.p_->doc.ErrorStr() ? doc.p_->doc.ErrorStr() : "XML parse error");
        return doc;
    }

    XmlInputArchive XmlDocument::root() const
    {
        return XmlInputArchive(p_->doc.RootElement());
    }
} // namespace lux::cxx::ser
