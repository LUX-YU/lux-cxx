#pragma once
/*
 * YAML backend — public header.
 *
 * rapidyaml is an implementation detail compiled into serialization_yaml;
 * this header exposes only opaque archive/document handles.
 *
 *   to_yaml(v)         -> std::string
 *   from_yaml<T>(text) -> result<T>
 */
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <lux/cxx/serialization/archive.hpp>
#include <lux/cxx/serialization/core.hpp>
#include <lux/cxx/serialization/error.hpp>

namespace lux::cxx::ser
{
    class YamlOutputArchive
    {
    public:
        explicit YamlOutputArchive(bool pretty = true);
        ~YamlOutputArchive();
        YamlOutputArchive(YamlOutputArchive&&) noexcept;
        YamlOutputArchive& operator=(YamlOutputArchive&&) noexcept;
        YamlOutputArchive(const YamlOutputArchive&) = delete;
        YamlOutputArchive& operator=(const YamlOutputArchive&) = delete;

        void begin_object(std::size_t = 0);
        void key(std::string_view, bool as_attribute = false);
        void end_object();
        void begin_array(std::size_t = 0);
        void end_array();
        void null_value();
        void value(bool);
        void value(std::int64_t);
        void value(std::uint64_t);
        void value(double);
        void value(std::string_view);

        std::string str() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    class YamlInputArchive
    {
    public:
        YamlInputArchive() noexcept = default;

        explicit operator bool() const noexcept { return tree_ != nullptr; }

        bool is_null()   const noexcept;
        bool is_object() const noexcept;
        bool is_array()  const noexcept;

        YamlInputArchive member(std::string_view, bool as_attribute = false) const;
        std::size_t      size() const noexcept;
        YamlInputArchive element(std::size_t) const;

        std::size_t      member_count() const noexcept;
        std::string_view member_key(std::size_t) const;
        YamlInputArchive member_value(std::size_t) const;

        void for_each_member(
            function_ref<void(std::string_view, const YamlInputArchive&)>
        ) const;
        void for_each_element(
            function_ref<void(std::size_t, const YamlInputArchive&)>
        ) const;

        bool read(bool&)          const;
        bool read(std::int64_t&)  const;
        bool read(std::uint64_t&) const;
        bool read(double&)        const;
        bool read(std::string&)   const;

    private:
        friend struct YamlCursorAccess;
        YamlInputArchive(const void* tree, std::size_t node) noexcept
            : tree_(tree), node_(node)
        {
        }

        const void* tree_ = nullptr;
        std::size_t node_ = 0;
    };

    class YamlDocument
    {
    public:
        YamlDocument();
        ~YamlDocument();
        YamlDocument(YamlDocument&&) noexcept;
        YamlDocument& operator=(YamlDocument&&) noexcept;
        YamlDocument(const YamlDocument&) = delete;
        YamlDocument& operator=(const YamlDocument&) = delete;

        static result<YamlDocument> parse(std::string_view text);
        YamlInputArchive root() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> p_;
    };

    static_assert(OutputArchive<YamlOutputArchive>);
    static_assert(InputArchive<YamlInputArchive>);

    template <class T>
    [[nodiscard]] std::string to_yaml(const T& value, bool pretty = true)
    {
        YamlOutputArchive archive(pretty);
        save(archive, value);
        return archive.str();
    }

    template <class T>
    [[nodiscard]] result<T> from_yaml(std::string_view text)
    {
        auto document = YamlDocument::parse(text);
        if (!document)
            return lux::cxx::unexpected<error>(document.error());

        T output{};
        error load_error;
        if (!load(document->root(), output, &load_error))
        {
            if (load_error.code == error_code::ok)
            {
                load_error = error{
                    error_code::load_failed,
                    "YAML deserialization failed",
                    {}
                };
            }
            return lux::cxx::unexpected<error>(std::move(load_error));
        }
        return output;
    }
} // namespace lux::cxx::ser
