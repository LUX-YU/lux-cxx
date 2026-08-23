// Smoke + assertion test for the parser against test_header.hpp.
//
// Originally this file was a write-out-JSON-and-return-0 smoke test. It now
// makes concrete claims about the parsed contents so a regression in
// declaration discovery, attribute capture, or template parsing breaks the
// test instead of silently rewriting `out.json`.

#include <lux/cxx/reflection/parser/CxxParser.hpp>
#include <lux/cxx/reflection/runtime/MetaIrJson.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
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

    const CXXRecordDecl* find_record(const MetaUnit& meta, std::string_view fq)
    {
        for (const auto* r : meta.markedRecordDecls())
            if (r->full_qualified_name == fq) return r;
        return nullptr;
    }

    const EnumDecl* find_enum(const MetaUnit& meta, std::string_view fq)
    {
        for (const auto* e : meta.markedEnumDecls())
            if (e->full_qualified_name == fq) return e;
        return nullptr;
    }

    void check_included_template_layout_dependency(
        CxxParser& parser,
        const std::filesystem::path& source)
    {
        auto [result, meta] = parser.parseLegacy(source.string());
        check(result == EParseResult::SUCCESS,
              "included template layout dependency parse succeeded");
        check(meta.markedRecordDecls().size() == 1,
              "included marked dependency is not emitted as a marked root");
        if (meta.markedRecordDecls().empty())
            return;

        const auto* consumer = meta.markedRecordDecls().front();
        check(consumer->bases.size() == 1,
              "layout consumer retains its lineage wrapper");
        if (consumer->bases.empty())
            return;

        const auto* wrapper = meta.getDeclAs<CXXRecordDecl>(consumer->bases.front());
        const auto* wrapper_type = wrapper
            ? dynamic_cast<const RecordType*>(wrapper->type)
            : nullptr;
        check(wrapper_type && wrapper_type->template_arguments.size() == 2,
              "lineage wrapper retains canonical template arguments");
        if (!wrapper_type || wrapper_type->template_arguments.size() != 2)
            return;

        const auto* base_type = dynamic_cast<const RecordType*>(
            wrapper_type->template_arguments[1].type
        );
        const auto* base_decl = base_type
            ? decl_cast<CXXRecordDecl>(base_type->decl)
            : nullptr;
        check(base_decl != nullptr,
              "included marked template argument has a hydrated declaration");
        check(base_decl
                  && base_decl->full_qualified_name
                      == "lux::cxx::reflection::test::IncludedLayout",
              "hydrated declaration identifies the included layout dependency");
    }
}

int main(int argc, char* argv[])
{
    const std::filesystem::path test_file_dir    = file_dir(__FILE__);
    const std::filesystem::path executable_dir   = file_dir(argv[0]);
    const std::filesystem::path target_file      = test_file_dir / "test_header.hpp";
    const std::filesystem::path reflection_project_dir = test_file_dir.parent_path();

    std::vector<std::string> compile_commands = {
        "-D__LUX_PARSE_TIME__=1",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        "-std=c++20",
        "-I" + (reflection_project_dir / "include").string(),
    };

    ParseOptions options{
        .name           = "meta_parse_test",
        .version        = "1.0",
        .marker_symbol  = "marked",
        .exclude_symbol = "no_reflect",
        .commands       = std::move(compile_commands),
        .pch_file       = "",
    };

    CxxParser parser(options);
    parser.setOnParseError([](const std::string& m) {
        std::cerr << "[parse] " << m << "\n";
    });

    check_included_template_layout_dependency(
        parser,
        test_file_dir / "template_layout_consumer.hpp"
    );

    auto [compact_result, compact] = parser.parse(target_file.string());
    check(compact_result == EParseResult::SUCCESS, "compact IR parse succeeded");
    check(!compact.nodes().empty(), "compact IR contains semantic nodes");
    const auto template_projection = ir::templateJson(compact);
    check(template_projection.has_value(), "compact IR retains template projection");

    auto [rst, meta] = parser.parseLegacy(target_file.string());
    check(rst == EParseResult::SUCCESS, "parse succeeded");
    if (rst != EParseResult::SUCCESS)
    {
        return 1;
    }

    // ----------------- top-level marker counts -----------------
    // test_header.hpp marks: TestFunction, TestStruct, TestEnum,
    // (anonymous enum), TestClass, TestClass2, TestClass3, TestStruct2.
    check(meta.markedFunctionDecls().size() == 1, "1 marked function");
    check(meta.markedEnumDecls().size()     == 2, "2 marked enums");
    check(meta.markedRecordDecls().size()   == 5, "5 marked records");

    // ----------------- marked function -----------------
    if (!meta.markedFunctionDecls().empty())
    {
        const auto* fn = meta.markedFunctionDecls().front();
        check(fn->name == "TestFunction(myint, size_t &&, const double &, const std::string &, FuncType *, std::string)",
              "TestFunction display name preserved");
        check(fn->spelling == "TestFunction", "TestFunction spelling");
        check(fn->params.size() == 6, "TestFunction has 6 parameters");
        check(!fn->is_variadic, "TestFunction is not variadic");
    }

    // ----------------- TestStruct -----------------
    if (const auto* s = find_record(meta, "TestStruct"))
    {
        check(s->tag_kind == TagDecl::ETagKind::Struct, "TestStruct tag_kind=Struct");
        check(s->field_decls.size() == 5, "TestStruct has 5 fields");
        check(s->bases.empty(), "TestStruct has no bases");
        check(!s->is_abstract, "TestStruct is not abstract");
    }
    else { check(false, "TestStruct present"); }

    // ----------------- TestClass (abstract, has pure virtual) -----------------
    if (const auto* c = find_record(meta, "TestClass"))
    {
        check(c->tag_kind == TagDecl::ETagKind::Class, "TestClass tag_kind=Class");
        check(c->is_abstract, "TestClass is abstract (has __virtual_func = 0)");
        check(c->constructor_decls.size() == 2, "TestClass has 2 constructors");
        check(c->destructor_decl != INVALID_DECL_INDEX, "TestClass has a destructor");
    }
    else { check(false, "TestClass present"); }

    // ----------------- TestStruct2 (multiple inheritance) -----------------
    if (const auto* s2 = find_record(meta, "TestStruct2"))
    {
        check(s2->bases.size() == 2, "TestStruct2 has 2 base classes");
        check(s2->static_method_decls.size() == 2, "TestStruct2 has 2 static methods");
    }
    else { check(false, "TestStruct2 present"); }

    // ----------------- TestEnum -----------------
    if (const auto* e = find_enum(meta, "TestEnum"))
    {
        check(e->is_scoped, "TestEnum is scoped (enum class)");
        check(e->enumerators.size() == 3, "TestEnum has 3 enumerators");
        if (e->enumerators.size() >= 2)
            check(e->enumerators[1].signed_value == 100,
                  "TestEnum::VALUE2 = 100");
    }
    else { check(false, "TestEnum present"); }

    // ----------------- write JSON out for offline inspection -----------------
    {
        const auto out_path = executable_dir / "out.json";
        std::ofstream out(out_path);
        if (out.is_open())
            out << meta.toJson(4);
    }

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
