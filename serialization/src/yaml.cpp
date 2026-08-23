// YAML backend implementation. rapidyaml is visible only in this translation
// unit and is compiled directly into serialization_yaml.
#include <lux/cxx/serialization/yaml.hpp>

#define RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS
#define RYML_SINGLE_HDR_DEFINE_NOW
#include <rapidyaml/rapidyaml.hpp>

#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lux::cxx::ser
{
    namespace
    {
        using NodeId = ryml::id_type;

        enum class ETagKind
        {
            NONE,
            STRING,
            BOOL,
            INTEGER,
            FLOAT,
            NULL_VALUE,
            INVALID,
        };

        enum class EScalarKind
        {
            STRING,
            BOOL,
            INTEGER,
            FLOAT,
            NULL_VALUE,
            INVALID,
        };

        std::string_view toView(ryml::csubstr value) noexcept
        {
            return std::string_view(value.str ? value.str : "", value.len);
        }

        ryml::csubstr toSubstring(std::string_view value) noexcept
        {
            return ryml::csubstr(value.data(), value.size());
        }

        ryml::csubstr copyToArena(ryml::Tree& tree, std::string_view value)
        {
            if (value.empty())
                return ryml::csubstr("", std::size_t{ 0 });
            const auto copy = tree.copy_to_arena(toSubstring(value));
            return ryml::csubstr(copy.str, copy.len);
        }

        bool isDigits(std::string_view value, unsigned base) noexcept
        {
            if (value.empty())
                return false;
            for (const char character : value)
            {
                bool valid = character >= '0' && character <= '9';
                unsigned digit = valid ? static_cast<unsigned>(character - '0') : base;
                if (base == 16 && character >= 'a' && character <= 'f')
                {
                    digit = static_cast<unsigned>(character - 'a') + 10;
                    valid = true;
                }
                else if (base == 16 && character >= 'A' && character <= 'F')
                {
                    digit = static_cast<unsigned>(character - 'A') + 10;
                    valid = true;
                }
                if (!valid || digit >= base)
                    return false;
            }
            return true;
        }

        bool isCoreNull(std::string_view value) noexcept
        {
            return value.empty() || value == "~" || value == "null" ||
                   value == "Null" || value == "NULL";
        }

        bool isCoreBool(std::string_view value) noexcept
        {
            return value == "true" || value == "True" || value == "TRUE" ||
                   value == "false" || value == "False" || value == "FALSE";
        }

        bool parseCoreBool(std::string_view value, bool& output) noexcept
        {
            if (value == "true" || value == "True" || value == "TRUE")
            {
                output = true;
                return true;
            }
            if (value == "false" || value == "False" || value == "FALSE")
            {
                output = false;
                return true;
            }
            return false;
        }

        bool isCoreInteger(std::string_view value) noexcept
        {
            if (value.size() > 2 && value[0] == '0' && value[1] == 'o')
                return isDigits(value.substr(2), 8);
            if (value.size() > 2 && value[0] == '0' && value[1] == 'x')
                return isDigits(value.substr(2), 16);

            if (!value.empty() && (value.front() == '+' || value.front() == '-'))
                value.remove_prefix(1);
            return isDigits(value, 10);
        }

        bool isCoreInfinity(std::string_view value) noexcept
        {
            if (!value.empty() && (value.front() == '+' || value.front() == '-'))
                value.remove_prefix(1);
            return value == ".inf" || value == ".Inf" || value == ".INF";
        }

        bool isCoreNan(std::string_view value) noexcept
        {
            return value == ".nan" || value == ".NaN" || value == ".NAN";
        }

        bool consumeExponent(std::string_view value, std::size_t& position) noexcept
        {
            if (position >= value.size() ||
                (value[position] != 'e' && value[position] != 'E'))
            {
                return true;
            }

            ++position;
            if (position < value.size() &&
                (value[position] == '+' || value[position] == '-'))
            {
                ++position;
            }
            const std::size_t first_digit = position;
            while (position < value.size() &&
                   value[position] >= '0' && value[position] <= '9')
            {
                ++position;
            }
            return position > first_digit;
        }

        bool isCoreFloat(std::string_view value) noexcept
        {
            if (isCoreInfinity(value) || isCoreNan(value))
                return true;

            if (!value.empty() && (value.front() == '+' || value.front() == '-'))
                value.remove_prefix(1);
            if (value.empty())
                return false;

            std::size_t position = 0;
            bool has_dot = false;
            bool has_exponent = false;
            if (value.front() == '.')
            {
                has_dot = true;
                ++position;
                const std::size_t first_digit = position;
                while (position < value.size() &&
                       value[position] >= '0' && value[position] <= '9')
                {
                    ++position;
                }
                if (position == first_digit)
                    return false;
            }
            else
            {
                const std::size_t first_digit = position;
                while (position < value.size() &&
                       value[position] >= '0' && value[position] <= '9')
                {
                    ++position;
                }
                if (position == first_digit)
                    return false;
                if (position < value.size() && value[position] == '.')
                {
                    has_dot = true;
                    ++position;
                    while (position < value.size() &&
                           value[position] >= '0' && value[position] <= '9')
                    {
                        ++position;
                    }
                }
            }

            if (position < value.size() &&
                (value[position] == 'e' || value[position] == 'E'))
            {
                has_exponent = true;
                if (!consumeExponent(value, position))
                    return false;
            }
            return position == value.size() && (has_dot || has_exponent);
        }

        ETagKind tagKind(std::string_view tag) noexcept
        {
            if (tag.empty())
                return ETagKind::NONE;
            if (tag.size() >= 3 && tag.front() == '<' && tag.back() == '>')
            {
                tag.remove_prefix(1);
                tag.remove_suffix(1);
            }
            if (tag == "!" || tag == "!!str" || tag == "tag:yaml.org,2002:str")
                return ETagKind::STRING;
            if (tag == "!!bool" || tag == "tag:yaml.org,2002:bool")
                return ETagKind::BOOL;
            if (tag == "!!int" || tag == "tag:yaml.org,2002:int")
                return ETagKind::INTEGER;
            if (tag == "!!float" || tag == "tag:yaml.org,2002:float")
                return ETagKind::FLOAT;
            if (tag == "!!null" || tag == "tag:yaml.org,2002:null")
                return ETagKind::NULL_VALUE;
            return ETagKind::INVALID;
        }

        EScalarKind inferredScalarKind(
            std::string_view value,
            bool quoted,
            ETagKind explicit_tag
        ) noexcept
        {
            switch (explicit_tag)
            {
                case ETagKind::STRING:     return EScalarKind::STRING;
                case ETagKind::BOOL:       return EScalarKind::BOOL;
                case ETagKind::INTEGER:    return EScalarKind::INTEGER;
                case ETagKind::FLOAT:      return EScalarKind::FLOAT;
                case ETagKind::NULL_VALUE: return EScalarKind::NULL_VALUE;
                case ETagKind::INVALID:    return EScalarKind::INVALID;
                case ETagKind::NONE:       break;
            }

            if (quoted)
                return EScalarKind::STRING;
            if (isCoreNull(value))
                return EScalarKind::NULL_VALUE;
            if (isCoreBool(value))
                return EScalarKind::BOOL;
            if (isCoreInteger(value))
                return EScalarKind::INTEGER;
            if (isCoreFloat(value))
                return EScalarKind::FLOAT;
            return EScalarKind::STRING;
        }

        EScalarKind scalarKind(const ryml::Tree& tree, NodeId node) noexcept
        {
            if (!tree.has_val(node) && !tree.is_container(node))
                return EScalarKind::NULL_VALUE;
            if (!tree.has_val(node) || tree.is_container(node))
                return EScalarKind::INVALID;

            const auto explicit_tag = tree.has_val_tag(node)
                ? tagKind(toView(tree.val_tag(node)))
                : ETagKind::NONE;
            return inferredScalarKind(
                toView(tree.val(node)),
                tree.is_val_quoted(node),
                explicit_tag
            );
        }

        bool isAmbiguousString(std::string_view value) noexcept
        {
            return isCoreNull(value) || isCoreBool(value) ||
                   isCoreInteger(value) || isCoreFloat(value);
        }

        template <class Integer>
        bool parseCoreInteger(std::string_view value, Integer& output) noexcept
        {
            bool negative = false;
            if (!value.empty() && (value.front() == '+' || value.front() == '-'))
            {
                negative = value.front() == '-';
                value.remove_prefix(1);
            }

            unsigned base = 10;
            if (value.size() > 2 && value[0] == '0' && value[1] == 'o')
            {
                base = 8;
                value.remove_prefix(2);
            }
            else if (value.size() > 2 && value[0] == '0' && value[1] == 'x')
            {
                base = 16;
                value.remove_prefix(2);
            }
            if (value.empty())
                return false;

            std::uint64_t magnitude = 0;
            const auto parsed = std::from_chars(
                value.data(),
                value.data() + value.size(),
                magnitude,
                static_cast<int>(base)
            );
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
                return false;

            if constexpr (std::is_signed_v<Integer>)
            {
                const auto maximum = static_cast<std::uint64_t>(
                    std::numeric_limits<Integer>::max()
                );
                if (negative)
                {
                    if (magnitude > maximum + 1)
                        return false;
                    if (magnitude == maximum + 1)
                        output = std::numeric_limits<Integer>::min();
                    else
                        output = static_cast<Integer>(-static_cast<Integer>(magnitude));
                }
                else
                {
                    if (magnitude > maximum)
                        return false;
                    output = static_cast<Integer>(magnitude);
                }
            }
            else
            {
                if (negative || magnitude > std::numeric_limits<Integer>::max())
                    return false;
                output = static_cast<Integer>(magnitude);
            }
            return true;
        }

        bool parseCoreDouble(
            std::string_view value,
            EScalarKind kind,
            double& output
        ) noexcept
        {
            if (isCoreNan(value))
            {
                output = std::numeric_limits<double>::quiet_NaN();
                return true;
            }
            if (isCoreInfinity(value))
            {
                output = value.front() == '-'
                    ? -std::numeric_limits<double>::infinity()
                    : std::numeric_limits<double>::infinity();
                return true;
            }
            if (kind == EScalarKind::INTEGER)
            {
                if (!value.empty() && value.front() == '-')
                {
                    std::int64_t integer = 0;
                    if (!parseCoreInteger(value, integer))
                        return false;
                    output = static_cast<double>(integer);
                    return true;
                }
                std::uint64_t integer = 0;
                if (!parseCoreInteger(value, integer))
                    return false;
                output = static_cast<double>(integer);
                return true;
            }

            const auto substring = toSubstring(value);
            return ryml::from_chars_float(substring, &output);
        }

        std::string childPath(
            const std::string& parent,
            std::string_view child
        )
        {
            if (parent.empty())
                return std::string(child);
            return parent + "." + std::string(child);
        }

        std::string elementPath(const std::string& parent, std::size_t index)
        {
            return parent + "[" + std::to_string(index) + "]";
        }

        bool validateExplicitScalar(
            std::string_view value,
            ETagKind tag,
            std::string message_path,
            error& validation_error
        )
        {
            bool valid = true;
            switch (tag)
            {
                case ETagKind::NONE:
                case ETagKind::STRING:
                    return true;
                case ETagKind::BOOL:
                    valid = isCoreBool(value);
                    break;
                case ETagKind::INTEGER:
                    valid = isCoreInteger(value);
                    break;
                case ETagKind::FLOAT:
                    valid = isCoreInteger(value) || isCoreFloat(value);
                    break;
                case ETagKind::NULL_VALUE:
                    valid = isCoreNull(value);
                    break;
                case ETagKind::INVALID:
                    validation_error = error{
                        error_code::parse_error,
                        "unsupported YAML tag",
                        std::move(message_path)
                    };
                    return false;
            }
            if (!valid)
            {
                validation_error = error{
                    error_code::parse_error,
                    "invalid scalar for explicit YAML core tag",
                    std::move(message_path)
                };
            }
            return valid;
        }

        bool validateTree(
            const ryml::Tree& tree,
            NodeId node,
            const std::string& path,
            int depth,
            error& validation_error
        )
        {
            if (depth > detail::ser_max_depth)
            {
                validation_error = error{
                    error_code::parse_error,
                    "maximum nesting depth exceeded",
                    path
                };
                return false;
            }

            if (tree.has_key_anchor(node) || tree.has_val_anchor(node) ||
                tree.is_key_ref(node) || tree.is_val_ref(node))
            {
                validation_error = error{
                    error_code::parse_error,
                    "YAML anchors and aliases are not supported",
                    path
                };
                return false;
            }

            if (tree.has_key_tag(node))
            {
                const auto key_tag = tagKind(toView(tree.key_tag(node)));
                if (key_tag != ETagKind::STRING)
                {
                    validation_error = error{
                        error_code::parse_error,
                        "mapping keys must use the YAML string tag",
                        path
                    };
                    return false;
                }
            }

            if (tree.has_val_tag(node))
            {
                const auto value_tag = tagKind(toView(tree.val_tag(node)));
                if (tree.is_container(node) ||
                    !validateExplicitScalar(
                        toView(tree.val(node)),
                        value_tag,
                        path,
                        validation_error
                    ))
                {
                    if (validation_error.code == error_code::ok)
                    {
                        validation_error = error{
                            error_code::parse_error,
                            "YAML collection tags are not supported",
                            path
                        };
                    }
                    return false;
                }
            }

            if (tree.is_map(node))
            {
                std::unordered_set<std::string> keys;
                keys.reserve(tree.num_children(node));
                for (NodeId child = tree.first_child(node);
                     child != ryml::NONE;
                     child = tree.next_sibling(child))
                {
                    const auto key = toView(tree.key(child));
                    const auto key_kind = inferredScalarKind(
                        key,
                        tree.is_key_quoted(child),
                        tree.has_key_tag(child)
                            ? tagKind(toView(tree.key_tag(child)))
                            : ETagKind::NONE
                    );
                    const std::string key_path = childPath(path, key);
                    if (key_kind != EScalarKind::STRING)
                    {
                        validation_error = error{
                            error_code::parse_error,
                            "YAML mapping keys must resolve to strings",
                            key_path
                        };
                        return false;
                    }
                    if (!tree.is_key_quoted(child) && key == "<<")
                    {
                        validation_error = error{
                            error_code::parse_error,
                            "YAML merge keys are not supported",
                            key_path
                        };
                        return false;
                    }
                    if (!keys.emplace(key).second)
                    {
                        validation_error = error{
                            error_code::duplicate_member,
                            "duplicate YAML mapping key",
                            key_path
                        };
                        return false;
                    }
                    if (!validateTree(
                        tree,
                        child,
                        key_path,
                        depth + 1,
                        validation_error
                    ))
                    {
                        return false;
                    }
                }
            }
            else if (tree.is_seq(node))
            {
                std::size_t index = 0;
                for (NodeId child = tree.first_child(node);
                     child != ryml::NONE;
                     child = tree.next_sibling(child), ++index)
                {
                    if (!validateTree(
                        tree,
                        child,
                        elementPath(path, index),
                        depth + 1,
                        validation_error
                    ))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        NodeId nthChild(
            const ryml::Tree& tree,
            NodeId parent,
            std::size_t index
        ) noexcept
        {
            NodeId child = tree.first_child(parent);
            for (std::size_t current = 0;
                 child != ryml::NONE && current < index;
                 ++current)
            {
                child = tree.next_sibling(child);
            }
            return child;
        }
    } // namespace

    struct YamlOutputArchive::Impl
    {
        ryml::Tree         tree;
        std::vector<NodeId> stack;
        std::string         pending_key;
        bool                has_pending_key = false;
        bool                wrote_root = false;
        bool                pretty;

        explicit Impl(bool use_pretty) : pretty(use_pretty)
        {
            stack.reserve(16);
        }

        NodeId addNode()
        {
            if (stack.empty())
            {
                if (wrote_root)
                    throw std::logic_error("YAML archive already has a root value");
                wrote_root = true;
                return tree.root_id();
            }

            const NodeId parent = stack.back();
            const NodeId node = tree.append_child(parent);
            if (tree.is_map(parent))
            {
                if (!has_pending_key)
                    throw std::logic_error("YAML object value has no key");
                const auto key = copyToArena(tree, pending_key);
                if (isAmbiguousString(pending_key))
                    tree.set_key(node, key, ryml::KEY_SQUO);
                else
                    tree.set_key(node, key);
                has_pending_key = false;
                pending_key.clear();
            }
            return node;
        }

        void setScalar(std::string_view value, ryml::NodeType style)
        {
            const NodeId node = addNode();
            tree.set_val(node, copyToArena(tree, value), style);
        }
    };

    YamlOutputArchive::YamlOutputArchive(bool pretty)
        : p_(std::make_unique<Impl>(pretty))
    {
    }

    YamlOutputArchive::~YamlOutputArchive() = default;
    YamlOutputArchive::YamlOutputArchive(YamlOutputArchive&&) noexcept = default;
    YamlOutputArchive& YamlOutputArchive::operator=(YamlOutputArchive&&) noexcept = default;

    void YamlOutputArchive::begin_object(std::size_t)
    {
        const NodeId node = p_->addNode();
        if (p_->pretty)
            p_->tree.set_map(node);
        else
            p_->tree.set_map(node, ryml::FLOW_SL);
        p_->stack.push_back(node);
    }

    void YamlOutputArchive::key(std::string_view name, bool)
    {
        p_->pending_key.assign(name);
        p_->has_pending_key = true;
    }

    void YamlOutputArchive::end_object()
    {
        p_->stack.pop_back();
    }

    void YamlOutputArchive::begin_array(std::size_t)
    {
        const NodeId node = p_->addNode();
        if (p_->pretty)
            p_->tree.set_seq(node);
        else
            p_->tree.set_seq(node, ryml::FLOW_SL);
        p_->stack.push_back(node);
    }

    void YamlOutputArchive::end_array()
    {
        p_->stack.pop_back();
    }

    void YamlOutputArchive::null_value()
    {
        const NodeId node = p_->addNode();
        p_->tree.set_val(node, ryml::csubstr("null"), ryml::VAL_PLAIN);
    }

    void YamlOutputArchive::value(bool value)
    {
        p_->setScalar(value ? "true" : "false", ryml::VAL_PLAIN);
    }

    void YamlOutputArchive::value(std::int64_t value)
    {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        p_->setScalar(
            std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer)),
            ryml::VAL_PLAIN
        );
    }

    void YamlOutputArchive::value(std::uint64_t value)
    {
        char buffer[32];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        p_->setScalar(
            std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer)),
            ryml::VAL_PLAIN
        );
    }

    void YamlOutputArchive::value(double value)
    {
        if (std::isnan(value))
        {
            p_->setScalar(".nan", ryml::VAL_PLAIN);
            return;
        }
        if (std::isinf(value))
        {
            p_->setScalar(value < 0 ? "-.inf" : ".inf", ryml::VAL_PLAIN);
            return;
        }

        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        p_->setScalar(
            std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer)),
            ryml::VAL_PLAIN
        );
    }

    void YamlOutputArchive::value(std::string_view value)
    {
        p_->setScalar(
            value,
            isAmbiguousString(value) ? ryml::VAL_SQUO : ryml::NOTYPE
        );
    }

    std::string YamlOutputArchive::str() const
    {
        ryml::EmitOptions options;
        options.max_depth(static_cast<NodeId>(detail::ser_max_depth));
        return ryml::emitrs_yaml<std::string>(p_->tree, options);
    }

    struct YamlCursorAccess
    {
        static const ryml::Tree* tree(const YamlInputArchive& cursor) noexcept
        {
            return static_cast<const ryml::Tree*>(cursor.tree_);
        }

        static NodeId node(const YamlInputArchive& cursor) noexcept
        {
            return static_cast<NodeId>(cursor.node_);
        }

        static YamlInputArchive make(
            const ryml::Tree* tree,
            NodeId node
        ) noexcept
        {
            if (!tree || node == ryml::NONE)
                return {};
            return YamlInputArchive(tree, static_cast<std::size_t>(node));
        }
    };

    bool YamlInputArchive::is_null() const noexcept
    {
        if (!tree_)
            return true;
        return scalarKind(*YamlCursorAccess::tree(*this), YamlCursorAccess::node(*this)) ==
               EScalarKind::NULL_VALUE;
    }

    bool YamlInputArchive::is_object() const noexcept
    {
        return tree_ && YamlCursorAccess::tree(*this)->is_map(
            YamlCursorAccess::node(*this)
        );
    }

    bool YamlInputArchive::is_array() const noexcept
    {
        return tree_ && YamlCursorAccess::tree(*this)->is_seq(
            YamlCursorAccess::node(*this)
        );
    }

    YamlInputArchive YamlInputArchive::member(
        std::string_view name,
        bool
    ) const
    {
        if (!is_object())
            return {};
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        for (NodeId child = tree->first_child(node);
             child != ryml::NONE;
             child = tree->next_sibling(child))
        {
            if (toView(tree->key(child)) == name)
                return YamlCursorAccess::make(tree, child);
        }
        return {};
    }

    std::size_t YamlInputArchive::size() const noexcept
    {
        if (!is_array())
            return 0;
        return YamlCursorAccess::tree(*this)->num_children(
            YamlCursorAccess::node(*this)
        );
    }

    YamlInputArchive YamlInputArchive::element(std::size_t index) const
    {
        if (!is_array())
            return {};
        const auto* tree = YamlCursorAccess::tree(*this);
        return YamlCursorAccess::make(
            tree,
            nthChild(*tree, YamlCursorAccess::node(*this), index)
        );
    }

    std::size_t YamlInputArchive::member_count() const noexcept
    {
        if (!is_object())
            return 0;
        return YamlCursorAccess::tree(*this)->num_children(
            YamlCursorAccess::node(*this)
        );
    }

    std::string_view YamlInputArchive::member_key(std::size_t index) const
    {
        if (!is_object())
            return {};
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId child = nthChild(
            *tree,
            YamlCursorAccess::node(*this),
            index
        );
        return child == ryml::NONE ? std::string_view{} : toView(tree->key(child));
    }

    YamlInputArchive YamlInputArchive::member_value(std::size_t index) const
    {
        if (!is_object())
            return {};
        const auto* tree = YamlCursorAccess::tree(*this);
        return YamlCursorAccess::make(
            tree,
            nthChild(*tree, YamlCursorAccess::node(*this), index)
        );
    }

    void YamlInputArchive::for_each_member(
        function_ref<void(std::string_view, const YamlInputArchive&)> function
    ) const
    {
        if (!is_object())
            return;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        for (NodeId child = tree->first_child(node);
             child != ryml::NONE;
             child = tree->next_sibling(child))
        {
            const auto cursor = YamlCursorAccess::make(tree, child);
            function(toView(tree->key(child)), cursor);
        }
    }

    void YamlInputArchive::for_each_element(
        function_ref<void(std::size_t, const YamlInputArchive&)> function
    ) const
    {
        if (!is_array())
            return;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        std::size_t index = 0;
        for (NodeId child = tree->first_child(node);
             child != ryml::NONE;
             child = tree->next_sibling(child), ++index)
        {
            const auto cursor = YamlCursorAccess::make(tree, child);
            function(index, cursor);
        }
    }

    bool YamlInputArchive::read(bool& output) const
    {
        if (!tree_)
            return false;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        if (scalarKind(*tree, node) != EScalarKind::BOOL)
            return false;
        return parseCoreBool(toView(tree->val(node)), output);
    }

    bool YamlInputArchive::read(std::int64_t& output) const
    {
        if (!tree_)
            return false;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        if (scalarKind(*tree, node) != EScalarKind::INTEGER)
            return false;
        return parseCoreInteger(toView(tree->val(node)), output);
    }

    bool YamlInputArchive::read(std::uint64_t& output) const
    {
        if (!tree_)
            return false;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        if (scalarKind(*tree, node) != EScalarKind::INTEGER)
            return false;
        return parseCoreInteger(toView(tree->val(node)), output);
    }

    bool YamlInputArchive::read(double& output) const
    {
        if (!tree_)
            return false;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        const auto kind = scalarKind(*tree, node);
        if (kind != EScalarKind::INTEGER && kind != EScalarKind::FLOAT)
            return false;
        return parseCoreDouble(toView(tree->val(node)), kind, output);
    }

    bool YamlInputArchive::read(std::string& output) const
    {
        if (!tree_)
            return false;
        const auto* tree = YamlCursorAccess::tree(*this);
        const NodeId node = YamlCursorAccess::node(*this);
        if (scalarKind(*tree, node) != EScalarKind::STRING)
            return false;
        output.assign(toView(tree->val(node)));
        return true;
    }

    struct YamlDocument::Impl
    {
        ryml::Tree tree;
        NodeId     root = ryml::NONE;
    };

    YamlDocument::YamlDocument() : p_(std::make_unique<Impl>()) {}
    YamlDocument::~YamlDocument() = default;
    YamlDocument::YamlDocument(YamlDocument&&) noexcept = default;
    YamlDocument& YamlDocument::operator=(YamlDocument&&) noexcept = default;

    result<YamlDocument> YamlDocument::parse(std::string_view text)
    {
        YamlDocument document;
        try
        {
            document.p_->tree = ryml::parse_in_arena(toSubstring(text));
        }
        catch (const std::exception& exception)
        {
            return make_error(error_code::parse_error, exception.what());
        }
        catch (...)
        {
            return make_error(error_code::parse_error, "YAML parse error");
        }

        NodeId root = document.p_->tree.root_id();
        if (document.p_->tree.is_stream(root))
        {
            const auto document_count = document.p_->tree.num_children(root);
            if (document_count != 1)
            {
                return make_error(
                    error_code::parse_error,
                    "YAML input must contain exactly one document"
                );
            }
            root = document.p_->tree.first_child(root);
        }
        document.p_->root = root;

        error validation_error;
        if (!validateTree(
            document.p_->tree,
            root,
            {},
            0,
            validation_error
        ))
        {
            return lux::cxx::unexpected<error>(std::move(validation_error));
        }
        return document;
    }

    YamlInputArchive YamlDocument::root() const
    {
        return YamlCursorAccess::make(&p_->tree, p_->root);
    }
} // namespace lux::cxx::ser
