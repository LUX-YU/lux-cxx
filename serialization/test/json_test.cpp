// json_test — round-trip the serialization core (save/load) through the JSON backend.
// Note: this TU includes ONLY <lux/cxx/serialization/json.hpp> — no nlohmann_json.

#include "json_types.hpp"
#include "json_types.serialize.hpp"          // generated meta_info<Address/Profile>

#include <lux/cxx/serialization/json.hpp>

#include <iostream>
#include <map>
#include <string>

using namespace lux::cxx::ser;

namespace
{
    int g_failures = 0;
    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_failures; }
    }
    bool has(const std::string& hay, std::string_view needle)
    {
        return hay.find(needle) != std::string::npos;
    }
}

int main()
{
    Profile p;
    p.name        = "Ada";
    p.age         = 36;
    p.favorite    = Color::Blue;
    p.tags        = { "math", "cs" };
    p.nickname_len = 3;
    p.address     = { "London", 12345 };
    p.scores      = { { "alpha", 1 }, { "beta", 2 } };
    p.blob        = { std::byte{1}, std::byte{2}, std::byte{0xFF} };

    // ---- serialize (compact) — verify structural decisions by substring ----
    const std::string j = to_json(p);
    std::cout << to_json(p, 2) << "\n";
    check(has(j, "\"favorite\":\"Blue\""),            "enum -> name");
    check(has(j, "\"tags\":[\"math\",\"cs\"]"),      "sequence -> array");
    check(has(j, "\"scores\":{\"alpha\":1,\"beta\":2}"), "string-keyed map -> object");
    check(has(j, "\"address\":{") && has(j, "\"zip\":12345"), "nested object");
    check(has(j, "\"nickname_len\":3"),              "optional engaged -> value");
    check(has(j, "\"blob\":\"AQL/\""),               "bytes -> base64 string");

    // ---- deserialize (round-trip) ----
    const auto r = from_json<Profile>(j);
    check(r.has_value(), "from_json succeeded");
    if (r)
    {
        const Profile& q = r.value();
        check(q.name == p.name,                                       "name round-trip");
        check(q.age == p.age,                                         "age round-trip");
        check(q.favorite == Color::Blue,                             "enum round-trip");
        check(q.tags == p.tags,                                       "vector round-trip");
        check(q.nickname_len && *q.nickname_len == 3,                "optional round-trip");
        check(q.address.city == "London" && q.address.zip == 12345, "nested round-trip");
        check(q.scores.size() == 2 && q.scores.at("alpha") == 1 && q.scores.at("beta") == 2, "map round-trip");
        check(q.blob == p.blob,                                  "base64 bytes round-trip");
    }

    // ---- optional null + empty containers ----
    Profile p2 = p;
    p2.nickname_len = std::nullopt;
    p2.tags.clear();
    p2.scores.clear();
    const std::string j2 = to_json(p2);
    check(has(j2, "\"nickname_len\":null"), "disengaged optional -> null");
    check(has(j2, "\"tags\":[]"),           "empty vector -> []");
    check(has(j2, "\"scores\":{}"),         "empty map -> {}");
    const auto r2 = from_json<Profile>(j2);
    check(r2 && !r2.value().nickname_len, "optional null round-trip");
    check(r2 && r2.value().tags.empty(),  "empty vector round-trip");
    check(r2 && r2.value().scores.empty(),"empty map round-trip");

    // ---- missing field tolerated (uses default), bad JSON -> error ----
    const auto r3 = from_json<Profile>(R"({"name":"X","age":1})");
    check(r3.has_value() && r3.value().name == "X" && r3.value().tags.empty(), "missing fields tolerated");
    const auto r4 = from_json<Profile>("not json");
    check(!r4.has_value() && r4.error().code == error_code::parse_error, "malformed JSON -> parse_error");

    // Product/config boundaries must not inherit a DOM backend's duplicate-key
    // overwrite behavior.
    const auto strict_duplicate = JsonDocument::parseStrict(
        R"({"name":"first","name":"second"})"
    );
    check(
        !strict_duplicate &&
            strict_duplicate.error().code == error_code::duplicate_member,
        "strict JSON rejects duplicate object members"
    );
    const auto strict_nested_duplicate = JsonDocument::parseStrict(
        R"({"outer":{"value":1,"value":2}})"
    );
    check(
        !strict_nested_duplicate &&
            strict_nested_duplicate.error().code == error_code::duplicate_member,
        "strict JSON rejects nested duplicate object members"
    );
    check(
        JsonDocument::parseStrict(R"({"left":1,"right":2})").has_value(),
        "strict JSON accepts unique object members"
    );
    const auto strict_empty_duplicate = JsonDocument::parseStrict(
        R"({"":1,"":2})"
    );
    check(
        !strict_empty_duplicate &&
            strict_empty_duplicate.error().code == error_code::duplicate_member,
        "strict JSON rejects duplicate empty member names"
    );
    const auto strict_array_duplicate = JsonDocument::parseStrict(
        R"([{"value":1,"value":2}])"
    );
    check(
        !strict_array_duplicate &&
            strict_array_duplicate.error().code == error_code::duplicate_member,
        "strict JSON rejects duplicates nested in arrays"
    );
    JsonParser strict_parser;
    check(
        !strict_parser.parseStrict(R"({"value":1,"value":2})"),
        "reusable strict parser rejects duplicates"
    );
    check(
        strict_parser.parseStrict(R"({"value":1})").has_value(),
        "reusable strict parser remains usable after an error"
    );

    // ---- structured load errors: code + dotted field path ----
    const auto e_top = from_json<Profile>("123");
    check(!e_top.has_value() && e_top.error().code == error_code::type_mismatch,
          "scalar where object expected -> type_mismatch");

    const auto e_nested = from_json<Profile>(R"({"address":{"zip":"not-a-number"}})");
    check(!e_nested.has_value() && e_nested.error().code == error_code::type_mismatch
          && e_nested.error().path == "address.zip",
          "nested type mismatch reports dotted field path");

    const auto e_req = from_json<Required>(R"({"value":7})");
    check(!e_req.has_value() && e_req.error().code == error_code::missing_required
          && e_req.error().path == "id",
          "missing required field -> missing_required with field path");

    const auto e_ok = from_json<Required>(R"({"id":"abc","value":7})");
    check(e_ok.has_value() && e_ok.value().id == "abc", "required field present -> success");

    // ---- omit_empty: empty tagged field is not emitted; non-empty is ----
    WithOmit wo;                          // maybe is empty
    const std::string jo_empty = to_json(wo);
    check(has(jo_empty, "\"keep\":5"),  "omit_empty: untagged field still emitted");
    check(!has(jo_empty, "\"maybe\""),  "omit_empty: empty tagged field is omitted");
    wo.maybe = { 1, 2 };
    check(has(to_json(wo), "\"maybe\":[1,2]"), "omit_empty: non-empty tagged field is emitted");

    // ---- large StringKeyMap: exercises the single-pass for_each_member path ----
    {
        const int N = 5000;
        std::string big = "{";
        for (int i = 0; i < N; ++i) { if (i) big += ','; big += "\"k" + std::to_string(i) + "\":" + std::to_string(i); }
        big += "}";
        const auto m = from_json<std::map<std::string, int>>(big);
        check(m.has_value() && m.value().size() == static_cast<std::size_t>(N), "large map: all entries loaded");
        check(m.has_value() && m.value().at("k0") == 0 && m.value().at("k4999") == 4999, "large map: values correct");
    }

    // ---- reusable JsonParser: many documents through one amortized parser ----
    {
        JsonParser parser;
        bool all_ok = true;
        // Vary size each iteration so buffer reuse is exercised across documents;
        // T owns its data, so each result stays valid after the next parse().
        for (int i = 0; i < 1000; ++i)
        {
            Profile pi = p;
            pi.age  = i;
            pi.name = "u" + std::to_string(i);
            const std::string ji = to_json(pi);
            const auto ri = from_json<Profile>(parser, ji);
            if (!ri || ri.value().age != i || ri.value().name != pi.name) { all_ok = false; break; }
        }
        check(all_ok, "JsonParser: 1000 reused parses round-trip correctly");

        // Errors still surface cleanly, and reuse after an error keeps working.
        const auto bad = from_json<Profile>(parser, "not json");
        check(!bad.has_value() && bad.error().code == error_code::parse_error,
              "JsonParser: malformed input -> parse_error");
        const auto good = from_json<Profile>(parser, to_json(p));
        check(good.has_value() && good.value().name == "Ada", "JsonParser: reuse after error still works");
    }

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
