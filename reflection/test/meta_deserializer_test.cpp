// Deserialization round-trip check.
//
// Usage:
//   meta_deserializer_test                  // self-test against an in-memory tiny JSON
//   meta_deserializer_test <path.json>      // round-trip an external JSON file
//
// In both modes the test:
//   1. parses the input JSON into a MetaUnit
//   2. re-serialises it
//   3. asserts the re-serialised dump round-trips on a *second* fromJson/toJson
//      cycle (idempotent), which catches non-round-tripping fields without
//      requiring the input to be canonicalised first
//   4. writes the deserialised JSON next to the executable for inspection

#include <lux/cxx/reflection/runtime/MetaUnit.hpp>
#include <lux/cxx/reflection/runtime/Declaration.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace lux::cxx::reflection;

namespace
{
    int g_failures = 0;

    void check(bool cond, const char* desc)
    {
        if (cond) { std::cout << "[PASS] " << desc << "\n"; }
        else      { std::cerr << "[FAIL] " << desc << "\n"; ++g_failures; }
    }

    std::filesystem::path file_dir(const std::string& p)
    {
        const auto pos = p.find_last_of("/\\");
        return pos == std::string::npos ? std::filesystem::path{} : p.substr(0, pos);
    }

    std::string read_file(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in.is_open()) {
            std::cerr << "[FATAL] cannot open " << path << "\n";
            std::exit(2);
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    // The smallest JSON document MetaUnit can deserialise without warnings.
    constexpr const char* kTinyJson = R"({
        "name":"tiny",
        "version":"1.0",
        "declarations":[],
        "types":[],
        "marked_record_decls":[],
        "marked_function_decls":[],
        "marked_enum_decls":[]
    })";

    // Regression for index reconciliation: an un-instantiable (UNKNOWN __kind)
    // declaration sits at array position 0, so every later node shifts down by one
    // when it is dropped. The record at JSON index 1 references the field at JSON
    // index 2 by id, and is itself referenced by marked_record_decls = [1]. With the
    // old position-trusting deserializer the record would not be marked (the marked
    // index pointed at the shifted field), fixup would pair nodes with the wrong
    // JSON, and the field cross-reference would point out of bounds. After the fix
    // everything must resolve to the right node.
    void test_drop_reindex()
    {
        using namespace lux::cxx::reflection;
        constexpr const char* kJson = R"({
            "name":"drop","version":"1.0",
            "declarations":[
                { "__kind":"TotallyUnknownKind", "id":"dropme", "index":0 },
                { "__kind":"CXXRecordDecl", "id":"rec", "index":1, "name":"Rec", "fq_name":"Rec", "field_decls":["fld"] },
                { "__kind":"FieldDecl", "id":"fld", "index":2, "name":"x" }
            ],
            "types":[],
            "marked_record_decls":[1],
            "marked_function_decls":[],
            "marked_enum_decls":[]
        })";

        MetaUnit unit = MetaUnit::fromJson(kJson);

        const auto& recs = unit.markedRecordDecls();
        check(recs.size() == 1, "drop-reindex: record is still marked despite a dropped earlier node");
        if (recs.size() == 1)
        {
            check(recs[0]->name == "Rec", "drop-reindex: marked record resolves to the right node");
            check(recs[0]->field_decls.size() == 1, "drop-reindex: record has its one field");
            if (recs[0]->field_decls.size() == 1)
            {
                auto* fld = unit.getDeclAs<FieldDecl>(recs[0]->field_decls[0]);
                check(fld != nullptr, "drop-reindex: field cross-reference resolves to a FieldDecl");
                check(fld && fld->name == "x", "drop-reindex: field cross-reference points at the correct field");
            }
        }
    }
}

int main(int argc, char* argv[])
{
    const std::filesystem::path executable_dir = file_dir(argv[0]);

    std::string source;
    const char* label;
    if (argc >= 2) {
        source = read_file(argv[1]);
        label  = argv[1];
    } else {
        source = kTinyJson;
        label  = "<tiny in-memory document>";
    }

    std::cout << "Deserialising: " << label << "\n";

    MetaUnit unit;
    try {
        unit = MetaUnit::fromJson(source);
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] fromJson threw: " << e.what() << "\n";
        return 1;
    }
    check(true, "fromJson did not throw");

    const std::string dump1 = unit.toJson(4);

    MetaUnit roundtrip = MetaUnit::fromJson(dump1);
    const std::string dump2 = roundtrip.toJson(4);

    check(dump1 == dump2, "idempotent round-trip (fromJson . toJson is stable)");

    // Index-reconciliation regression (independent of the input document).
    test_drop_reindex();

    // Write for offline inspection.
    {
        const auto out_path = executable_dir / "deserialize_out.json";
        std::ofstream out(out_path);
        if (out.is_open()) out << dump1;
    }

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
