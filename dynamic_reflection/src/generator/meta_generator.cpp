/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <filesystem>

#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/algotithm/hash.hpp>
#include <lux/cxx/algotithm/string_operations.hpp>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

#include "static_meta.inja.hpp"
#include "dynamic_meta.inja.hpp"

/**
 * @brief Fetch additional include paths from compile_commands.json for a given source file.
 *
 * The compile_commands.json is searched for entries corresponding to @p source_file_path.
 * Any "-I" or "-external:I" arguments found in those entries are turned into strings prefixed
 * with "-I" that can be passed to the compiler invocation.
 *
 * @param compile_command_path Path to compile_commands.json.
 * @param source_file_path The source file to match in compile_commands.json.
 * @return A list of strings (e.g. {"-I/path/to/include", ...}).
 */
static std::vector<std::string> fetchIncludePaths(const std::filesystem::path& compile_command_path,
    const std::filesystem::path& source_file_path);

/**
 * @brief Converts an EVisibility enum value to a string for C++ code generation or debugging.
 *
 * @param visibility The EVisibility value (PUBLIC, PROTECTED, PRIVATE, INVALID).
 * @return A const char* representing the visibility in a descriptive form.
 */
static const char* visibility2Str(lux::cxx::dref::EVisibility visibility)
{
    using lux::cxx::dref::EVisibility;
    switch (visibility)
    {
    case EVisibility::PUBLIC:
        return "EVisibility::PUBLIC";
    case EVisibility::PRIVATE:
        return "EVisibility::PRIVATE";
    case EVisibility::PROTECTED:
        return "EVisibility::PROTECTED";
    default:
        return "EVisibility::INVALID";
    }
}

/**
 * @brief Configuration for the meta code generator.
 *
 * This struct contains paths and file suffixes for generating static and dynamic
 * meta-information files.
 */
struct GeneratorConfig
{
    std::filesystem::path target_dir;       ///< Directory where generated files will be placed.
    std::string           file_name;        ///< Base name of the output file.
    std::string           static_meta_suffix;   ///< Suffix for static meta files (e.g. ".meta.static.hpp").
    std::string           dynamic_meta_suffix;  ///< Suffix for dynamic meta files (e.g. ".meta.dynamic.hpp").
};

/**
 * @brief A meta code generator that processes C++ declarations from a reflection
 *        data structure and produces static and dynamic metadata files.
 *
 * The generator uses inja templates for rendering. Declarations can be marked
 * with attributes such as "static_reflection" or "dynamic_reflection" to control
 * what data gets generated.
 */
class MetaGenerator
{
public:
    /**
     * @brief Possible error states when generating meta files.
     */
    enum class EGenerateError
    {
        SUCCESS,   ///< Generation succeeded with no errors.
        UNMARKED,  ///< Declarations are unmarked (no reflection attributes), skipped.
        FAILED     ///< Failed due to an exception or error during rendering.
    };

    /**
     * @brief Helper structure for collecting JSON data needed by inja templates.
     */
    struct GenerationContext
    {
        nlohmann::json static_records = nlohmann::json::array();
        nlohmann::json static_enums = nlohmann::json::array();
        nlohmann::json dynamic_records = nlohmann::json::array();
        nlohmann::json dynamic_enums = nlohmann::json::array();
    };

    /**
     * @brief Constructor that captures the generator's configuration.
     *
     * @param config An instance of GeneratorConfig.
     */
    MetaGenerator(GeneratorConfig config)
        : _config(std::move(config))
    {
    }

    /**
     * @brief Main function to generate metadata from a MetaUnit (which contains reflected declarations).
     *
     * @param unit The reflection data unit containing declarations.
     * @return EGenerateError indicating success or failure.
     */
    EGenerateError generate(const lux::cxx::dref::MetaUnit& unit)
    {
        // (1) Gather static metadata
        nlohmann::json static_data;

        // (2) Gather dynamic metadata
        nlohmann::json dynamic_data;

        // Context to hold intermediate JSON arrays
        GenerationContext context;

        // Iterate over all "marked" declarations
        for (auto* decl : unit.markedDeclarations())
        {
            declarationDispatch(decl, context);
        }

        // Prepare final data structures for the inja templates
        static_data["classes"] = context.static_records;
        static_data["enums"] = context.static_enums;

        dynamic_data["classes"] = context.dynamic_records;
        dynamic_data["enums"] = context.dynamic_enums;

        // (3) Render static meta template
        try
        {
            inja::Environment env;
            inja::Template tpl = env.parse(static_meta_template.data());
            std::string result = env.render(tpl, static_data);

            // Write output to file
            auto output_path = _config.target_dir / (_config.file_name + _config.static_meta_suffix);
            std::ofstream ofs(output_path);
            ofs << result;
            ofs.close();
        }
        catch (std::exception& e)
        {
            std::cerr << "[MetaGenerator] Error rendering static meta: " << e.what() << std::endl;
            return EGenerateError::FAILED;
        }

        // (4) Render dynamic meta template
        try
        {
            inja::Environment env;
            inja::Template tpl = env.parse(dynamic_meta_template.data());
            std::string result = env.render(tpl, dynamic_data);

            // Write output to file
            auto output_path = _config.target_dir / (_config.file_name + _config.dynamic_meta_suffix);
            std::ofstream ofs(output_path);
            ofs << result;
            ofs.close();
        }
        catch (std::exception& e)
        {
            std::cerr << "[MetaGenerator] Error rendering dynamic meta: " << e.what() << std::endl;
            return EGenerateError::FAILED;
        }

        return EGenerateError::SUCCESS;
    }

private:
    /**
     * @brief Internal enum for types of reflection marking found in attributes.
     */
    enum class EMarkedType : uint8_t
    {
        STATIC = 0,
        DYNAMIC,
        BOTH,
        NONE
    };

    /**
     * @brief Determines how a NamedDecl is marked (static, dynamic, both, or none)
     *        based on its attributes.
     *
     * @param decl The declaration to inspect.
     * @return EMarkedType indicating the reflection type.
     */
    EMarkedType markedType(lux::cxx::dref::NamedDecl* decl)
    {
        // The transition table helps combine multiple attributes:
        // * If we see "static_reflection" or "dynamic_reflection" repeatedly,
        //   we may evolve from NONE to STATIC, or to BOTH, etc.
        static std::array<std::array<EMarkedType, 2>, 4> table{
            std::array<EMarkedType, 2>{EMarkedType::STATIC, EMarkedType::BOTH},      // from NONE
            std::array<EMarkedType, 2>{EMarkedType::BOTH,   EMarkedType::DYNAMIC},   // from STATIC
            std::array<EMarkedType, 2>{EMarkedType::BOTH,   EMarkedType::BOTH},      // from DYNAMIC
            std::array<EMarkedType, 2>{EMarkedType::STATIC, EMarkedType::DYNAMIC}    // from BOTH
        };

        EMarkedType result = EMarkedType::NONE;

        for (const auto& attr : decl->attributes)
        {
            if (attr == "static_reflection")
            {
                result = table[static_cast<uint8_t>(result)][0];
            }
            if (attr == "dynamic_reflection")
            {
                result = table[static_cast<uint8_t>(result)][1];
            }
        }
        return result;
    }

    /**
     * @brief Dispatches a declaration to the appropriate metadata-generating functions,
     *        depending on how it is marked for reflection (static, dynamic, or both).
     *
     * @param decl The declaration to process.
     * @param context JSON context to store generated data.
     * @return EGenerateError indicating success or if the decl was unmarked.
     */
    EGenerateError declarationDispatch(lux::cxx::dref::Decl* decl, GenerationContext& context)
    {
        using namespace lux::cxx::dref;
        auto* namedDecl = dynamic_cast<NamedDecl*>(decl);
        auto mark = markedType(namedDecl);

        if (mark == EMarkedType::STATIC)
        {
            generateStaticMeta(namedDecl, context);
        }
        else if (mark == EMarkedType::DYNAMIC)
        {
            generateDynamicMeta(namedDecl, context);
        }
        else if (mark == EMarkedType::BOTH)
        {
            generateStaticMeta(namedDecl, context);
            generateDynamicMeta(namedDecl, context);
        }
        else
        {
            // Declarations that are not marked will do nothing here.
            // This can be changed to a different return if desired.
            return EGenerateError::UNMARKED;
        }

        return EGenerateError::SUCCESS;
    }

    /**
     * @brief Generates static metadata for a NamedDecl (if it is a class or enum).
     */
    bool generateStaticMeta(lux::cxx::dref::NamedDecl* decl, GenerationContext& context)
    {
        using namespace lux::cxx::dref;
        if (decl->kind == EDeclKind::CXX_RECORD_DECL)
        {
            return generateStaticMeta(
                dynamic_cast<CXXRecordDecl&>(*decl), context.static_records
            );
        }
        else if (decl->kind == EDeclKind::ENUM_DECL)
        {
            return generateStaticMeta(
                dynamic_cast<EnumDecl&>(*decl), context.static_enums
            );
        }
        return false;
    }

    /**
     * @brief Generates dynamic metadata for a NamedDecl (if it is a class or enum).
     */
    bool generateDynamicMeta(lux::cxx::dref::NamedDecl* decl, GenerationContext& context)
    {
        using namespace lux::cxx::dref;
        if (decl->kind == EDeclKind::CXX_RECORD_DECL)
        {
            return generateDynamicMeta(
                dynamic_cast<CXXRecordDecl&>(*decl), context.dynamic_records
            );
        }
        else if (decl->kind == EDeclKind::ENUM_DECL)
        {
            return generateDynamicMeta(
                dynamic_cast<EnumDecl&>(*decl), context.dynamic_enums
            );
        }
        return false;
    }

    //-------------------------------------------------------------------------
    // Static Metadata Generators
    //-------------------------------------------------------------------------

    /**
     * @brief Generate static metadata for a CXXRecordDecl (class/struct).
     *
     * This populates data such as record name, alignment, fields, methods,
     * static methods, and so on. The result is appended to the @p classes JSON array.
     */
    bool generateStaticMeta(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& classes)
    {
        using namespace lux::cxx::dref;

        nlohmann::json record_info = nlohmann::json::object();
        record_info["name"]  = decl.full_qualified_name;
		record_info["extended_name"] = lux::cxx::algorithm::replace(decl.full_qualified_name, "::", "_");
        record_info["align"] = decl.type->align;
        record_info["record_type"] =
            (decl.tag_kind == TagDecl::ETagKind::Class) ? "EMetaType::CLASS" : "EMetaType::STRUCT";
        record_info["fields_count"] = static_cast<int>(decl.field_decls.size());

        // Helper lambda to build function parameter JSON
        auto build_params = [](const auto& params_vector) -> nlohmann::json
        {
            nlohmann::json params = nlohmann::json::array();
            size_t index = 0;
            for (const auto& param : params_vector)
            {
                nlohmann::json param_map;
                param_map["type"] = param->type->name;
                param_map["hash"] = lux::cxx::algorithm::fnv1a(param->type->name);
                param_map["last"] = (index == params_vector.size() - 1);
                params.push_back(param_map);
                ++index;
            }
            return params;
        };

        // Helper lambda to build method name list and type info
        auto build_function_context = [&](const auto& methods) -> std::pair<nlohmann::json, nlohmann::json>
        {
            nlohmann::json names = nlohmann::json::array();
            nlohmann::json types = nlohmann::json::array();
            size_t count = 0;

            for (const auto& method : methods)
            {
                // Collect method names
                nlohmann::json name_obj;
                name_obj["name"] = method->spelling;
                names.push_back(name_obj);

                // Collect method type info
                nlohmann::json type_obj;
                type_obj["return_type"] = method->result_type->name;
                type_obj["class_name"] = decl.name;
                type_obj["function_name"] = method->spelling;
                type_obj["parameters"] = build_params(method->params);
                type_obj["last"] = (count == methods.size() - 1);
                type_obj["is_const"] = method->is_const;

                types.push_back(type_obj);
                ++count;
            }
            return std::make_pair(names, types);
        };

        // Fields
        {
            nlohmann::json fields = nlohmann::json::array();
            nlohmann::json field_types = nlohmann::json::array();
            size_t count = 0;

            for (auto* field : decl.field_decls)
            {
                // Basic field metadata
                nlohmann::json field_obj;
                field_obj["name"] = field->name;
                // Dividing offset by 8 to represent byte offset if offset is in bits.
                field_obj["offset"] = std::to_string(field->offset / 8);
                field_obj["visibility"] = visibility2Str(field->visibility);
                field_obj["index"] = std::to_string(count);
                fields.push_back(field_obj);

                // The field type info for the template
                nlohmann::json ft_obj;
                ft_obj["value"] = field->type->name;
                ft_obj["last"] = (count == decl.field_decls.size() - 1);
                field_types.push_back(ft_obj);

                ++count;
            }
            record_info["fields"] = fields;
            record_info["field_types"] = field_types;
        }

        // Attributes
        {
            nlohmann::json attributes = nlohmann::json::array();
            for (const auto& attr : decl.attributes)
            {
                attributes.push_back(attr);
            }
            record_info["attributes"] = attributes;
            record_info["attributes_count"] = std::to_string(attributes.size());
        }

        // Non-static methods
        {
            auto [func_names, func_types] = build_function_context(decl.method_decls);
            record_info["funcs"] = func_names;
            record_info["func_types"] = func_types;
            record_info["funcs_count"] = std::to_string(func_names.size());
        }

        // Static methods
        {
            auto [static_func_names, static_func_types] = build_function_context(decl.static_method_decls);
            record_info["static_funcs"] = static_func_names;
            record_info["static_func_types"] = static_func_types;
            record_info["static_funcs_count"] = std::to_string(static_func_names.size());
        }

        classes.push_back(record_info);
        return true;
    }

    /**
     * @brief Generate static metadata for an EnumDecl.
     *
     * This collects enum name, enumerators, their values, and attributes,
     * and appends them to the @p enums JSON array.
     */
    bool generateStaticMeta(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums)
    {
        using namespace lux::cxx::dref;

        nlohmann::json enum_info = nlohmann::json::object();
        enum_info["name"] = decl.full_qualified_name;
        enum_info["extended_name"] = lux::cxx::algorithm::replace(decl.full_qualified_name, "::", "_");
        enum_info["size"] = std::to_string(decl.enumerators.size());

        // Keys (enumerator names)
        nlohmann::json keys = nlohmann::json::array();
        for (const auto& enumerator : decl.enumerators)
        {
            keys.push_back(enumerator.name);
        }
        enum_info["keys"] = keys;

        // Values (signed or unsigned)
        nlohmann::json values = nlohmann::json::array();
        if (decl.underlying_type->isSignedInteger())
        {
            for (const auto& enumerator : decl.enumerators)
            {
                values.push_back(std::to_string(enumerator.signed_value));
            }
        }
        else
        {
            for (const auto& enumerator : decl.enumerators)
            {
                values.push_back(std::to_string(enumerator.unsigned_value));
            }
        }
        enum_info["values"] = values;

        // Combined elements (key + value)
        nlohmann::json elements = nlohmann::json::array();
        if (decl.underlying_type->isSignedInteger())
        {
            for (const auto& enumerator : decl.enumerators)
            {
                nlohmann::json e;
                e["key"] = enumerator.name;
                e["value"] = std::to_string(enumerator.signed_value);
                elements.push_back(e);
            }
        }
        else
        {
            for (const auto& enumerator : decl.enumerators)
            {
                nlohmann::json e;
                e["key"] = enumerator.name;
                e["value"] = std::to_string(enumerator.unsigned_value);
                elements.push_back(e);
            }
        }
        enum_info["elements"] = elements;

        // Attributes
        nlohmann::json attributes = nlohmann::json::array();
        for (const auto& attr : decl.attributes)
        {
            attributes.push_back(attr);
        }
        enum_info["attributes"] = attributes;
        enum_info["attributes_count"] = std::to_string(attributes.size());

        // Cases (essentially the enumerator names again)
        nlohmann::json cases = nlohmann::json::array();
        for (const auto& enumerator : decl.enumerators)
        {
            cases.push_back(enumerator.name);
        }
        enum_info["cases"] = cases;

        enums.push_back(enum_info);
        return true;
    }

    //-------------------------------------------------------------------------
    // Dynamic Metadata Generators
    //-------------------------------------------------------------------------

    /**
     * @brief Generate dynamic metadata for a CXXRecordDecl (class/struct).
     *
     * This includes methods, static methods, constructors, fields, and other
     * information that could be used at runtime for introspection.
     */
    bool generateDynamicMeta(lux::cxx::dref::CXXRecordDecl& decl, nlohmann::json& records)
    {
        using namespace lux::cxx::dref;

        // JSON object storing the record's dynamic info
        nlohmann::json record_info = nlohmann::json::object();
        record_info["name"] = decl.full_qualified_name; // or decl.name
        std::string extended_name = lux::cxx::algorithm::replace(decl.full_qualified_name, "::", "_");
        record_info["extended_name"] = extended_name;

        // 1) Instance methods
        {
            nlohmann::json methods_json = nlohmann::json::array();
            for (auto* method : decl.method_decls)
            {
                nlohmann::json method_info;
                method_info["name"] = method->spelling;
                method_info["return_type"] = method->result_type->name;
                method_info["is_const"] = method->is_const;
                method_info["is_virtual"] = method->is_virtual;
                method_info["is_static"] = false;

                // Gather parameters
                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < method->params.size(); ++i)
                {
                    auto* param_decl = method->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                method_info["parameters"] = params;

                // Create a bridge function name for runtime invocation
                std::string bridge_name = extended_name + "_" + method->spelling + "_invoker";
                method_info["bridge_name"] = bridge_name;

                methods_json.push_back(method_info);
            }
            record_info["methods"] = methods_json;
        }

        // 2) Static methods
        {
            nlohmann::json static_methods_json = nlohmann::json::array();
            for (auto* method : decl.static_method_decls)
            {
                nlohmann::json method_info;
                method_info["name"] = method->spelling;
                method_info["return_type"] = method->result_type->name;
                method_info["is_static"] = true;

                // Gather parameters
                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < method->params.size(); ++i)
                {
                    auto* param_decl = method->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                method_info["parameters"] = params;

                // Create a bridge name for static invocations
                std::string bridge_name = extended_name + "_" + method->spelling + "_static_invoker";
                method_info["bridge_name"] = bridge_name;

                static_methods_json.push_back(method_info);
            }
            record_info["static_methods"] = static_methods_json;
        }

        // 3) Constructors
        {
            nlohmann::json ctors_json = nlohmann::json::array();
            for (auto* ctor : decl.constructor_decls)
            {
                nlohmann::json ctor_info;
                ctor_info["ctor_name"] = ctor->spelling; // Typically same as class name

                // Gather parameters
                nlohmann::json params = nlohmann::json::array();
                for (size_t i = 0; i < ctor->params.size(); ++i)
                {
                    auto* param_decl = ctor->params[i];
                    nlohmann::json pj;
                    pj["type_name"] = param_decl->type->name;
                    pj["index"] = i;
                    params.push_back(pj);
                }
                ctor_info["parameters"] = params;

                // Construct a unique bridge name for the constructor
                std::string bridge_name = extended_name + "_ctor_" +
                    std::to_string(reinterpret_cast<uintptr_t>(ctor));
                ctor_info["bridge_name"] = bridge_name;

                ctors_json.push_back(ctor_info);
            }
            record_info["constructor_num"] = decl.constructor_decls.size();
            record_info["constructors"] = ctors_json;
        }

        // 4) Fields
        {
            nlohmann::json fields_json = nlohmann::json::array();
            for (auto* field : decl.field_decls)
            {
                nlohmann::json f;
                f["name"] = field->name;
                f["type_name"] = field->type->name;
                f["offset"] = static_cast<int>(field->offset / 8);
                f["visibility"] = visibility2Str(field->visibility);
                f["is_static"] = false; // For static member variables, you would handle them separately
                fields_json.push_back(f);
            }
            record_info["fields"] = fields_json;
        }

        // Add this record_info object into the array
        records.push_back(record_info);
        return true;
    }

    /**
     * @brief Generate dynamic metadata for an EnumDecl.
     *
     * Collects enumerator names and values (signed or unsigned), whether it is a
     * scoped enum, and the name of its underlying type. Appends the result to the
     * @p enums JSON array.
     */
    bool generateDynamicMeta(lux::cxx::dref::EnumDecl& decl, nlohmann::json& enums)
    {
        using namespace lux::cxx::dref;

        nlohmann::json enum_info;
        enum_info["name"] = decl.full_qualified_name; // or decl.name
        enum_info["extended_name"] = lux::cxx::algorithm::replace(decl.full_qualified_name, "::", "_");

        // Collect enumerators
        nlohmann::json enumerators_json = nlohmann::json::array();
        for (auto& enumerator : decl.enumerators)
        {
            nlohmann::json e;
            e["name"] = enumerator.name;
            e["value"] = (decl.underlying_type->isSignedInteger())
                ? std::to_string(enumerator.signed_value)
                : std::to_string(enumerator.unsigned_value);
            enumerators_json.push_back(e);
        }
        enum_info["enumerators"] = enumerators_json;

        // Check if it's a scoped enum
        enum_info["is_scoped"] = decl.is_scoped;
        enum_info["underlying_type_name"] = decl.underlying_type->name;

        enums.push_back(enum_info);
        return true;
    }

private:
    GeneratorConfig _config; ///< Internal config for file paths, suffixes, etc.
};

//=============================================================================================
//  Main entry point
//=============================================================================================

/**
 * @brief Main function for the meta_generator tool.
 *
 * Usage:
 *   - argv[1]: The file path to parse for reflection data (e.g., a header file).
 *   - argv[2]: The output directory for generated files.
 *   - argv[3]: A source file that needs metadata (must appear in compile_commands.json).
 *   - argv[4]: The path to compile_commands.json.
 *
 * The program will parse the file using libclang, extract reflection data,
 * then produce static and dynamic metadata files under the output directory.
 */
int main(const int argc, const char* argv[])
{
    if (argc < 5)
    {
        return 1;
    }

    // Input arguments
    static auto target_file = argv[1];
    static auto output_path = argv[2];
    static auto source_file = argv[3];
    static auto compile_command_path = argv[4];

    inja::Environment env;

    // Basic checks for file existence
    if (!std::filesystem::exists(target_file))
    {
        std::cerr << "File " << target_file << " does not exist" << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(source_file))
    {
        std::cerr << "Source file " << source_file << " does not exist" << std::endl;
        return 1;
    }
    if (!std::filesystem::exists(compile_command_path))
    {
        std::cerr << "Compile command file " << compile_command_path << " does not exist" << std::endl;
        return 1;
    }

    // Gather include paths from compile_commands.json
    auto include_paths = fetchIncludePaths(compile_command_path, source_file);

    // Prepare Clang compile options
    const lux::cxx::dref::CxxParser cxx_parser;
    std::vector<std::string> options;
    options.emplace_back("--std=c++20");
    for (auto& path : include_paths)
    {
        options.emplace_back(std::move(path));
    }

    // Print compile options for debugging
    std::cout << "Compile options:";
    for (const auto& opt : options)
    {
        std::cout << "\t" << opt;
    }
	std::cout << std::endl;

    // Parse the target file using the reflection parser
    auto [parse_rst, data] = cxx_parser.parse(
        target_file,
        std::move(options),
        "test_unit",
        "1.0.0"
    );

    if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS)
    {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }

    // Construct output file name (remove extension)
    auto file_name = std::filesystem::path(target_file).filename().replace_extension("").string();

    // Prepare generator config
    GeneratorConfig config{
        .target_dir = std::filesystem::path(output_path),
        .file_name = file_name,
        .static_meta_suffix = ".meta.static.hpp",
        .dynamic_meta_suffix = ".meta.dynamic.hpp"
    };

    // Instantiate and run the meta generator
    MetaGenerator generator(config);
    auto generate_rst = generator.generate(data);

    if (generate_rst != MetaGenerator::EGenerateError::SUCCESS)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief Helper function to split a command string into tokens.
 *
 * @param cmd A command line string (e.g. "clang++ -I/path ...").
 * @return A vector of tokens split by whitespace.
 */
static std::vector<std::string> splitCommand(const std::string& cmd)
{
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token)
    {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief Constructs an absolute path from a base directory and a relative path.
 *
 * @param baseDir The base directory (absolute path).
 * @param p The potential relative path.
 * @return An absolute path (if p is relative, it is joined with baseDir).
 */
static std::filesystem::path makeAbsolute(const std::filesystem::path& baseDir, const std::filesystem::path& p)
{
    if (p.is_absolute())
        return p;
    return std::filesystem::absolute(baseDir / p);
}

/**
 * @brief Checks if a string conforms to the Windows standard absolute path format,
 *        e.g., "C:\\", "D:\\", etc.
 *
 * @param s The string to check.
 * @return True if it matches a pattern like "D:\\..." or "C:/...", otherwise false.
 */
static bool isStandardAbsolute(const std::string& s)
{
    return (s.size() >= 3 && std::isalpha(s[0]) && s[1] == ':' && (s[2] == '\\' || s[2] == '/'));
}

/**
 * @brief Fetch include paths from compile_commands.json corresponding to a given source file.
 *
 * This function searches for an entry in compile_commands.json where "file" matches
 * @p source_file_path, then collects any arguments that begin with "-I" or "-external:I",
 * converting them into a consistent "-I" prefix for later use.
 *
 * @param compile_command_path The path to compile_commands.json.
 * @param source_file_path The source file name to look up in compile_commands.json.
 * @return A list of include paths, each prefixed with "-I".
 */
std::vector<std::string> fetchIncludePaths(const std::filesystem::path& compile_command_path,
    const std::filesystem::path& source_file_path)
{
    std::ifstream ifs(compile_command_path, std::ios::binary);
    if (!ifs)
    {
        std::cerr << "Failed to open " << compile_command_path << std::endl;
        return {};
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string jsonContent = buffer.str();
    ifs.close();

    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    if (doc.HasParseError() || !doc.IsArray())
    {
        std::cerr << "Failed to parse compile_commands.json or root is not an array." << std::endl;
        return {};
    }

    std::set<std::string> includePaths;

    // Scan every entry in compile_commands.json
    for (auto& entry : doc.GetArray())
    {
        if (!entry.IsObject())
            continue;

        if (!entry.HasMember("file") || !entry.HasMember("directory"))
            continue;

        std::string fileStr = entry["file"].GetString();

        // Simple filename matching (might need path normalization for real-world usage)
        if (std::filesystem::path(fileStr) != source_file_path)
            continue;

        std::filesystem::path baseDir = entry["directory"].GetString();
        std::vector<std::string> args;

        // If "arguments" field exists, use it directly.
        if (entry.HasMember("arguments") && entry["arguments"].IsArray())
        {
            for (auto& arg : entry["arguments"].GetArray())
            {
                args.push_back(arg.GetString());
            }
        }
        // Otherwise, if "command" field exists, split it into tokens.
        else if (entry.HasMember("command") && entry["command"].IsString())
        {
            std::string cmdStr = entry["command"].GetString();
            args = splitCommand(cmdStr);
        }

        // Search for arguments starting with "-I" or "-external:I"
        for (size_t i = 0; i < args.size(); ++i)
        {
            std::string arg = args[i];
            std::string pathStr;

            // Handle -I
            if (arg.rfind("-I", 0) == 0)
            {
                if (arg == "-I")
                {
                    // -I <path>
                    if (i + 1 < args.size())
                    {
                        pathStr = args[++i];
                    }
                }
                else
                {
                    // -I<path>
                    pathStr = arg.substr(2);
                }
            }
            // Handle -external:I
            else if (arg.rfind("-external:I", 0) == 0)
            {
                if (arg == "-external:I")
                {
                    if (i + 1 < args.size())
                    {
                        pathStr = args[++i];
                    }
                }
                else
                {
                    pathStr = arg.substr(std::string("-external:I").length());
                }
                // We still store it as "-I" + path
            }

            if (!pathStr.empty())
            {
                std::filesystem::path incPath;
                // If pathStr is a standard absolute path (Windows style), use it directly
                if (isStandardAbsolute(pathStr))
                {
                    incPath = std::filesystem::path(pathStr);
                }
                else
                {
                    // Otherwise construct an absolute path relative to baseDir
                    incPath = makeAbsolute(baseDir, pathStr);
                }
                includePaths.insert("-I" + incPath.string());
            }
        }
    }

    return std::vector<std::string>(includePaths.begin(), includePaths.end());
}
