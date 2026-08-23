#include <lux/cxx/reflection/runtime/MetaIrJson.hpp>

#include <nlohmann/json.hpp>

namespace lux::cxx::reflection::ir
{
    namespace
    {
        [[nodiscard]] EMetaNodeKind nodeKind(std::string_view kind) noexcept
        {
            if (kind == "CXXRecordDecl" || kind == "RecordDecl")
                return EMetaNodeKind::RECORD;
            if (kind == "EnumDecl") return EMetaNodeKind::ENUMERATION;
            if (kind == "FunctionDecl" || kind == "CXXMethodDecl"
                || kind == "CXXConstructorDecl" || kind == "CXXDestructorDecl"
                || kind == "CXXConversionDecl")
                return EMetaNodeKind::FUNCTION;
            if (kind == "FieldDecl") return EMetaNodeKind::FIELD;
            if (kind == "ParmVarDecl") return EMetaNodeKind::PARAMETER;
            if (kind == "VarDecl") return EMetaNodeKind::FIELD;
            return EMetaNodeKind::TYPE;
        }

        [[nodiscard]] bool addJsonNode(
            MetaUnitBuilder<>& builder,
            const nlohmann::json& value
        )
        {
            const auto kind_text = value.value("__kind", std::string{"Type"});
            const auto name = value.value(
                "fq_name",
                value.value("name", value.value("id", std::string{}))
            );
            auto node = builder.addNode(nodeKind(kind_text), name);
            if (!node) return false;
            if (!builder.addAttribute(*node, "id", value.value("id", std::string{})))
                return false;
            if (!builder.addAttribute(*node, "kind", kind_text)) return false;
            if (!builder.addAttribute(*node, "template_node", value.dump())) return false;
            return true;
        }
    }

    expected<MetaUnit, EMetaIrJsonError> makeMetaUnitFromTemplateJson(
        std::string_view json_text
    )
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(json_text);
        }
        catch (const nlohmann::json::exception&)
        {
            return unexpected(EMetaIrJsonError::INVALID_JSON);
        }
        if (!root.is_object())
            return unexpected(EMetaIrJsonError::INVALID_SHAPE);

        MetaUnitBuilder builder;
        const auto declaration_count = root.contains("declarations")
            && root["declarations"].is_array()
            ? root["declarations"].size()
            : 0;
        const auto type_count = root.contains("types") && root["types"].is_array()
            ? root["types"].size()
            : 0;
        builder.reserve(
            1 + declaration_count + type_count,
            json_text.size() * 2,
            1 + (declaration_count + type_count) * 3
        );
        auto unit = builder.addNode(
            EMetaNodeKind::NAMESPACE,
            root.value("name", std::string{"meta_unit"})
        );
        if (!unit || !builder.addAttribute(*unit, "template_json", json_text))
            return unexpected(EMetaIrJsonError::IR_LIMIT);

        if (root.contains("declarations") && root["declarations"].is_array())
        {
            for (const auto& declaration : root["declarations"])
            {
                if (!declaration.is_object())
                    return unexpected(EMetaIrJsonError::INVALID_SHAPE);
                if (!addJsonNode(builder, declaration))
                    return unexpected(EMetaIrJsonError::IR_LIMIT);
            }
        }
        if (root.contains("types") && root["types"].is_array())
        {
            for (const auto& type : root["types"])
            {
                if (!type.is_object())
                    return unexpected(EMetaIrJsonError::INVALID_SHAPE);
                if (!addJsonNode(builder, type))
                    return unexpected(EMetaIrJsonError::IR_LIMIT);
            }
        }
        return std::move(builder).freeze();
    }

    expected<std::string, EMetaIrJsonError> templateJson(const MetaUnit& unit)
    {
        if (unit.nodes().empty())
            return unexpected(EMetaIrJsonError::INVALID_SHAPE);
        const MetaNodeId root{0};
        for (const auto& attribute : unit.attributes())
        {
            if (attribute.owner == root && unit.string(attribute.name) == "template_json")
                return std::string{unit.string(attribute.value)};
        }
        return unexpected(EMetaIrJsonError::INVALID_SHAPE);
    }
}
