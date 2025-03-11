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
#include <filesystem>
#include <optional>
#include <set>
#include <array>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <inja/inja.hpp>
#include <nlohmann/json.hpp>

 // Your reflection/AST parsing library headers
#include <lux/cxx/dref/parser/CxxParser.hpp>
#include <lux/cxx/dref/parser/libclang.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/algotithm/hash.hpp>
#include <lux/cxx/algotithm/string_operations.hpp>

// The text of your Inja templates:
#include "static_meta.inja.hpp"
#include "dynamic_meta.inja.hpp"

// ------------------------------------------------------
// Utility to parse the compile_commands "command"/"arguments" fields
// and split them into tokens
// ------------------------------------------------------
static std::vector<std::string> splitCommand(const std::string& cmd)
{
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// ------------------------------------------------------
// Convert a baseDir + path to an absolute path
// ------------------------------------------------------
static std::filesystem::path makeAbsolute(
    const std::filesystem::path& baseDir,
    const std::filesystem::path& p)
{
    if (p.is_absolute())
        return p;
    return std::filesystem::absolute(baseDir / p);
}

// ------------------------------------------------------
// Check if a string is a Windows-style absolute path
// e.g., "C:\..." or "D:/..."
// ------------------------------------------------------
static bool isStandardAbsolute(const std::string& s)
{
    return (s.size() >= 3 && std::isalpha(s[0]) && s[1] == ':' &&
        (s[2] == '\\' || s[2] == '/'));
}

// -------------------------------------------------------------------------
// Attempt to see if a file is inside one of the 'includeList' directories
// If so, return the relative path, otherwise std::nullopt
// -------------------------------------------------------------------------
static std::optional<std::filesystem::path> findRelativeIncludePath(
    const std::filesystem::path& metaFile,
    const std::vector<std::filesystem::path>& includeList)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    // Normalize the metaFile path
    fs::path absFile = fs::canonical(metaFile, ec);
    if (ec) {
        // fallback to absolute
        ec.clear();
        absFile = fs::absolute(metaFile, ec);
        if (ec) {
            return std::nullopt;
        }
    }

    for (const auto& incPath : includeList)
    {
        ec.clear();
        fs::path absInc = fs::canonical(incPath, ec);
        if (ec) {
            ec.clear();
            absInc = fs::absolute(incPath, ec);
            if (ec) {
                // skip if we fail
                continue;
            }
        }

        fs::path rel = fs::relative(absFile, absInc, ec);
        if (!ec && !rel.empty() && *rel.begin() != "..") {
            return rel; // e.g. "subdir/foo.hpp"
        }
    }
    return std::nullopt;
}

// -------------------------------------------------------------------------
// Convert EVisibility to string for code gen
// -------------------------------------------------------------------------
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

// -------------------------------------------------------------------------
// The meta code generator classes
// (similar to your original code, omitted for brevity)...
// ...
// [Paste your MetaGenerator class definition here, or keep it in separate .hpp/.cpp]
// For illustration, we keep the key points only
// -------------------------------------------------------------------------

// ========== Begin: Sample MetaGenerator Implementation ============

struct GeneratorConfig
{
    std::filesystem::path target_dir;
    std::string           file_name;
    std::string           file_hash;
    std::string           relative_include;
    std::string           static_meta_suffix;
    std::string           dynamic_meta_suffix;
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
        static_data["classes"]   = context.static_records;
        static_data["class_num"] = context.static_records.size();
        static_data["enums"]     = context.static_enums;
		static_data["enum_num"]  = context.static_enums.size();

        dynamic_data["classes"]  = context.dynamic_records;
		dynamic_data["class_num"]= context.dynamic_records.size();
        dynamic_data["enums"]    = context.dynamic_enums;
		dynamic_data["enum_num"] = context.dynamic_enums.size();

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

        dynamic_data["file_hash"]    = _config.file_hash;
        dynamic_data["include_path"] = _config.relative_include;
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
        record_info["name"] = decl.full_qualified_name;
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
                    type_obj["class_name"] = decl.full_qualified_name;
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


// ========== End: Sample MetaGenerator Implementation ============

// -------------------------------------------------------------------------
//  Implementation of 'fetchIncludePaths'
//  Reads compile_commands.json for "command" or "arguments" that match the
//  given source_file_path. Then extracts any "-I" or "-external:I" options.
// -------------------------------------------------------------------------
static std::vector<std::filesystem::path> fetchIncludePaths(
    const std::filesystem::path& compile_command_path,
    const std::filesystem::path& source_file_path)
{
    namespace fs = std::filesystem;
    std::ifstream ifs(compile_command_path, std::ios::binary);
    if (!ifs) {
        std::cerr << "[fetchIncludePaths] Cannot open " << compile_command_path << std::endl;
        return {};
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    ifs.close();
    std::string jsonContent = buffer.str();

    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    if (doc.HasParseError() || !doc.IsArray()) {
        std::cerr << "[fetchIncludePaths] Invalid compile_commands.json." << std::endl;
        return {};
    }

    std::set<fs::path> includes;  // store unique filesystem paths
    for (auto& entry : doc.GetArray())
    {
        if (!entry.IsObject()) continue;

        // require "file" & "directory"
        if (!entry.HasMember("file") || !entry.HasMember("directory"))
            continue;

        std::string fileStr = entry["file"].GetString();
        if (fs::path(fileStr) != source_file_path)
            continue;

        fs::path baseDir = entry["directory"].GetString();
        std::vector<std::string> args;

        // If "arguments" exists, read them
        if (entry.HasMember("arguments") && entry["arguments"].IsArray())
        {
            for (auto& arg : entry["arguments"].GetArray())
            {
                args.push_back(arg.GetString());
            }
        }
        // else if "command" exists, parse it
        else if (entry.HasMember("command") && entry["command"].IsString())
        {
            std::string cmdStr = entry["command"].GetString();
            args = splitCommand(cmdStr); // assume you already have a splitCommand(...)
        }

        // gather -I or -external:I
        for (size_t i = 0; i < args.size(); ++i)
        {
            std::string argStr = args[i];
            std::string pathStr;

            // handle -I
            if (argStr.rfind("-I", 0) == 0)
            {
                if (argStr == "-I") {
                    if (i + 1 < args.size()) {
                        pathStr = args[++i];
                    }
                }
                else {
                    // e.g. "-ID:/some/path"
                    pathStr = argStr.substr(2);
                }
            }
            // handle -external:I
            else if (argStr.rfind("-external:I", 0) == 0)
            {
                if (argStr == "-external:I") {
                    if (i + 1 < args.size()) {
                        pathStr = args[++i];
                    }
                }
                else {
                    // e.g. "-external:ID:/some/path"
                    pathStr = argStr.substr(std::string("-external:I").length());
                }
                // we'll store it purely as a path, no prefix
            }

            if (!pathStr.empty())
            {
                fs::path incP;
                // if it's a windows absolute
                if (isStandardAbsolute(pathStr)) {
                    incP = fs::path(pathStr);
                }
                else {
                    incP = makeAbsolute(baseDir, pathStr);
                }
                includes.insert(incP); // store the raw path
            }
        }
    }

    // convert set -> vector
    return std::vector<fs::path>(includes.begin(), includes.end());
}

static std::vector<std::string> convertToDashI(const std::vector<std::filesystem::path>& paths)
{
    std::vector<std::string> result;
    result.reserve(paths.size());
    for (const auto& p : paths)
    {
        result.push_back("-I" + p.string());
    }
    return result;
}

// -------------------------------------------------------------------------
// main()
// Example usage: 
//   meta_generator <file_to_parse> <out_dir> <source_file> <compile_commands> <hash>
// -------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 6) {
        std::cerr << "Usage: meta_generator <file_to_parse> <out_dir> <source_file> <compile_commands> <file_hash>\n";
        return 1;
    }

    std::filesystem::path file_to_parse = argv[1];
    std::filesystem::path out_dir = argv[2];
    std::filesystem::path source_file = argv[3];
    std::filesystem::path compile_cmds = argv[4];
    std::string file_hash = argv[5];

    if (!std::filesystem::exists(file_to_parse)) {
        std::cerr << "[Error] file_to_parse " << file_to_parse << " does not exist.\n";
        return 1;
    }
    if (!std::filesystem::exists(source_file)) {
        std::cerr << "[Warn] source_file " << source_file << " does not exist or not matched.\n";
        // not strictly an error, but we can't find the compile entry
    }
    if (!std::filesystem::exists(compile_cmds)) {
        std::cerr << "[Error] compile_commands " << compile_cmds << " does not exist.\n";
        return 1;
    }

    // gather additional includes from compile_commands
    auto extra_includes = fetchIncludePaths(compile_cmds, source_file);
	auto include_options = convertToDashI(extra_includes);
    auto maybeRel = findRelativeIncludePath(file_to_parse, extra_includes);
    if (!maybeRel)
    {
		std::cerr << "[Error] Could not find a relative include path for " << file_to_parse << "\n";
        return 1;
    }

    // Build final clang parse options
    // e.g. { "--std=c++20", "-I/some/path", ... }
    std::vector<std::string> options;
    options.push_back("--std=c++20");
    for (const auto& inc : include_options) {
        options.push_back(inc);  // e.g. "-ID:/some/include"
    }

    // We can also do something like printing them out for debug
    // std::cerr << "[Debug] Clang parse options:\n";
    // for (const auto& opt : options) {
    //     std::cerr << "    " << opt << "\n";
    // }

    // Now parse the file
    lux::cxx::dref::CxxParser cxx_parser;
    auto [parse_rst, data] = cxx_parser.parse(
        file_to_parse.string(),
        std::move(options),
        "test_unit",
        "1.0.0"
    );

    if (parse_rst != lux::cxx::dref::EParseResult::SUCCESS) {
        std::cerr << "[Error] Parsing of " << file_to_parse << " failed.\n";
        return 1;
    }

    // Construct base filename from the file to parse
    auto base_name = file_to_parse.filename().replace_extension("").string();

    // Setup generator config
    GeneratorConfig config;
    config.target_dir = out_dir;
    config.file_name = base_name;
    config.file_hash = file_hash;
    config.relative_include = lux::cxx::algorithm::replace((*maybeRel).string(), "\\", "/");
    config.static_meta_suffix = ".meta.static.hpp";
    config.dynamic_meta_suffix = ".meta.dynamic.cpp";

    // Create the meta generator
    MetaGenerator generator(config);
    auto gen_result = generator.generate(data);
    if (gen_result != MetaGenerator::EGenerateError::SUCCESS) {
        return 1;
    }

    return 0;
}
