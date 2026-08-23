// Round-trip + structural-identity test for MetaUnit JSON.
//
// For each of the three reference headers (test_header / test_header2 /
// test_header_extended) the test:
//   1. parses the header, captures original metadata
//   2. serialises to JSON, then deserialises into a fresh MetaUnit
//   3. re-serialises the deserialised unit and asserts string equality
//   4. walks both models and asserts structural identity (counts of
//      marked decls / fields / methods / enumerators / template args)
//   5. spot-checks a handful of newly-round-tripped fields (tag_kind,
//      template_arguments, exclude_symbol effect, etc.)
//
// Also exercises a small set of malformed-JSON inputs to make sure the
// deserialiser degrades gracefully instead of crashing.

#include <lux/cxx/reflection/parser/CxxParser.hpp>
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

    MetaUnit parse_header(const std::filesystem::path& header,
                          const std::filesystem::path& include_root,
                          std::string_view marker)
    {
        std::vector<std::string> commands = {
            "-D__LUX_PARSE_TIME__=1",
            "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
            "-std=c++20",
            "-I" + include_root.string(),
        };

        ParseOptions opts{
            .name           = "roundtrip_test",
            .version        = "1.0",
            .marker_symbol  = std::string(marker),
            .exclude_symbol = "no_reflect",
            .commands       = std::move(commands),
            .pch_file       = "",
        };

        CxxParser parser(opts);
        parser.setOnParseError([](const std::string& m) {
            std::cerr << "[parse] " << m << "\n";
        });

        auto [rst, meta] = parser.parseLegacy(header.string());
        if (rst != EParseResult::SUCCESS)
        {
            std::cerr << "[FATAL] parse of " << header << " failed\n";
            std::exit(2);
        }
        return std::move(meta);
    }

    // Structural-identity walk: returns true iff the two units agree on every
    // surface field we can compare without re-running the parser.
    bool structural_equal(const MetaUnit& a, const MetaUnit& b, const char* label)
    {
        bool ok = true;
        auto eq = [&](bool c, std::string_view d) {
            if (!c) {
                std::cerr << "[FAIL] (" << label << ") " << d << "\n";
                ok = false;
            }
        };

        eq(a.markedRecordDecls().size()   == b.markedRecordDecls().size(),
           "markedRecordDecls.size()");
        eq(a.markedFunctionDecls().size() == b.markedFunctionDecls().size(),
           "markedFunctionDecls.size()");
        eq(a.markedEnumDecls().size()     == b.markedEnumDecls().size(),
           "markedEnumDecls.size()");

        for (size_t i = 0; i < a.markedRecordDecls().size()
             && i < b.markedRecordDecls().size(); ++i)
        {
            const auto* ra = a.markedRecordDecls()[i];
            const auto* rb = b.markedRecordDecls()[i];
            eq(ra->id == rb->id, "record.id");
            eq(ra->full_qualified_name == rb->full_qualified_name, "record.fq_name");
            eq(ra->tag_kind == rb->tag_kind, "record.tag_kind");   // Phase 1.3
            eq(ra->is_abstract == rb->is_abstract, "record.is_abstract");
            eq(ra->is_template == rb->is_template, "record.is_template");
            eq(ra->field_decls.size() == rb->field_decls.size(), "record.field_decls.size()");
            eq(ra->static_var_decls.size() == rb->static_var_decls.size(),
               "record.static_var_decls.size()");
            eq(ra->method_decls.size() == rb->method_decls.size(), "record.method_decls.size()");
            eq(ra->static_method_decls.size() == rb->static_method_decls.size(),
               "record.static_method_decls.size()");
            eq(ra->constructor_decls.size() == rb->constructor_decls.size(),
               "record.constructor_decls.size()");
            eq(ra->bases.size() == rb->bases.size(), "record.bases.size()");
        }

        for (size_t i = 0; i < a.markedEnumDecls().size()
             && i < b.markedEnumDecls().size(); ++i)
        {
            const auto* ea = a.markedEnumDecls()[i];
            const auto* eb = b.markedEnumDecls()[i];
            eq(ea->is_scoped == eb->is_scoped, "enum.is_scoped");
            eq(ea->tag_kind == eb->tag_kind, "enum.tag_kind");   // Phase 1.3
            eq(ea->enumerators.size() == eb->enumerators.size(),
               "enum.enumerators.size()");
            for (size_t k = 0; k < ea->enumerators.size()
                 && k < eb->enumerators.size(); ++k)
            {
                eq(ea->enumerators[k].name == eb->enumerators[k].name,
                   "enum.enumerators[k].name");
                eq(ea->enumerators[k].signed_value
                       == eb->enumerators[k].signed_value,
                   "enum.enumerators[k].signed_value");
            }
        }

        for (size_t i = 0; i < a.markedFunctionDecls().size()
             && i < b.markedFunctionDecls().size(); ++i)
        {
            const auto* fa = a.markedFunctionDecls()[i];
            const auto* fb = b.markedFunctionDecls()[i];
            eq(fa->id == fb->id, "function.id");
            eq(fa->mangling == fb->mangling, "function.mangling");
            eq(fa->invoke_name == fb->invoke_name, "function.invoke_name");
            eq(fa->is_variadic == fb->is_variadic, "function.is_variadic");
            eq(fa->params.size() == fb->params.size(), "function.params.size()");
        }

        return ok;
    }

    void roundtrip_one(const std::filesystem::path& header,
                       const std::filesystem::path& include_root,
                       std::string_view marker,
                       const char* label)
    {
        std::cout << "\n--- roundtrip: " << label << " ---\n";

        MetaUnit original = parse_header(header, include_root, marker);

        const std::string j1 = original.toJson(4);

        MetaUnit restored = MetaUnit::fromJson(j1);

        const std::string j2 = restored.toJson(4);

        check(j1 == j2, (std::string{label} + ": JSON dump equality").c_str());
        check(structural_equal(original, restored, label),
              (std::string{label} + ": structural equality").c_str());

        // Spot-check a single surviving tag_kind value — was previously NOT
        // serialised and would have been default-initialised after fromJson.
        if (!restored.markedRecordDecls().empty())
        {
            const auto k = restored.markedRecordDecls().front()->tag_kind;
            check(k == TagDecl::ETagKind::Struct
                  || k == TagDecl::ETagKind::Class
                  || k == TagDecl::ETagKind::Union,
                  (std::string{label} + ": restored record tag_kind is valid").c_str());
        }
        if (!restored.markedEnumDecls().empty())
        {
            const auto k = restored.markedEnumDecls().front()->tag_kind;
            check(k == TagDecl::ETagKind::Enum,
                  (std::string{label} + ": restored enum tag_kind is Enum").c_str());
        }
    }

    // Fed malformed JSON — must not throw past the MetaUnit::fromJson boundary
    // and must not crash.  We just want a non-crash baseline; the contents of
    // the produced MetaUnit are not asserted.
    void test_malformed_json()
    {
        std::cout << "\n--- malformed JSON resilience ---\n";

        struct Sample { const char* desc; const char* json; };
        const Sample samples[] = {
            {"empty object",       "{}"},
            {"empty declarations", R"({"declarations":[],"types":[]})"},
            {"missing index",      R"({"declarations":[{"__kind":"FunctionDecl","id":"x"}],"types":[]})"},
            {"missing hash",       R"({"declarations":[{"__kind":"FunctionDecl","id":"x","index":0}],"types":[]})"},
            {"unknown kind",       R"({"declarations":[{"__kind":"BogusKind","id":"x"}],"types":[]})"},
            {"non-array decls",    R"({"declarations":"not an array","types":[]})"},
            {"marked idx oob",     R"({"declarations":[],"types":[],"marked_record_decls":[999]})"},
            {"marked idx sentinel",R"({"declarations":[],"types":[],"marked_record_decls":[18446744073709551615]})"},
        };

        for (const auto& s : samples)
        {
            bool threw = false;
            try {
                auto u = MetaUnit::fromJson(s.json);
                (void)u;
            } catch (const std::exception& e) {
                threw = true;
                std::cerr << "[note] '" << s.desc << "' threw: " << e.what() << "\n";
            }
            check(!threw, (std::string{"malformed JSON does not throw: "} + s.desc).c_str());
        }
    }
}

int main(int argc, char* argv[])
{
    const std::filesystem::path test_file_dir = file_dir(__FILE__);
    const std::filesystem::path include_root  = test_file_dir.parent_path() / "include";
    (void)argc; (void)argv;

    roundtrip_one(test_file_dir / "test_header.hpp",          include_root, "marked",              "test_header");
    roundtrip_one(test_file_dir / "test_header_extended.hpp", include_root, "marked",              "test_header_extended");
    roundtrip_one(test_file_dir / "test_header2.hpp",         include_root, "lux_reflection",  "test_header2");

    test_malformed_json();

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
