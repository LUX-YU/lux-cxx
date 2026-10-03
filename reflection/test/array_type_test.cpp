#include <lux/cxx/reflection/parser/CxxParser.hpp>
#include <lux/cxx/reflection/runtime/MetaIrJson.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace lux::cxx::reflection;

namespace
{
    bool verify(const MetaUnit& unit)
    {
        if (unit.markedRecordDecls().size() != 1)
            return false;
        const auto* record = unit.markedRecordDecls().front();
        if (record->field_decls.size() != 3)
            return false;
        for (const auto index : record->field_decls)
        {
            const auto* field = unit.getDeclAs<FieldDecl>(index);
            const auto* outer = dynamic_cast<const ArrayType*>(field->type);
            if (!outer || outer->array_size != 2 || !outer->element_type)
                return false;
            const auto* inner = dynamic_cast<const ArrayType*>(outer->element_type);
            if (!inner || inner->array_size != 3 || !inner->element_type)
                return false;
            if (inner->element_type->kind != ETypeKinds::Builtin || inner->element_type->size != sizeof(int))
                return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    const auto header = std::filesystem::path(argv[1]);
    std::ofstream(header) << R"cpp(
using Matrix = int[2][3];
typedef int LegacyMatrix[2][3];
struct __attribute__((annotate("fixture::type"))) Arrays
{
    int direct[2][3];
    Matrix aliased;
    LegacyMatrix legacy;
};
)cpp";
    ParseOptions options;
    options.marker_symbol = "fixture";
    options.commands = {"-std=c++20"};
    CxxParser parser(std::move(options));
    parser.setOnParseError([](const std::string& error) { std::cerr << error << '\n'; });
    auto [status, unit] = parser.parseLegacy(header.generic_string());
    if (status != EParseResult::SUCCESS || !verify(unit))
        return 2;
    const auto json = unit.toJson();
    auto restored = MetaUnit::fromJson(json);
    if (!verify(restored) || restored.toJson() != json)
        return 3;
    auto [ir_status, ir] = parser.parse(header.generic_string());
    if (ir_status != EParseResult::SUCCESS)
        return 4;
    const auto transported = ir::templateJson(ir);
    if (!transported || !verify(MetaUnit::fromJson(*transported)))
        return 5;
    std::cout << "Parsed direct/using/typedef nested arrays, JSON fixup and production MetaIr: PASS\n";
}
