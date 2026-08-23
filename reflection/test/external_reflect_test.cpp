// Parser test for non-intrusive (LUX_REFLECT_EXTERNAL) reflection of external
// types. Proves that an unannotated foreign struct/enum, registered via a proxy
// in the main file, lands in the MetaUnit with meta identical in shape to an
// intrusively-annotated equivalent — and that the proxy itself never appears.

#include <lux/cxx/reflection/parser/CxxParser.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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

    // (name, type-name, visibility) view of one field — used to compare the
    // external record shape against the intrusive control.
    struct FieldView { std::string name; std::string type_name; EVisibility vis; };

    std::vector<FieldView> public_fields(const MetaUnit& meta, const CXXRecordDecl* rec)
    {
        std::vector<FieldView> out;
        for (size_t idx : rec->field_decls)
        {
            const auto* f = meta.getDeclAs<FieldDecl>(idx);
            if (!f) continue;
            out.push_back(FieldView{
                f->name,
                f->type ? f->type->name : std::string{ "<null>" },
                f->visibility
            });
        }
        return out;
    }
}

int main(int /*argc*/, char* argv[])
{
    const std::filesystem::path test_file_dir          = file_dir(__FILE__);
    const std::filesystem::path target_file            = test_file_dir / "ext_register.hpp";
    const std::filesystem::path reflection_project_dir = test_file_dir.parent_path();

    std::vector<std::string> compile_commands = {
        "-D__LUX_PARSE_TIME__=1",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        "-std=c++20",
        "-I" + (reflection_project_dir / "include").string(),
    };

    ParseOptions options{
        .name           = "external_reflect_test",
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

    auto [rst, meta] = parser.parseLegacy(target_file.string());
    check(rst == EParseResult::SUCCESS, "parse succeeded");
    if (rst != EParseResult::SUCCESS)
        return 1;

    // Two records (ext::ExtPoint + IntrPoint), two enums (ext::ExtColor +
    // IntrColor). The proxy structs themselves must NOT be registered.
    check(meta.markedRecordDecls().size() == 2, "2 marked records (external + intrusive)");
    check(meta.markedEnumDecls().size()   == 2, "2 marked enums (external + intrusive)");

    // --- External struct reflected through the proxy ---
    const auto* ext_point  = find_record(meta, "ext::ExtPoint");
    const auto* intr_point = find_record(meta, "IntrPoint");
    check(ext_point  != nullptr, "ext::ExtPoint reflected non-intrusively");
    check(intr_point != nullptr, "IntrPoint reflected intrusively (control)");

    if (ext_point)
    {
        check(ext_point->tag_kind == TagDecl::ETagKind::Struct, "ExtPoint is a struct");
        const auto fields = public_fields(meta, ext_point);
        check(fields.size() == 2, "ExtPoint has 2 fields");
        if (fields.size() == 2)
        {
            check(fields[0].name == "x" && fields[0].vis == EVisibility::PUBLIC, "ExtPoint.x is public");
            check(fields[1].name == "y" && fields[1].vis == EVisibility::PUBLIC, "ExtPoint.y is public");
            check(fields[0].type_name == "int",    "ExtPoint.x : int");
            check(fields[1].type_name == "double", "ExtPoint.y : double");
        }
    }

    // --- Identical shape to the intrusive control ---
    if (ext_point && intr_point)
    {
        const auto ext_fields  = public_fields(meta, ext_point);
        const auto intr_fields = public_fields(meta, intr_point);
        bool same = ext_fields.size() == intr_fields.size();
        for (size_t i = 0; same && i < ext_fields.size(); ++i)
            same = ext_fields[i].name == intr_fields[i].name
                && ext_fields[i].type_name == intr_fields[i].type_name
                && ext_fields[i].vis == intr_fields[i].vis;
        check(same, "external record shape == intrusive record shape");
    }

    // --- External enum reflected through the proxy ---
    const auto* ext_color  = find_enum(meta, "ext::ExtColor");
    const auto* intr_color = find_enum(meta, "IntrColor");
    check(ext_color  != nullptr, "ext::ExtColor reflected non-intrusively");
    if (ext_color)
    {
        check(ext_color->is_scoped, "ExtColor is scoped (enum class)");
        check(ext_color->enumerators.size() == 3, "ExtColor has 3 enumerators");
        if (ext_color->enumerators.size() == 3)
        {
            check(ext_color->enumerators[0].name == "Red",   "ExtColor[0] == Red");
            check(ext_color->enumerators[1].name == "Green", "ExtColor[1] == Green");
            check(ext_color->enumerators[2].name == "Blue",  "ExtColor[2] == Blue");
        }
    }
    if (ext_color && intr_color)
    {
        bool same = ext_color->is_scoped == intr_color->is_scoped
                 && ext_color->enumerators.size() == intr_color->enumerators.size();
        for (size_t i = 0; same && i < ext_color->enumerators.size(); ++i)
            same = ext_color->enumerators[i].name == intr_color->enumerators[i].name;
        check(same, "external enum shape == intrusive enum shape");
    }

    std::cout << "\n=== Results ===\nFailures: " << g_failures << "\n";
    return g_failures;
}
