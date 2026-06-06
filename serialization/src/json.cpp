// JSON backend implementation — the ONLY translation unit that sees nlohmann_json.
// Keeps the third-party type out of the public headers (no clash with consumers).
#include <lux/cxx/serialization/json.hpp>

#include <exception>
#include <iterator>
#include <vector>

#include <nlohmann/json.hpp>

namespace lux::cxx::ser
{
    using json = nlohmann::json;

    static const json* as_json(const void* p) noexcept { return static_cast<const json*>(p); }

    // ===================== JsonOutputArchive ==============================
    struct JsonOutputArchive::Impl
    {
        struct Frame { json value; bool is_object; std::string key; };

        json               root;
        std::vector<Frame> stack;

        void emit(json v)
        {
            if (stack.empty()) { root = std::move(v); return; }
            Frame& f = stack.back();
            if (f.is_object) f.value[f.key] = std::move(v);
            else             f.value.push_back(std::move(v));
        }
        void pop_emit()
        {
            json v = std::move(stack.back().value);
            stack.pop_back();
            emit(std::move(v));
        }
    };

    JsonOutputArchive::JsonOutputArchive() : p_(std::make_unique<Impl>()) {}
    JsonOutputArchive::~JsonOutputArchive() = default;
    JsonOutputArchive::JsonOutputArchive(JsonOutputArchive&&) noexcept = default;
    JsonOutputArchive& JsonOutputArchive::operator=(JsonOutputArchive&&) noexcept = default;

    void JsonOutputArchive::begin_object(std::size_t) { p_->stack.push_back(Impl::Frame{ json::object(), true, {} }); }
    void JsonOutputArchive::key(std::string_view k, bool)   { p_->stack.back().key.assign(k); }
    void JsonOutputArchive::end_object()              { p_->pop_emit(); }
    void JsonOutputArchive::begin_array(std::size_t)  { p_->stack.push_back(Impl::Frame{ json::array(), false, {} }); }
    void JsonOutputArchive::end_array()               { p_->pop_emit(); }
    void JsonOutputArchive::null_value()              { p_->emit(json(nullptr)); }
    void JsonOutputArchive::value(bool b)             { p_->emit(json(b)); }
    void JsonOutputArchive::value(std::int64_t i)     { p_->emit(json(i)); }
    void JsonOutputArchive::value(std::uint64_t u)    { p_->emit(json(u)); }
    void JsonOutputArchive::value(double d)           { p_->emit(json(d)); }
    void JsonOutputArchive::value(std::string_view s) { p_->emit(json(std::string(s))); }

    std::string JsonOutputArchive::str(int indent) const { return p_->root.dump(indent); }

    // ===================== JsonInputArchive ===============================
    bool JsonInputArchive::is_null()   const noexcept { const json* j = as_json(node_); return !j || j->is_null(); }
    bool JsonInputArchive::is_object() const noexcept { const json* j = as_json(node_); return j && j->is_object(); }
    bool JsonInputArchive::is_array()  const noexcept { const json* j = as_json(node_); return j && j->is_array(); }

    JsonInputArchive JsonInputArchive::member(std::string_view k, bool) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_object()) return JsonInputArchive(nullptr);
        const auto it = j->find(std::string(k));
        if (it == j->end()) return JsonInputArchive(nullptr);
        return JsonInputArchive(&*it);
    }

    std::size_t JsonInputArchive::size() const noexcept { const json* j = as_json(node_); return j ? j->size() : 0; }

    JsonInputArchive JsonInputArchive::element(std::size_t i) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_array() || i >= j->size()) return JsonInputArchive(nullptr);
        return JsonInputArchive(&(*j)[i]);
    }

    std::size_t JsonInputArchive::member_count() const noexcept
    {
        const json* j = as_json(node_);
        return (j && j->is_object()) ? j->size() : 0;
    }

    std::string_view JsonInputArchive::member_key(std::size_t i) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_object() || i >= j->size()) return {};
        auto it = j->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(i));
        return std::string_view{ it.key() };
    }

    JsonInputArchive JsonInputArchive::member_value(std::size_t i) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_object() || i >= j->size()) return JsonInputArchive(nullptr);
        auto it = j->begin();
        std::advance(it, static_cast<std::ptrdiff_t>(i));
        return JsonInputArchive(&it.value());
    }

    bool JsonInputArchive::read(bool& o) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_boolean()) return false;
        o = j->get<bool>(); return true;
    }
    bool JsonInputArchive::read(std::int64_t& o) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_number_integer()) return false;
        o = j->get<std::int64_t>(); return true;
    }
    bool JsonInputArchive::read(std::uint64_t& o) const
    {
        const json* j = as_json(node_);
        if (!j) return false;
        if (j->is_number_unsigned()) { o = j->get<std::uint64_t>(); return true; }
        if (j->is_number_integer())  { const auto s = j->get<std::int64_t>(); if (s < 0) return false; o = static_cast<std::uint64_t>(s); return true; }
        return false;
    }
    bool JsonInputArchive::read(double& o) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_number()) return false;
        o = j->get<double>(); return true;
    }
    bool JsonInputArchive::read(std::string& o) const
    {
        const json* j = as_json(node_);
        if (!j || !j->is_string()) return false;
        o = j->get<std::string>(); return true;
    }

    // ===================== JsonDocument ===================================
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

    JsonInputArchive JsonDocument::root() const { return JsonInputArchive(&p_->root); }
} // namespace lux::cxx::ser
