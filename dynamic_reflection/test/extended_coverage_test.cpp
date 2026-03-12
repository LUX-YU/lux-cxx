#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cassert>

using namespace lux::cxx::dref;

static std::filesystem::path file_dir(const std::string& filePath) {
    size_t pos = filePath.find_last_of("/\\");
    if (pos == std::string::npos) {
        return "";
    }
    return filePath.substr(0, pos);
}

int main(int argc, char* argv[])
{
    std::filesystem::path test_file_dir    = file_dir(__FILE__);
    std::filesystem::path executable_dir   = file_dir(argv[0]);
    std::filesystem::path target_file      = test_file_dir / "test_header_extended.hpp";
    std::filesystem::path dref_project_dir = test_file_dir.parent_path();

    std::vector<std::string> compile_commands = {
        "-D__LUX_PARSE_TIME__=1",
        "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH",
        "-std=c++20",
        "-I" + (dref_project_dir / "include").string(),
    };

    ParseOptions options{
        .name          = "extended_test",
        .version       = "1.0",
        .marker_symbol = "marked",
        .commands      = std::move(compile_commands),
        .pch_file      = "",
    };

    CxxParser parser(options);

    int error_count = 0;
    parser.setOnParseError(
        [&error_count](const std::string& error) {
            std::cerr << "[ERROR] " << error << std::endl;
            error_count++;
        }
    );

    auto [rst, meta] = parser.parse(target_file.string());
    if (rst != EParseResult::SUCCESS)
    {
        std::cerr << "Parse failed!" << std::endl;
        return 1;
    }

    // Write full JSON output
    auto json = meta.toJson();
    auto meta_json_str = json.dump(4);
    auto out_file = executable_dir / "extended_out.json";
    std::ofstream out(out_file);
    if (!out.is_open())
    {
        std::cerr << "Failed to open output file: " << out_file << std::endl;
        return 1;
    }
    out << meta_json_str;
    out.close();

    // ============================================================
    // Verify parsed results
    // ============================================================
    int test_failures = 0;
    auto check = [&test_failures](bool condition, const char* desc) {
        if (!condition) {
            std::cerr << "[FAIL] " << desc << std::endl;
            test_failures++;
        } else {
            std::cout << "[PASS] " << desc << std::endl;
        }
    };

    // Basic counts
    auto records = meta.markedRecordDecls();
    auto functions = meta.markedFunctionDecls();
    auto enums = meta.markedEnumDecls();

    std::cout << "=== Marked Records: " << records.size() << " ===" << std::endl;
    for (auto* r : records) {
        std::cout << "  Record: " << r->full_qualified_name << std::endl;
    }
    std::cout << "=== Marked Functions: " << functions.size() << " ===" << std::endl;
    for (auto* f : functions) {
        std::cout << "  Function: " << f->full_qualified_name << std::endl;
    }
    std::cout << "=== Marked Enums: " << enums.size() << " ===" << std::endl;
    for (auto* e : enums) {
        std::cout << "  Enum: " << e->full_qualified_name << std::endl;
    }

    // 1. Union
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "TestUnion") {
                found = true;
                check(r->tag_kind == TagDecl::ETagKind::Union, "TestUnion is tagged as Union");
                check(r->field_decls.size() == 4, "TestUnion has 4 fields (i, f, c, d)");
            }
        }
        check(found, "TestUnion found in marked records");
    }

    // 2. Builtin types
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "BuiltinTypesStruct") {
                found = true;
                check(r->field_decls.size() == 18, "BuiltinTypesStruct has 18 fields");
            }
        }
        check(found, "BuiltinTypesStruct found");
    }

    // 3. Variadic function
    {
        bool found = false;
        for (auto* f : functions) {
            if (f->full_qualified_name.find("TestVariadicFunction") != std::string::npos) {
                found = true;
                check(f->is_variadic, "TestVariadicFunction is variadic");
                check(f->params.size() == 1, "TestVariadicFunction has 1 named param");
            }
        }
        check(found, "TestVariadicFunction found");
    }

    // 4. Volatile method
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "TestVolatileClass") {
                found = true;
                for (auto midx : r->method_decls) {
                    auto* m = meta.getDeclAs<CXXMethodDecl>(midx);
                    if (!m) continue;
                    if (m->spelling == "volatile_method") {
                        check(m->is_volatile, "volatile_method has volatile qualifier");
                    }
                    if (m->spelling == "const_volatile_method") {
                        check(m->is_const && m->is_volatile,
                              "const_volatile_method has const+volatile");
                    }
                }
            }
        }
        check(found, "TestVolatileClass found");
    }

    // 5. Member pointer
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "MemberPointerHolder") {
                found = true;
                check(r->field_decls.size() == 2, "MemberPointerHolder has 2 fields");
            }
        }
        check(found, "MemberPointerHolder found");
    }

    // 6. Virtual destructor + virtual inheritance
    {
        bool found_base = false, found_derived = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "VirtualBase") {
                found_base = true;
                check(r->destructor_decl != INVALID_DECL_INDEX, "VirtualBase has destructor");
                if (r->destructor_decl != INVALID_DECL_INDEX) {
                    auto* dtor = meta.getDeclAs<CXXMethodDecl>(r->destructor_decl);
                    check(dtor && dtor->is_virtual, "VirtualBase destructor is virtual");
                }
                check(r->is_abstract, "VirtualBase is abstract");
            }
            if (r->full_qualified_name == "VirtualDerived") {
                found_derived = true;
                check(r->bases.size() == 1, "VirtualDerived has 1 base");
            }
        }
        check(found_base, "VirtualBase found");
        check(found_derived, "VirtualDerived found");
    }

    // 7. Nested class
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "OuterClass") {
                found = true;
                check(r->field_decls.size() >= 1, "OuterClass has at least nested_field");
            }
        }
        check(found, "OuterClass found");
    }

    // 8. Operator overloads
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "OperatorStruct") {
                found = true;
                // operator+, operator==, operator= are all methods
                check(r->method_decls.size() >= 3, "OperatorStruct has >= 3 methods (operators)");
            }
        }
        check(found, "OperatorStruct found");
    }

    // 9. Conversion operator
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ConversionClass") {
                found = true;
                check(r->method_decls.size() == 2, "ConversionClass has 2 conversion operators");
                for (auto midx : r->method_decls) {
                    auto* m = meta.getDeclAs<CXXMethodDecl>(midx);
                    if (m)
                        std::cout << "    method: " << m->full_qualified_name 
                                  << " kind=" << (int)m->kind << std::endl;
                }
            }
        }
        check(found, "ConversionClass found");
    }

    // 10. Static field (note: static fields are typically not parsed as FieldDecl)
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "StaticFieldStruct") {
                found = true;
                // instance_val should be the field, static_count may not appear
                std::cout << "  StaticFieldStruct fields: " << r->field_decls.size() << std::endl;
                for (auto fidx : r->field_decls) {
                    auto* f = meta.getDeclAs<FieldDecl>(fidx);
                    if (f) std::cout << "    field: " << f->spelling << std::endl;
                }
            }
        }
        check(found, "StaticFieldStruct found");
    }

    // 11. Const fields
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ConstFieldStruct") {
                found = true;
                check(r->field_decls.size() == 3, "ConstFieldStruct has 3 fields");
                if (r->field_decls.size() >= 1) {
                    // First field should be const int
                    auto* fd   = meta.getDeclAs<FieldDecl>(r->field_decls[0]);
                    auto* type = fd ? fd->type : nullptr;
                    check(type && type->is_const, "ConstFieldStruct::ci has const type");
                }
            }
        }
        check(found, "ConstFieldStruct found");
    }

    // 12. noexcept
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "NoexceptStruct") {
                found = true;
                check(r->method_decls.size() == 2, "NoexceptStruct has 2 methods");
            }
        }
        check(found, "NoexceptStruct found");
    }

    // 13. Complex return types
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ComplexReturnTypes") {
                found = true;
                check(r->method_decls.size() == 5, "ComplexReturnTypes has 5 methods");
                for (auto midx : r->method_decls) {
                    auto* fn = meta.getDeclAs<FunctionDecl>(midx);
                    if (fn) {
                        std::cout << "    " << fn->spelling << " -> " 
                                  << (fn->result_type ? fn->result_type->name : "null") << std::endl;
                    }
                }
            }
        }
        check(found, "ComplexReturnTypes found");
    }

    // 14. Multiple inheritance
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "MultiDerived") {
                found = true;
                check(r->bases.size() == 2, "MultiDerived has 2 bases (MixinA, MixinB)");
            }
        }
        check(found, "MultiDerived found");
    }

    // 15. Enums with explicit underlying type
    {
        bool found_small = false, found_signed = false;
        for (auto* e : enums) {
            if (e->full_qualified_name == "SmallEnum") {
                found_small = true;
                check(e->is_scoped, "SmallEnum is scoped");
                check(e->enumerators.size() == 3, "SmallEnum has 3 enumerators");
                if (e->underlying_type) {
                    std::cout << "  SmallEnum underlying: " << e->underlying_type->name << std::endl;
                }
            }
            if (e->full_qualified_name == "SignedEnum") {
                found_signed = true;
                check(e->is_scoped, "SignedEnum is scoped");
                check(e->enumerators.size() == 3, "SignedEnum has 3 enumerators");
                // Check negative value
                for (auto& en : e->enumerators) {
                    if (en.name == "NEG") {
                        check(en.signed_value == -100, "SignedEnum::NEG == -100");
                    }
                }
            }
        }
        check(found_small, "SmallEnum found");
        check(found_signed, "SignedEnum found");
    }

    // 16. Deeply nested namespace
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ns1::ns2::ns3::DeepNestedStruct") {
                found = true;
                check(r->field_decls.size() == 1, "DeepNestedStruct has 1 field");
                check(r->method_decls.size() == 1, "DeepNestedStruct has 1 method");
            }
        }
        check(found, "ns1::ns2::ns3::DeepNestedStruct found");
    }

    // 17. extern "C" function
    {
        bool found = false;
        for (auto* f : functions) {
            if (f->full_qualified_name.find("ExternCFunction") != std::string::npos) {
                found = true;
                check(f->params.size() == 2, "ExternCFunction has 2 params");
            }
        }
        check(found, "ExternCFunction found");
    }

    // 18. Anonymous struct/union
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ContainsAnonymous") {
                found = true;
                std::cout << "  ContainsAnonymous fields: " << r->field_decls.size() << std::endl;
                for (auto fidx : r->field_decls) {
                    auto* f = meta.getDeclAs<FieldDecl>(fidx);
                    if (f)
                        std::cout << "    field: " << f->spelling << " type: " 
                                  << (f->type ? f->type->name : "null") << std::endl;
                }
            }
        }
        check(found, "ContainsAnonymous found");
    }

    // 19. Default parameters
    {
        bool found = false;
        for (auto* f : functions) {
            if (f->full_qualified_name.find("FuncWithDefaults") != std::string::npos) {
                found = true;
                check(f->params.size() == 3, "FuncWithDefaults has 3 params");
            }
        }
        check(found, "FuncWithDefaults found");
    }

    // 20. nullptr_t parameter
    {
        bool found = false;
        for (auto* f : functions) {
            if (f->full_qualified_name.find("FuncWithNullptr") != std::string::npos) {
                found = true;
                check(f->params.size() == 1, "FuncWithNullptr has 1 param");
            }
        }
        check(found, "FuncWithNullptr found");
    }

    // 21. Complex param struct
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ComplexParamStruct") {
                found = true;
                check(r->method_decls.size() == 2, "ComplexParamStruct has 2 methods");
            }
        }
        check(found, "ComplexParamStruct found");
    }

    // 22. Template specialization types (ECS component fields)
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "ECSComponent") {
                found = true;
                check(r->field_decls.size() == 8, "ECSComponent has 8 fields");

                // Helper: get field type as RecordType
                auto getFieldRecordType = [&](const char* fieldName) -> const RecordType* {
                    for (auto fidx : r->field_decls) {
                        auto* f = meta.getDeclAs<FieldDecl>(fidx);
                        if (f && f->spelling == fieldName && f->type) {
                            return dynamic_cast<const RecordType*>(f->type);
                        }
                    }
                    return nullptr;
                };

                // positions: std::vector<int>
                {
                    auto* rt = getFieldRecordType("positions");
                    check(rt != nullptr, "positions field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "positions is a template specialization");
                        std::cout << "  positions template_name: " << rt->template_name << std::endl;
                        std::cout << "  positions template_arguments count: " << rt->template_arguments.size() << std::endl;
                        // std::vector has 2 template params (T, Allocator) but we at least expect 1+
                        check(!rt->template_name.empty(), "positions has a template name");
                        check(rt->template_name.find("vector") != std::string::npos, 
                              "positions template_name contains 'vector'");
                        bool has_int_arg = false;
                        for (auto& arg : rt->template_arguments) {
                            std::cout << "    arg kind=" << (int)arg.kind 
                                      << " spelling=" << arg.spelling << std::endl;
                            if (arg.kind == TemplateArgument::Kind::Type && 
                                arg.spelling.find("int") != std::string::npos) {
                                has_int_arg = true;
                            }
                        }
                        check(has_int_arg, "positions has 'int' template argument");
                    }
                }

                // velocity: std::array<float, 3>
                {
                    auto* rt = getFieldRecordType("velocity");
                    check(rt != nullptr, "velocity field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "velocity is a template specialization");
                        std::cout << "  velocity template_name: " << rt->template_name << std::endl;
                        std::cout << "  velocity template_arguments count: " << rt->template_arguments.size() << std::endl;
                        check(rt->template_name.find("array") != std::string::npos,
                              "velocity template_name contains 'array'");
                        bool has_float_arg = false;
                        bool has_integral_3 = false;
                        for (auto& arg : rt->template_arguments) {
                            std::cout << "    arg kind=" << (int)arg.kind
                                      << " spelling=" << arg.spelling
                                      << " integral_value=" << arg.integral_value << std::endl;
                            if (arg.kind == TemplateArgument::Kind::Type && 
                                arg.spelling.find("float") != std::string::npos) {
                                has_float_arg = true;
                            }
                            if (arg.kind == TemplateArgument::Kind::Integral &&
                                arg.integral_value == 3) {
                                has_integral_3 = true;
                            }
                        }
                        check(has_float_arg, "velocity has 'float' template argument");
                        check(has_integral_3, "velocity has integral template argument = 3");
                    }
                }

                // attributes: std::map<std::string, int>
                {
                    auto* rt = getFieldRecordType("attributes");
                    check(rt != nullptr, "attributes field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "attributes is a template specialization");
                        std::cout << "  attributes template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("map") != std::string::npos,
                              "attributes template_name contains 'map'");
                        // std::map<std::string, int> has 4 template params (Key, Val, Compare, Alloc)
                        // but at least 2 are meaningful
                        check(rt->template_arguments.size() >= 2, 
                              "attributes has at least 2 template arguments");
                    }
                }

                // flexible_data: std::variant<int, float, std::string>
                {
                    auto* rt = getFieldRecordType("flexible_data");
                    check(rt != nullptr, "flexible_data field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "flexible_data is a template specialization");
                        std::cout << "  flexible_data template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("variant") != std::string::npos,
                              "flexible_data template_name contains 'variant'");
                        check(rt->template_arguments.size() >= 3,
                              "flexible_data has at least 3 template arguments");
                    }
                }

                // grouped_data: std::unordered_map<std::string, std::vector<int>>
                {
                    auto* rt = getFieldRecordType("grouped_data");
                    check(rt != nullptr, "grouped_data field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "grouped_data is a template specialization");
                        std::cout << "  grouped_data template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("unordered_map") != std::string::npos,
                              "grouped_data template_name contains 'unordered_map'");
                    }
                }

                // optional_value: std::optional<double>
                {
                    auto* rt = getFieldRecordType("optional_value");
                    check(rt != nullptr, "optional_value field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "optional_value is a template specialization");
                        std::cout << "  optional_value template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("optional") != std::string::npos,
                              "optional_value template_name contains 'optional'");
                    }
                }

                // tags: std::set<std::string>
                {
                    auto* rt = getFieldRecordType("tags");
                    check(rt != nullptr, "tags field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "tags is a template specialization");
                        std::cout << "  tags template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("set") != std::string::npos,
                              "tags template_name contains 'set'");
                    }
                }

                // matrix: std::vector<std::vector<float>>
                {
                    auto* rt = getFieldRecordType("matrix");
                    check(rt != nullptr, "matrix field has RecordType");
                    if (rt) {
                        check(rt->isTemplateSpecialization(), "matrix is a template specialization");
                        std::cout << "  matrix template_name: " << rt->template_name << std::endl;
                        check(rt->template_name.find("vector") != std::string::npos,
                              "matrix template_name contains 'vector'");
                        // The first template arg should itself be a vector<float>
                        if (!rt->template_arguments.empty()) {
                            auto& first_arg = rt->template_arguments[0];
                            check(first_arg.kind == TemplateArgument::Kind::Type,
                                  "matrix first arg is a Type argument");
                            auto* inner = dynamic_cast<const RecordType*>(first_arg.type);
                            if (inner) {
                                check(inner->isTemplateSpecialization(),
                                      "matrix inner type is also a template specialization");
                                check(inner->template_name.find("vector") != std::string::npos,
                                      "matrix inner template_name contains 'vector'");
                            }
                        }
                    }
                }
            }
        }
        check(found, "ECSComponent found");
    }

    // 23. Member exclusion via LUX_META(no_reflect)
    {
        bool found = false;
        for (auto* r : records) {
            if (r->full_qualified_name == "SelectiveReflectStruct") {
                found = true;
                // Should have only 2 visible fields (visible_field_1, visible_field_2)
                // hidden_field and another_hidden are excluded
                check(r->field_decls.size() == 2, 
                      "SelectiveReflectStruct has 2 visible fields (hidden excluded)");
                
                // Verify the visible fields are the right ones
                bool has_visible_1 = false, has_visible_2 = false;
                bool has_hidden = false;
                for (auto fidx : r->field_decls) {
                    auto* f = meta.getDeclAs<FieldDecl>(fidx);
                    if (!f) continue;
                    if (f->spelling == "visible_field_1") has_visible_1 = true;
                    if (f->spelling == "visible_field_2") has_visible_2 = true;
                    if (f->spelling == "hidden_field" || f->spelling == "another_hidden") 
                        has_hidden = true;
                }
                check(has_visible_1, "visible_field_1 is present");
                check(has_visible_2, "visible_field_2 is present");
                check(!has_hidden, "hidden fields are NOT present");

                // Should have only 2 visible methods (visible_method, visible_getter)
                // hidden_method and hidden_setter are excluded
                check(r->method_decls.size() == 2,
                      "SelectiveReflectStruct has 2 visible methods (hidden excluded)");
                
                bool has_visible_method = false, has_visible_getter = false;
                bool has_hidden_method = false;
                for (auto midx : r->method_decls) {
                    auto* m = meta.getDeclAs<CXXMethodDecl>(midx);
                    if (!m) continue;
                    if (m->spelling == "visible_method") has_visible_method = true;
                    if (m->spelling == "visible_getter") has_visible_getter = true;
                    if (m->spelling == "hidden_method" || m->spelling == "hidden_setter")
                        has_hidden_method = true;
                }
                check(has_visible_method, "visible_method is present");
                check(has_visible_getter, "visible_getter is present");
                check(!has_hidden_method, "hidden methods are NOT present");

                std::cout << "  SelectiveReflectStruct fields: " << r->field_decls.size() << std::endl;
                for (auto fidx : r->field_decls) {
                    auto* f = meta.getDeclAs<FieldDecl>(fidx);
                    if (f) std::cout << "    field: " << f->spelling << std::endl;
                }
                std::cout << "  SelectiveReflectStruct methods: " << r->method_decls.size() << std::endl;
                for (auto midx : r->method_decls) {
                    auto* m = meta.getDeclAs<CXXMethodDecl>(midx);
                    if (m) std::cout << "    method: " << m->spelling << std::endl;
                }
            }
        }
        check(found, "SelectiveReflectStruct found");
    }

    // Round-trip serialization test
    {
        std::string json_str = meta_json_str;
        MetaUnit deserialized;
        MetaUnit::fromJson(json_str, deserialized);
        auto reserialized = deserialized.toJson().dump(4);
        check(meta_json_str == reserialized, "Round-trip serialization matches");
    }

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Tests: " << (test_failures == 0 ? "ALL PASSED" : "SOME FAILED") << std::endl;
    std::cout << "Failures: " << test_failures << std::endl;
    std::cout << "Parse errors: " << error_count << std::endl;

    return test_failures;
}
