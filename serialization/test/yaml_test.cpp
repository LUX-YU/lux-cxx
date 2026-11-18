#include "yaml_types.hpp"
#include "yaml_types.serialize.hpp"

#include <lux/cxx/serialization/yaml.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace lux::cxx::ser;

template <>
struct lux::cxx::ser::serializer<YamlCustomPoint>
{
    static constexpr bool is_specialized = true;

    template <class Archive>
    static void save(Archive& archive, const YamlCustomPoint& value)
    {
        archive.begin_array(2);
        archive.value(static_cast<std::int64_t>(value.x));
        archive.value(static_cast<std::int64_t>(value.y));
        archive.end_array();
    }

    template <class Cursor>
    static bool load(const Cursor& cursor, YamlCustomPoint& value)
    {
        if (!cursor.is_array() || cursor.size() != 2)
            return false;
        std::int64_t x = 0;
        std::int64_t y = 0;
        if (!cursor.element(0).read(x) || !cursor.element(1).read(y))
            return false;
        value.x = static_cast<int>(x);
        value.y = static_cast<int>(y);
        return true;
    }
};

namespace
{
    int failures = 0;

    void check(bool condition, const char* description)
    {
        if (condition)
            std::cout << "[PASS] " << description << "\n";
        else
        {
            std::cerr << "[FAIL] " << description << "\n";
            ++failures;
        }
    }

    bool has(std::string_view text, std::string_view part)
    {
        return text.find(part) != std::string_view::npos;
    }
}

int main()
{
    YamlProfile profile;
    profile.name = "Ada";
    profile.ambiguous = "true";
    profile.age = 36;
    profile.ratio = 0.7;
    profile.enabled = true;
    profile.favorite = EYamlColor::GREEN;
    profile.tags = { "C++", "机器人", "line\nbreak" };
    profile.optional_value = 7;
    profile.unique_value = std::make_unique<int>(11);
    profile.shared_value = std::make_shared<std::string>("shared");
    profile.address = { "London", 10001 };
    profile.tuple_value = { 3, "tuple" };
    profile.scores = { { "alpha", 1 }, { "123", 2 } };
    profile.numbered = { { 1, "one" }, { 2, "two" } };
    profile.blob = { std::byte{ 1 }, std::byte{ 2 }, std::byte{ 0xff } };
    profile.custom = { 4, 5 };
    profile.options.name = "visible";

    const std::string yaml = to_yaml(profile);
    const std::string compact = to_yaml(profile, false);
    check(has(yaml, "name: Ada"), "pretty YAML uses block mappings");
    check(has(yaml, "ambiguous: 'true'"), "ambiguous string is quoted");
    check(has(yaml, "favorite: GREEN"), "reflected enum uses its name");
    check(has(yaml, "blob: AQL/"), "byte sequence uses Base64");
    check(has(yaml, "display_name: visible"), "field name override is honored");
    check(has(yaml, "yaml_scalar: 17"), "xml_attribute is ignored by YAML");
    check(!has(yaml, "omitted_when_empty"), "omit_empty field is omitted");
    check(!has(yaml, "runtime_only"), "skip field is omitted");
    check(has(compact, "{") && has(compact, "}"), "compact YAML uses flow style");

    const auto round_trip = from_yaml<YamlProfile>(yaml);
    check(round_trip.has_value(), "reflected profile round-trip parses");
    if (round_trip)
    {
        const auto& value = round_trip.value();
        check(value.name == profile.name && value.ambiguous == "true", "strings round-trip");
        check(value.age == 36 && value.enabled, "scalars round-trip");
        check(value.favorite == EYamlColor::GREEN, "enum round-trips");
        check(value.tags == profile.tags, "Unicode and multiline strings round-trip");
        check(value.optional_value == 7, "optional round-trips");
        check(value.unique_value && *value.unique_value == 11, "unique_ptr round-trips");
        check(value.shared_value && *value.shared_value == "shared", "shared_ptr round-trips");
        check(value.address.city == "London" && value.address.zip == 10001, "nested object round-trips");
        check(value.tuple_value == profile.tuple_value, "tuple round-trips");
        check(value.scores == profile.scores, "string-key map round-trips");
        check(value.numbered == profile.numbered, "non-string-key map round-trips");
        check(value.blob == profile.blob, "Base64 bytes round-trip");
        check(value.custom.x == 4 && value.custom.y == 5, "custom serializer round-trips");
        check(value.options.name == "visible" && value.options.yaml_scalar == 17 &&
                  value.options.runtime_only == 99,
              "field options load correctly");
    }

    const auto null_optional = from_yaml<std::optional<int>>("null");
    check(null_optional && !null_optional.value(), "null loads a disengaged optional");
    check(from_yaml<std::vector<int>>("[]")->empty(), "empty sequence loads");
    check(from_yaml<std::map<std::string, int>>("{}")->empty(), "empty mapping loads");

    check(from_yaml<bool>("TRUE").value_or(false), "YAML 1.2 boolean variants load");
    check(from_yaml<int>("0x2A").value_or(0) == 42, "hexadecimal Core integer loads");
    check(from_yaml<int>("0o52").value_or(0) == 42, "octal Core integer loads");
    check(!from_yaml<bool>("yes") &&
          from_yaml<bool>("yes").error().code == error_code::type_mismatch,
          "YAML 1.1 yes is not accepted as bool");
    check(!from_yaml<std::string>("123") &&
          from_yaml<std::string>("123").error().code == error_code::type_mismatch,
          "plain numeric scalar is not silently converted to string");
    check(from_yaml<std::string>("'123'").value_or("") == "123",
          "quoted numeric-looking string loads as string");
    check(from_yaml<std::string>("!!str 123").value_or("") == "123",
          "standard string tag is supported");
    check(from_yaml<int>("!!int '42'").value_or(0) == 42,
          "standard integer tag is supported");

    const auto positive_inf = from_yaml<double>(to_yaml(std::numeric_limits<double>::infinity()));
    const auto negative_inf = from_yaml<double>(to_yaml(-std::numeric_limits<double>::infinity()));
    const auto nan = from_yaml<double>(to_yaml(std::numeric_limits<double>::quiet_NaN()));
    check(positive_inf && std::isinf(*positive_inf) && *positive_inf > 0,
          "positive infinity round-trips");
    check(negative_inf && std::isinf(*negative_inf) && *negative_inf < 0,
          "negative infinity round-trips");
    check(nan && std::isnan(*nan), "NaN round-trips");

    const auto malformed = from_yaml<YamlProfile>("name: [unterminated");
    check(!malformed && malformed.error().code == error_code::parse_error,
          "malformed YAML reports parse_error");

    const auto duplicate = YamlDocument::parse("outer:\n  value: 1\n  value: 2\n");
    check(!duplicate && duplicate.error().code == error_code::duplicate_member &&
          duplicate.error().path == "outer.value",
          "duplicate key reports duplicate_member and path");

    check(!YamlDocument::parse("---\na: 1\n---\na: 2\n"),
          "multiple YAML documents are rejected");
    check(!YamlDocument::parse("base: &base 1\n"), "anchors are rejected");
    check(!YamlDocument::parse("base: &base 1\ncopy: *base\n"), "aliases are rejected");
    check(!YamlDocument::parse("base: &base {a: 1}\nnext:\n  <<: *base\n"),
          "merge keys are rejected");
    check(!YamlDocument::parse("value: !custom tagged\n"), "custom tags are rejected");
    check(!YamlDocument::parse("value: !!int nope\n"), "invalid core-tagged scalar is rejected");

    const auto missing = from_yaml<YamlRequired>("value: 1\n");
    check(!missing && missing.error().code == error_code::missing_required &&
          missing.error().path == "id",
          "required field error includes path");

    const auto nested_type = from_yaml<YamlProfile>(
        "name: Ada\nambiguous: text\nage: 1\nratio: 1.0\nenabled: true\n"
        "favorite: RED\naddress:\n  city: London\n  zip: wrong\n"
    );
    check(!nested_type && nested_type.error().code == error_code::type_mismatch &&
          nested_type.error().path == "address.zip",
          "nested type mismatch includes dotted path");

    const auto explicit_document = from_yaml<std::map<std::string, int>>(
        "---\nvalue: 3\n...\n"
    );
    check(explicit_document && explicit_document->at("value") == 3,
          "one explicitly marked document is accepted");

    std::cout << "\n=== Results ===\nFailures: " << failures << "\n";
    return failures;
}
