// JSON backend implementation — the ONLY translation unit that sees nlohmann_json.
// Keeps the third-party type out of the public headers (no clash with consumers).
#include <lux/cxx/serialization/json.hpp>

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <unordered_set>
#include <vector>

#if defined(LUX_SER_JSON_SIMDJSON)
#   include <simdjson.h>
#else
#   include <exception>
#   include <iterator>
#   include <nlohmann/json.hpp>
#endif

namespace lux::cxx::ser
{
    // ===================== JsonOutputArchive (streaming writer) ===========
    // Emits JSON straight into a std::string — no DOM, no second pass, nlohmann is
    // NOT used on this path. Numbers use std::to_chars (shortest round-trippable
    // form, so 0.7 stays "0.7"); object keys come out in field-declaration order.
    struct JsonOutputArchive::Impl
    {
        std::string        out;
        int                indent;          // < 0 => compact
        struct Frame { bool is_object; bool first; };
        std::vector<Frame> stack;
        bool               after_key = false;   // next value belongs to the key just written

        explicit Impl(int ind) : indent(ind) { out.reserve(256); stack.reserve(16); }

        void newline_indent()
        {
            if (indent < 0) return;
            out.push_back('\n');
            out.append(static_cast<std::size_t>(indent) * stack.size(), ' ');
        }
        // Separator + indent before an array element or the root value. Object
        // members emit their own separator in key().
        void before_value()
        {
            if (after_key) { after_key = false; return; }
            if (stack.empty()) return;
            Frame& f = stack.back();
            if (f.first) f.first = false; else out.push_back(',');
            newline_indent();
        }
        void write_string(std::string_view s)
        {
            out.push_back('"');
            for (unsigned char c : s)
            {
                switch (c)
                {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b";  break;
                    case '\f': out += "\\f";  break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (c < 0x20) { char u[8]; std::snprintf(u, sizeof(u), "\\u%04x", c); out += u; }
                        else          { out.push_back(static_cast<char>(c)); }
                }
            }
            out.push_back('"');
        }
        template <class N>
        void write_number(N n)
        {
            char buf[32];
            const auto r = std::to_chars(buf, buf + sizeof(buf), n);
            out.append(buf, r.ptr);
        }
    };

    JsonOutputArchive::JsonOutputArchive(int indent) : p_(std::make_unique<Impl>(indent)) {}
    JsonOutputArchive::~JsonOutputArchive() = default;
    JsonOutputArchive::JsonOutputArchive(JsonOutputArchive&&) noexcept = default;
    JsonOutputArchive& JsonOutputArchive::operator=(JsonOutputArchive&&) noexcept = default;

    void JsonOutputArchive::begin_object(std::size_t)
    {
        p_->before_value();
        p_->out.push_back('{');
        p_->stack.push_back({ true, true });
    }
    void JsonOutputArchive::key(std::string_view k, bool)
    {
        auto& f = p_->stack.back();
        if (f.first) f.first = false; else p_->out.push_back(',');
        p_->newline_indent();
        p_->write_string(k);
        p_->out.push_back(':');
        if (p_->indent >= 0) p_->out.push_back(' ');
        p_->after_key = true;
    }
    void JsonOutputArchive::end_object()
    {
        const bool had = !p_->stack.back().first;
        p_->stack.pop_back();
        if (had) p_->newline_indent();
        p_->out.push_back('}');
    }
    void JsonOutputArchive::begin_array(std::size_t)
    {
        p_->before_value();
        p_->out.push_back('[');
        p_->stack.push_back({ false, true });
    }
    void JsonOutputArchive::end_array()
    {
        const bool had = !p_->stack.back().first;
        p_->stack.pop_back();
        if (had) p_->newline_indent();
        p_->out.push_back(']');
    }
    void JsonOutputArchive::null_value()              { p_->before_value(); p_->out += "null"; }
    void JsonOutputArchive::value(bool b)             { p_->before_value(); p_->out += (b ? "true" : "false"); }
    void JsonOutputArchive::value(std::int64_t i)     { p_->before_value(); p_->write_number(i); }
    void JsonOutputArchive::value(std::uint64_t u)    { p_->before_value(); p_->write_number(u); }
    void JsonOutputArchive::value(double d)
    {
        p_->before_value();
        if (std::isfinite(d)) p_->write_number(d);
        else                  p_->out += "null";   // JSON has no NaN / Infinity
    }
    void JsonOutputArchive::value(std::string_view s) { p_->before_value(); p_->write_string(s); }

    std::string JsonOutputArchive::str() { return std::move(p_->out); }

#if defined(LUX_SER_JSON_SIMDJSON)
    // =================== JsonInputArchive — simdjson dom backend ==========
    using namespace simdjson;

    // A cursor wraps a simdjson::dom::element by value in JsonInputArchive::storage_.
    // dom::element is a tiny trivially-copyable handle into the document's tape, so
    // copying a cursor is a byte copy and field lookup allocates nothing.
    static_assert(std::is_trivially_copyable_v<dom::element>
                  && sizeof(dom::element) <= 24 && alignof(dom::element) <= 16,
                  "JsonInputArchive::storage_ is too small for simdjson::dom::element");

    struct JsonCursorAccess
    {
        static dom::element elem(const JsonInputArchive& c) noexcept
        {
            return *reinterpret_cast<const dom::element*>(c.storage_);
        }
        static JsonInputArchive make(const dom::element& e) noexcept
        {
            JsonInputArchive c;
            std::memcpy(c.storage_, &e, sizeof(e));
            c.valid_ = true;
            return c;
        }
    };

    bool JsonInputArchive::is_null() const noexcept
    {
        return !valid_ || JsonCursorAccess::elem(*this).is_null();
    }
    bool JsonInputArchive::is_object() const noexcept
    {
        return valid_ && JsonCursorAccess::elem(*this).type() == dom::element_type::OBJECT;
    }
    bool JsonInputArchive::is_array() const noexcept
    {
        return valid_ && JsonCursorAccess::elem(*this).type() == dom::element_type::ARRAY;
    }

    JsonInputArchive JsonInputArchive::member(std::string_view k, bool) const
    {
        if (!valid_) return {};
        auto r = JsonCursorAccess::elem(*this)[k];
        if (r.error()) return {};
        return JsonCursorAccess::make(r.value());
    }

    std::size_t JsonInputArchive::size() const noexcept
    {
        if (!valid_) return 0;
        const auto el = JsonCursorAccess::elem(*this);
        if (auto r = el.get_array();  !r.error()) return r.value().size();
        if (auto r = el.get_object(); !r.error()) return r.value().size();
        return 0;
    }

    JsonInputArchive JsonInputArchive::element(std::size_t i) const
    {
        if (!valid_) return {};
        auto ra = JsonCursorAccess::elem(*this).get_array();
        if (ra.error()) return {};
        auto re = ra.value().at(i);
        if (re.error()) return {};
        return JsonCursorAccess::make(re.value());
    }

    std::size_t JsonInputArchive::member_count() const noexcept
    {
        if (!valid_) return 0;
        auto r = JsonCursorAccess::elem(*this).get_object();
        return r.error() ? 0 : r.value().size();
    }

    std::string_view JsonInputArchive::member_key(std::size_t i) const
    {
        if (!valid_) return {};
        auto r = JsonCursorAccess::elem(*this).get_object();
        if (r.error()) return {};
        std::size_t idx = 0;
        for (dom::key_value_pair kv : r.value()) { if (idx++ == i) return kv.key; }
        return {};
    }

    JsonInputArchive JsonInputArchive::member_value(std::size_t i) const
    {
        if (!valid_) return {};
        auto r = JsonCursorAccess::elem(*this).get_object();
        if (r.error()) return {};
        std::size_t idx = 0;
        for (dom::key_value_pair kv : r.value()) { if (idx++ == i) return JsonCursorAccess::make(kv.value); }
        return {};
    }

    void JsonInputArchive::for_each_member(
        function_ref<void(std::string_view, const JsonInputArchive&)> fn) const
    {
        if (!valid_) return;
        auto r = JsonCursorAccess::elem(*this).get_object();
        if (r.error()) return;
        for (dom::key_value_pair kv : r.value())
        {
            const JsonInputArchive c = JsonCursorAccess::make(kv.value);
            fn(kv.key, c);
        }
    }

    void JsonInputArchive::for_each_element(
        function_ref<void(std::size_t, const JsonInputArchive&)> fn) const
    {
        if (!valid_) return;
        auto r = JsonCursorAccess::elem(*this).get_array();
        if (r.error()) return;
        std::size_t i = 0;
        for (dom::element e : r.value())
        {
            const JsonInputArchive c = JsonCursorAccess::make(e);
            fn(i++, c);
        }
    }

    bool JsonInputArchive::read(bool& o) const
    {
        return valid_ && JsonCursorAccess::elem(*this).get_bool().get(o) == SUCCESS;
    }
    bool JsonInputArchive::read(std::int64_t& o) const
    {
        return valid_ && JsonCursorAccess::elem(*this).get_int64().get(o) == SUCCESS;
    }
    bool JsonInputArchive::read(std::uint64_t& o) const
    {
        if (!valid_) return false;
        const auto el = JsonCursorAccess::elem(*this);
        if (el.get_uint64().get(o) == SUCCESS) return true;
        std::int64_t s;                                  // tolerate a non-negative signed integer
        if (el.get_int64().get(s) == SUCCESS && s >= 0) { o = static_cast<std::uint64_t>(s); return true; }
        return false;
    }
    bool JsonInputArchive::read(double& o) const
    {
        if (!valid_) return false;
        const auto el = JsonCursorAccess::elem(*this);
        if (el.get_double().get(o) == SUCCESS) return true;     // a real double
        std::int64_t s;                                          // an integer-valued number (e.g. "42")
        if (el.get_int64().get(s) == SUCCESS)  { o = static_cast<double>(s); return true; }
        std::uint64_t u;
        if (el.get_uint64().get(u) == SUCCESS) { o = static_cast<double>(u); return true; }
        return false;
    }
    bool JsonInputArchive::read(std::string& o) const
    {
        if (!valid_) return false;
        std::string_view sv;
        if (JsonCursorAccess::elem(*this).get_string().get(sv) != SUCCESS) return false;
        o.assign(sv);   // one copy out of the tape into the caller's string
        return true;
    }

    // ===================== JsonDocument ===================================
    struct JsonDocument::Impl
    {
        dom::parser   parser;   // owns the tape + string buffer the cursors point into
        padded_string padded;   // owns the padded copy of the input
        dom::element  root;
    };

    JsonDocument::JsonDocument() : p_(std::make_unique<Impl>()) {}
    JsonDocument::~JsonDocument() = default;
    JsonDocument::JsonDocument(JsonDocument&&) noexcept = default;
    JsonDocument& JsonDocument::operator=(JsonDocument&&) noexcept = default;

    result<JsonDocument> JsonDocument::parse(std::string_view text)
    {
        JsonDocument doc;
        doc.p_->padded = padded_string(text);           // padded copy simdjson may over-read
        if (const auto err = doc.p_->parser.parse(doc.p_->padded).get(doc.p_->root); err != SUCCESS)
            return make_error(error_code::parse_error, error_message(err));
        return doc;
    }

    namespace
    {
        bool hasDuplicateMembers(
            const dom::element element,
            std::string& duplicate,
            const std::size_t depth = 0
        )
        {
            if (depth > 256)
            {
                duplicate = "JSON nesting exceeds the strict parser limit";
                return true;
            }
            if (element.type() == dom::element_type::OBJECT)
            {
                std::unordered_set<std::string_view> members;
                for (const dom::key_value_pair member : dom::object(element))
                {
                    if (!members.emplace(member.key).second)
                    {
                        duplicate.assign(member.key);
                        return true;
                    }
                    if (hasDuplicateMembers(member.value, duplicate, depth + 1))
                        return true;
                }
            }
            else if (element.type() == dom::element_type::ARRAY)
            {
                for (const dom::element child : dom::array(element))
                    if (hasDuplicateMembers(child, duplicate, depth + 1))
                        return true;
            }
            return false;
        }
    } // namespace

    result<JsonDocument> JsonDocument::parseStrict(std::string_view text)
    {
        auto document = parse(text);
        if (!document) return document;
        std::string duplicate;
        if (hasDuplicateMembers(document->p_->root, duplicate))
        {
            return make_error(
                error_code::duplicate_member,
                "duplicate JSON object member: " + duplicate,
                duplicate
            );
        }
        return document;
    }

    JsonInputArchive JsonDocument::root() const { return JsonCursorAccess::make(p_->root); }

    // ===================== JsonParser (reusable) ==========================
    struct JsonParser::Impl
    {
        dom::parser  parser;   // reused across documents: tape + scratch grow once
        dom::element root;
    };

    JsonParser::JsonParser() : p_(std::make_unique<Impl>()) {}
    JsonParser::~JsonParser() = default;
    JsonParser::JsonParser(JsonParser&&) noexcept = default;
    JsonParser& JsonParser::operator=(JsonParser&&) noexcept = default;

    result<JsonInputArchive> JsonParser::parse(std::string_view text)
    {
        // realloc_if_needed defaults to true: simdjson copies `text` into the
        // parser's OWN padded buffer, which is reused/grown across calls — so there
        // is no fresh padded_string per document, and the tape is reused too.
        if (const auto err = p_->parser.parse(text.data(), text.size()).get(p_->root); err != SUCCESS)
            return make_error(error_code::parse_error, error_message(err));
        return JsonCursorAccess::make(p_->root);
    }

    result<JsonInputArchive> JsonParser::parseStrict(std::string_view text)
    {
        auto parsed = parse(text);
        if (!parsed) return parsed;
        std::string duplicate;
        if (hasDuplicateMembers(p_->root, duplicate))
        {
            return make_error(
                error_code::duplicate_member,
                "duplicate JSON object member: " + duplicate,
                duplicate
            );
        }
        return parsed;
    }

#else // ===================== nlohmann_json backend ======================
    using json = nlohmann::json;

    // The cursor stores a `const json*` (a pointer to a stable node in the parsed
    // DOM) in the same opaque storage_ the simdjson path uses for a dom::element.
    struct JsonCursorAccess
    {
        static const json* node(const JsonInputArchive& c) noexcept
        {
            if (!c.valid_) return nullptr;
            const json* p;
            std::memcpy(&p, c.storage_, sizeof(p));
            return p;
        }
        static JsonInputArchive make(const json* p) noexcept
        {
            JsonInputArchive c;
            if (p) { std::memcpy(c.storage_, &p, sizeof(p)); c.valid_ = true; }
            return c;
        }
    };

    bool JsonInputArchive::is_null()   const noexcept { const json* j = JsonCursorAccess::node(*this); return !j || j->is_null(); }
    bool JsonInputArchive::is_object() const noexcept { const json* j = JsonCursorAccess::node(*this); return j && j->is_object(); }
    bool JsonInputArchive::is_array()  const noexcept { const json* j = JsonCursorAccess::node(*this); return j && j->is_array(); }

    JsonInputArchive JsonInputArchive::member(std::string_view k, bool) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_object()) return {};
        const auto it = j->find(std::string(k));
        if (it == j->end()) return {};
        return JsonCursorAccess::make(&*it);
    }

    std::size_t JsonInputArchive::size() const noexcept { const json* j = JsonCursorAccess::node(*this); return j ? j->size() : 0; }

    JsonInputArchive JsonInputArchive::element(std::size_t i) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_array() || i >= j->size()) return {};
        return JsonCursorAccess::make(&(*j)[i]);
    }

    std::size_t JsonInputArchive::member_count() const noexcept
    {
        const json* j = JsonCursorAccess::node(*this);
        return (j && j->is_object()) ? j->size() : 0;
    }

    std::string_view JsonInputArchive::member_key(std::size_t i) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_object() || i >= j->size()) return {};
        auto it = j->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(i));
        return std::string_view{ it.key() };
    }

    JsonInputArchive JsonInputArchive::member_value(std::size_t i) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_object() || i >= j->size()) return {};
        auto it = j->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(i));
        return JsonCursorAccess::make(&it.value());
    }

    void JsonInputArchive::for_each_member(
        function_ref<void(std::string_view, const JsonInputArchive&)> fn) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_object()) return;
        for (auto it = j->begin(); it != j->end(); ++it)
            fn(std::string_view{ it.key() }, JsonCursorAccess::make(&it.value()));
    }

    void JsonInputArchive::for_each_element(
        function_ref<void(std::size_t, const JsonInputArchive&)> fn) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_array()) return;
        std::size_t i = 0;
        for (const auto& el : *j) fn(i++, JsonCursorAccess::make(&el));
    }

    bool JsonInputArchive::read(bool& o) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_boolean()) return false;
        o = j->get<bool>(); return true;
    }
    bool JsonInputArchive::read(std::int64_t& o) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_number_integer()) return false;
        o = j->get<std::int64_t>(); return true;
    }
    bool JsonInputArchive::read(std::uint64_t& o) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j) return false;
        if (j->is_number_unsigned()) { o = j->get<std::uint64_t>(); return true; }
        if (j->is_number_integer())  { const auto s = j->get<std::int64_t>(); if (s < 0) return false; o = static_cast<std::uint64_t>(s); return true; }
        return false;
    }
    bool JsonInputArchive::read(double& o) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_number()) return false;
        o = j->get<double>(); return true;
    }
    bool JsonInputArchive::read(std::string& o) const
    {
        const json* j = JsonCursorAccess::node(*this);
        if (!j || !j->is_string()) return false;
        o = j->get<std::string>(); return true;
    }

    struct JsonDocument::Impl { json root; };

    JsonDocument::JsonDocument() : p_(std::make_unique<Impl>()) {}
    JsonDocument::~JsonDocument() = default;
    JsonDocument::JsonDocument(JsonDocument&&) noexcept = default;
    JsonDocument& JsonDocument::operator=(JsonDocument&&) noexcept = default;

    result<JsonDocument> JsonDocument::parse(std::string_view text)
    {
        JsonDocument doc;
        try { doc.p_->root = json::parse(text); }
        catch (const std::exception& e) { return make_error(error_code::parse_error, e.what()); }
        return doc;
    }

    namespace
    {
        template <class Parse>
        auto parseStrictNlohmann(Parse&& parse) -> result<json>
        {
            std::vector<std::unordered_set<std::string>> object_members;
            std::string duplicate;
            bool has_duplicate = false;
            const json::parser_callback_t callback =
                [&](const int, const json::parse_event_t event, json& value)
                {
                    if (event == json::parse_event_t::object_start)
                    {
                        object_members.emplace_back();
                    }
                    else if (event == json::parse_event_t::object_end)
                    {
                        if (!object_members.empty()) object_members.pop_back();
                    }
                    else if (event == json::parse_event_t::key &&
                             !object_members.empty())
                    {
                        const std::string key = value.get<std::string>();
                        if (!object_members.back().emplace(key).second &&
                            !has_duplicate)
                        {
                            duplicate = key;
                            has_duplicate = true;
                        }
                    }
                    return true;
                };
            try
            {
                json root = parse(callback);
                if (has_duplicate)
                {
                    return make_error(
                        error_code::duplicate_member,
                        "duplicate JSON object member: " + duplicate,
                        duplicate
                    );
                }
                return root;
            }
            catch (const std::exception& exception)
            {
                return make_error(error_code::parse_error, exception.what());
            }
        }
    } // namespace

    result<JsonDocument> JsonDocument::parseStrict(std::string_view text)
    {
        auto root = parseStrictNlohmann(
            [&](const json::parser_callback_t& callback)
            {
                return json::parse(text, callback);
            }
        );
        if (!root) return lux::cxx::unexpected<error>{root.error()};
        JsonDocument document;
        document.p_->root = std::move(*root);
        return document;
    }

    JsonInputArchive JsonDocument::root() const { return JsonCursorAccess::make(&p_->root); }

    // ===================== JsonParser (reusable) ==========================
    // nlohmann has no reusable parser state, so each parse() builds a fresh DOM held
    // by the parser's Impl — same result as from_json(text), without amortization.
    struct JsonParser::Impl { json root; };

    JsonParser::JsonParser() : p_(std::make_unique<Impl>()) {}
    JsonParser::~JsonParser() = default;
    JsonParser::JsonParser(JsonParser&&) noexcept = default;
    JsonParser& JsonParser::operator=(JsonParser&&) noexcept = default;

    result<JsonInputArchive> JsonParser::parse(std::string_view text)
    {
        try { p_->root = json::parse(text); }
        catch (const std::exception& e) { return make_error(error_code::parse_error, e.what()); }
        return JsonCursorAccess::make(&p_->root);
    }

    result<JsonInputArchive> JsonParser::parseStrict(std::string_view text)
    {
        auto root = parseStrictNlohmann(
            [&](const json::parser_callback_t& callback)
            {
                return json::parse(text, callback);
            }
        );
        if (!root) return lux::cxx::unexpected<error>{root.error()};
        p_->root = std::move(*root);
        return JsonCursorAccess::make(&p_->root);
    }
#endif // LUX_SER_JSON_SIMDJSON
} // namespace lux::cxx::ser
