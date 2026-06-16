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
    // Streaming writer: emits XML straight into a std::string as save() drives it,
    // with NO intermediate tinyxml2 DOM (the old path allocated a full node tree,
    // then printed it). Mirrors the JSON streaming writer. Each element's start tag
    // is kept "open" (no '>') until its first child/text so scalar fields marked
    // xml_attribute land as attributes. Per XML convention those fields precede the
    // non-attribute ones (declaration order); should one arrive after a child is
    // already written, it degrades to a child element — still well-formed, and it
    // round-trips because the reader's member(..., as_attribute) falls back from an
    // attribute to a same-named child element.
    struct XmlOutputArchive::Impl
    {
        struct Frame
        {
            std::size_t name_pos   = 0;     // [pos, pos+len) into `names` = this tag's name
            std::size_t name_len   = 0;
            bool        is_array   = false; // children are <item> vs named members
            bool        start_open = true;  // '<name' written, '>' not yet (attrs ok)
            bool        has_child  = false; // emitted >= 1 child element
        };

        std::string        out;
        std::vector<Frame> stack;
        std::string        names;           // stack arena of open tags' names: append on
                                            // open, pop (resize) on close — reused, so the
                                            // writer allocates only as `out`/`names` grow.
        std::string        root_name;
        bool               pretty = true;

        std::string pending_key;    bool has_pending_key  = false;  // named child awaiting content
        std::string pending_attr;   bool has_pending_attr = false;  // attribute name awaiting value

        void indent(std::size_t depth)
        {
            if (!pretty) return;
            out.push_back('\n');
            out.append(depth * 2, ' ');
        }

        // Close the top element's start tag so children/text may follow.
        void close_start_tag()
        {
            if (!stack.empty() && stack.back().start_open)
            {
                out.push_back('>');
                stack.back().start_open = false;
            }
        }

        static void append_escaped(std::string& o, std::string_view s, bool in_attr)
        {
            for (char c : s)
            {
                switch (c)
                {
                    case '&': o += "&amp;"; break;
                    case '<': o += "&lt;";  break;
                    case '>': o += "&gt;";  break;
                    case '"': o += (in_attr ? "&quot;" : "\""); break;
                    default:  o.push_back(c);
                }
            }
        }

        // Begin a child element <name (start tag left open): close the parent start
        // tag, mark it as having a child, and indent.
        void open_tag(std::string_view name)
        {
            if (!stack.empty())
            {
                close_start_tag();
                stack.back().has_child = true;
                indent(stack.size());
            }
            out.push_back('<');
            out.append(name);
        }

        void write_leaf(std::string_view name, std::string_view raw, bool needs_escape)
        {
            open_tag(name);
            out.push_back('>');
            if (needs_escape) append_escaped(out, raw, false);
            else              out.append(raw);
            out += "</";
            out.append(name);
            out.push_back('>');
        }

        void emit_value(std::string_view raw, bool needs_escape)
        {
            if (has_pending_attr)
            {
                if (!stack.empty() && stack.back().start_open)
                {
                    out.push_back(' ');
                    out.append(pending_attr);
                    out += "=\"";
                    if (needs_escape) append_escaped(out, raw, true);
                    else              out.append(raw);
                    out.push_back('"');
                    has_pending_attr = false;
                    return;
                }
                has_pending_attr = false;                       // start tag already closed:
                write_leaf(pending_attr, raw, needs_escape);    // degrade to a child element
                return;
            }
            if (has_pending_key)
            {
                has_pending_key = false;
                write_leaf(pending_key, raw, needs_escape);
                return;
            }
            if (!stack.empty() && stack.back().is_array) { write_leaf("item", raw, needs_escape); return; }
            if (stack.empty()) { write_leaf(root_name, raw, needs_escape); return; }   // scalar root
            write_leaf("item", raw, needs_escape);
        }

        void begin_container(bool is_array)
        {
            std::string_view name;   // views pending_key / root_name / a literal — never `out`
            if (has_pending_key)      { name = pending_key; has_pending_key = false; }
            else if (!stack.empty() && stack.back().is_array) name = "item";
            else if (stack.empty())   name = root_name;                                 // root
            else                      name = "item";

            open_tag(name);   // start tag stays open so attributes can follow
            const std::size_t pos = names.size();
            names.append(name);                                 // remember for the close tag
            stack.push_back(Frame{ pos, name.size(), is_array, true, false });
        }

        void end_container()
        {
            const Frame f = stack.back();
            stack.pop_back();
            if (!f.start_open)
            {
                if (f.has_child) indent(stack.size());          // close tag on its own line
                out += "</";
                out.append(names, f.name_pos, f.name_len);      // from arena (!= out): safe
                out.push_back('>');
            }
            else
            {
                out += "/>";                                    // no content -> self-close
            }
            names.resize(f.name_pos);                           // pop this name off the arena
        }
    };

    XmlOutputArchive::XmlOutputArchive(std::string_view root_name, bool pretty)
        : p_(std::make_unique<Impl>())
    {
        p_->root_name.assign(root_name);
        p_->pretty = pretty;
        p_->out.reserve(256);
        p_->stack.reserve(16);
        p_->names.reserve(64);
    }
    XmlOutputArchive::~XmlOutputArchive() = default;
    XmlOutputArchive::XmlOutputArchive(XmlOutputArchive&&) noexcept = default;
    XmlOutputArchive& XmlOutputArchive::operator=(XmlOutputArchive&&) noexcept = default;

    void XmlOutputArchive::begin_object(std::size_t) { p_->begin_container(false); }
    void XmlOutputArchive::end_object()              { p_->end_container(); }
    void XmlOutputArchive::begin_array(std::size_t)  { p_->begin_container(true); }
    void XmlOutputArchive::end_array()               { p_->end_container(); }

    void XmlOutputArchive::key(std::string_view name, bool as_attribute)
    {
        if (as_attribute) { p_->pending_attr.assign(name); p_->has_pending_attr = true; }
        else              { p_->pending_key.assign(name);  p_->has_pending_key  = true; }
    }

    void XmlOutputArchive::null_value()
    {
        if (p_->has_pending_attr) { p_->has_pending_attr = false; return; }   // omit attribute
        if (p_->has_pending_key)  { p_->has_pending_key  = false; return; }   // omit optional field
        if (!p_->stack.empty() && p_->stack.back().is_array)
        {
            p_->close_start_tag();
            p_->stack.back().has_child = true;
            p_->indent(p_->stack.size());
            p_->out += "<item/>";                                            // null array element
            return;
        }
        if (p_->stack.empty())                                               // scalar-root null
        {
            p_->out.push_back('<');
            p_->out.append(p_->root_name);
            p_->out += "/>";
        }
    }

    void XmlOutputArchive::value(bool b) { p_->emit_value(b ? "true" : "false", false); }
    void XmlOutputArchive::value(std::int64_t i)
    {
        char buf[32];
        const auto r = std::to_chars(buf, buf + sizeof(buf), i);
        p_->emit_value(std::string_view(buf, static_cast<std::size_t>(r.ptr - buf)), false);
    }
    void XmlOutputArchive::value(std::uint64_t u)
    {
        char buf[32];
        const auto r = std::to_chars(buf, buf + sizeof(buf), u);
        p_->emit_value(std::string_view(buf, static_cast<std::size_t>(r.ptr - buf)), false);
    }
    void XmlOutputArchive::value(double d)
    {
        char buf[32];
        const auto r = std::to_chars(buf, buf + sizeof(buf), d);   // shortest round-trippable
        p_->emit_value(std::string_view(buf, static_cast<std::size_t>(r.ptr - buf)), false);
    }
    void XmlOutputArchive::value(std::string_view s) { p_->emit_value(s, true); }

    std::string XmlOutputArchive::str() const { return p_->out; }

    // ===================== XmlInputArchive ===============================
    bool XmlInputArchive::is_null()   const noexcept { return node_ == nullptr && attr_ == nullptr; }

    // Distinguish shape so load-time type guards actually fire. The writer emits a
    // sequence as <item> children and an object as named (non-item) children and/or
    // attributes; a scalar carries text only. An empty element is shape-ambiguous and
    // is accepted as either an empty object or an empty sequence (so empty containers
    // still round-trip). An attribute cursor (node_ == nullptr) is a scalar → neither.
    bool XmlInputArchive::is_object() const noexcept
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return false;
        if (e->FirstChildElement("item")) return false;                 // <item>s ⇒ sequence
        if (e->FirstChildElement() || e->FirstAttribute()) return true; // named members / attrs ⇒ object
        if (e->GetText() != nullptr) return false;                      // text only ⇒ scalar
        return true;                                                    // empty element ⇒ empty object OK
    }
    bool XmlInputArchive::is_array() const noexcept
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return false;
        if (e->FirstChildElement("item")) return true;                  // has sequence items
        if (e->FirstChildElement() || e->FirstAttribute()) return false;// named members / attrs ⇒ object
        if (e->GetText() != nullptr) return false;                      // text only ⇒ scalar
        return true;                                                    // empty element ⇒ empty sequence OK
    }

    XmlInputArchive XmlInputArchive::member(std::string_view k, bool as_attribute) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return XmlInputArchive(nullptr);
        if (as_attribute)
        {
            const char* v = e->Attribute(std::string(k).c_str());
            if (v) return XmlInputArchive(v, from_attr_t{});
            // Fallback: a streaming writer that couldn't place the attribute in an
            // already-closed start tag degrades it to a same-named child element.
            // Reading it back either way keeps attribute round-trips order-independent.
            return XmlInputArchive(e->FirstChildElement(std::string(k).c_str()));
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

    void XmlInputArchive::for_each_member(
        function_ref<void(std::string_view, const XmlInputArchive&)> fn) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return;
        for (const XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement())  // single O(n) walk
            fn(std::string_view{ c->Name() }, XmlInputArchive(c));
    }

    void XmlInputArchive::for_each_element(
        function_ref<void(std::size_t, const XmlInputArchive&)> fn) const
    {
        const XMLElement* e = as_elem(node_);
        if (!e) return;
        std::size_t i = 0;
        for (const XMLElement* c = e->FirstChildElement("item"); c; c = c->NextSiblingElement("item"))  // single O(n) walk
            fn(i++, XmlInputArchive(c));
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
