#include <lux/cxx/arguments/StructuredValue.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <lux/cxx/arguments/Help.hpp>
#include <memory_resource>
#include <cstdio>
#include <cstdlib>

namespace a = lux::cxx::args;
namespace
{
    int checks = 0;
}
#define CHECK(...)                                                                                         \
    do                                                                                                     \
    {                                                                                                      \
        ++checks;                                                                                          \
        if (!(__VA_ARGS__))                                                                                \
        {                                                                                                  \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__);                    \
            std::abort();                                                                                  \
        }                                                                                                  \
    } while (false)

template<class T, std::size_t N>
struct SmallBuffer
{
    std::array<T, N> data{};
    std::size_t used = 0;
    auto begin() const
    {
        return data.begin();
    }
    auto end() const
    {
        return data.begin() + used;
    }
};
struct Color
{
    int value = 0;
};
struct Wrapped
{
    int value = 0;
};
struct Override
{
    int value = 0;
};
namespace lux::cxx::args
{
    template<class T, std::size_t N>
    struct SequenceTraits<::SmallBuffer<T, N>>
    {
        using value_type = T;
        static constexpr std::size_t extent = dynamic_extent;
        static void clear(::SmallBuffer<T, N>& out) noexcept
        {
            out.used = 0;
        }
        static EErrorCode append(::SmallBuffer<T, N>& out, T&& value) noexcept
        {
            if (out.used == N)
                return EErrorCode::CAPACITY_EXCEEDED;
            out.data[out.used++] = std::move(value);
            return EErrorCode::OK;
        }
    };
    template<>
    struct ValueParser<::Color>
    {
        static EErrorCode parse(std::string_view s, ::Color& out)
        {
            if (s == "red")
            {
                out.value = 1;
                return EErrorCode::OK;
            }
            if (s == "blue")
            {
                out.value = 2;
                return EErrorCode::OK;
            }
            return EErrorCode::INVALID_VALUE;
        }
    };
    // This deliberately has traits AND an explicit value_parser. The explicit
    // specialization wins; the array fallback must never instantiate its body.
    template<>
    struct SequenceTraits<::Override>
    {
        using value_type = int;
        static constexpr std::size_t extent = 1;
        static int& at(::Override& out, std::size_t)
        {
            return out.value;
        }
    };
    template<>
    struct ValueParser<::Override>
    {
        static EErrorCode parse(std::string_view s, ::Override& out)
        {
            if (s != "special")
                return EErrorCode::INVALID_VALUE;
            out.value = 77;
            return EErrorCode::OK;
        }
    };
} // namespace lux::cxx::args

template<class T>
void successful(std::string_view text, const T& expected)
{
    T out{};
    auto e = a::parseStructured(text, out);
    CHECK(!e);
    CHECK(out == expected);
}

int main()
{
    successful<std::vector<int>>(" [1, -2, 3] ", {1, -2, 3});
    successful<std::array<int, 3>>("[1,2,3]", {1, 2, 3});
    successful<std::array<int, 0>>("[]", {});
    successful<std::tuple<int, double, bool>>("[1, 2.5, true]", {1, 2.5, true});
    successful<std::tuple<>>("[]", {});
    successful<std::pair<std::string, int>>(R"(["name", 3])", {"name", 3});
    successful<std::deque<int>>("[1,2,3]", {1, 2, 3});
    successful<std::list<int>>("[1,2,3]", {1, 2, 3});
    successful<std::set<int>>("[3,1,1]", {1, 3});
    successful<std::multiset<int>>("[3,1,1]", {1, 1, 3});
    successful<std::unordered_set<int>>("[3,1,1]", {1, 3});
    successful<std::unordered_multiset<int>>("[3,1,1]", {1, 1, 3});
    successful<std::vector<bool>>("[true,false,1,0]", {true, false, true, false});
    successful<std::map<std::string, int>>(R"({a:1, "b":2})", {{"a", 1}, {"b", 2}});
    successful<std::unordered_map<int, std::string>>(R"({1:"a",2:hello})", {{1, "a"}, {2, "hello"}});
    successful<std::vector<std::vector<int>>>("[[1,2],[],[3]]", {{1, 2}, {}, {3}});
    successful<std::map<std::string, std::vector<std::array<int, 2>>>>(
        R"({items:[[1,2],[3,4]], empty:[]})", {{"items", {{1, 2}, {3, 4}}}, {"empty", {}}}
    );
    successful<std::vector<std::optional<int>>>("[1,null,3]", {1, std::nullopt, 3});
    successful<std::optional<std::vector<int>>>("[1,2]", std::vector<int>{1, 2});
    successful<std::vector<std::string>>(
        R"(["a,b", "x:y", "bracket]}", 'two words', "a\"b", "line\nnext"])",
        {"a,b", "x:y", "bracket]}", "two words", "a\"b", "line\nnext"}
    );
    successful<std::string>(R"("\u4f60\u597d\ud83d\ude00")", "\xe4\xbd\xa0\xe5\xa5\xbd\xf0\x9f\x98\x80");
    successful<std::string>(R"("a\u0000b")", std::string("a\0b", 3));
    successful<std::vector<std::string>>("[]", {});

    std::vector<int> ints{9, 9};
    for (auto s : {"", "[", "[1,]", "[,1]", "[1 2]", "[1,,2]", "[1]tail", "{a:1}", "[1}"})
        CHECK(a::parseStructured(s, ints));
    CHECK(a::parseStructured("[1,2oops]", ints).code == a::EErrorCode::INVALID_INTEGER);
    CHECK(a::parseStructured("[1,2oops]", ints).offset == 3);
    std::array<int, 3> fixed{};
    CHECK(a::parseStructured("[1,2]", fixed).code == a::EErrorCode::SIZE_MISMATCH);
    CHECK(a::parseStructured("[1,2,3,4]", fixed).code == a::EErrorCode::SIZE_MISMATCH);
    std::string str;
    for (auto s :
         {R"("\uD800")", R"("\uDC00")", R"("\uD800\u1234")", R"("\uXYZ1")", R"("\q")", "\"raw\ncontrol\""})
        CHECK(a::parseStructured(s, str));
    CHECK(a::parseStructured("[1,2]", ints, {.max_depth = 0}).code == a::EErrorCode::DEPTH_LIMIT);
    CHECK(a::parseStructured("[1,2]", ints, {.max_depth = 65}).code == a::EErrorCode::INVALID_INPUT);
    CHECK(a::parseStructured("[1,2]", ints, {.max_elements = 2}).code == a::EErrorCode::ELEMENT_LIMIT);
    std::vector<std::vector<int>> nested;
    CHECK(a::parseStructured("[[1]]", nested, {.max_depth = 1}).code == a::EErrorCode::DEPTH_LIMIT);

    std::map<std::string, int> table;
    CHECK(a::parseStructured("{a:1,a:2}", table).code == a::EErrorCode::DUPLICATE_KEY);
    CHECK(!a::parseStructured("{a:1,a:2}", table, {.keys = a::EDuplicateKeys::FIRST}));
    CHECK(table.at("a") == 1);
    CHECK(!a::parseStructured("{a:1,a:2}", table, {.keys = a::EDuplicateKeys::LAST}));
    CHECK(table.at("a") == 2);
    CHECK(
        a::parseStructured("{a:1,a:bad}", table, {.keys = a::EDuplicateKeys::FIRST}).code ==
        a::EErrorCode::INVALID_INTEGER
    );
    for (auto s : {"{a}", "{a:}", "{a:1,}", "{a:1 b:2}", "{a:1]"})
        CHECK(a::parseStructured(s, table));

    const a::Option<std::array<double, 3>, "position", "p"> position;
    const a::Option<std::vector<std::vector<int>>, "matrix", "m"> matrix;
    const a::Option<std::map<std::string, std::vector<int>>, "table", "t"> settings;
    const a::Option<std::vector<std::string>, "names", "n"> names;
    const a::Option<std::string, "label", "l"> label;
    const a::Option<int, "width", "w"> width;
    auto r = a::parse(
        R"(position=[1, 2, 3] matrix=[[1,2], [3,4]] table={left: [1,2], "right": [3]} names=["a,b", "x:y", "c d"] label="CLI title" -w10)",
        position,
        matrix,
        settings,
        names,
        label,
        width
    );
    if (!r)
        std::fprintf(
            stderr,
            "parse: %s\n",
            a::describe(r.error(), position, matrix, settings, names, label, width).c_str()
        );
    CHECK(r);
    CHECK(r.get<"position">()->at(2) == 3);
    CHECK(r.get<"matrix">()->at(1).at(1) == 4);
    CHECK(r.get<"table">()->at("right").at(0) == 3);
    CHECK(*r.get<"names">() == std::vector<std::string>({"a,b", "x:y", "c d"}));
    CHECK(*r.get<"label">() == "CLI title" && *r.get<"width">() == 10);
    struct Object1
    {
        std::array<double, 3> position;
    } o1;
    struct Object2
    {
        std::vector<std::vector<int>> matrix;
    } o2;
    CHECK(!a::parseInto(
        "-p[4, 5, 6] -m=[[9,8], [7,6]]", a::bind(position, o1.position), a::bind(matrix, o2.matrix)
    ));
    CHECK(o1.position[2] == 6 && o2.matrix[0][0] == 9);
    const char* argv[] = {"app", "--position=[1, 2, 3]", "--names=[\"a,b\",\"x y\"]"};
    auto ar = a::parse(3, argv, position, names);
    CHECK(ar && (*ar.get<"names">())[0] == "a,b");
    const auto whole = a::Option<std::vector<int>, "v">{};
    auto complete = a::parse("v=[1,2] v=[3]", whole);
    CHECK(complete && *complete.get<"v">() == std::vector<int>{3});
    CHECK(a::parse("v=1", whole).error().code == a::EErrorCode::INVALID_STRUCTURE);
    auto multi = whole.multi();
    auto many = a::parse("v=1,2 --v 3 4", multi.delimiter(','));
    CHECK(many && *many.get<"v">() == std::vector<int>({1, 2, 3, 4}));
    CHECK(a::parse("v=[1,2]", multi).error().code == a::EErrorCode::INVALID_INTEGER);
    auto rows = matrix.multi().delimiter(';');
    auto row_result = a::parse("matrix=[1,2];[3,4]", rows);
    CHECK(row_result && *row_result.get<"matrix">() == std::vector<std::vector<int>>({{1, 2}, {3, 4}}));
    auto row_error = a::parse("matrix=[1];[2oops]", rows).error();
    CHECK(row_error.code == a::EErrorCode::INVALID_INTEGER && row_error.value_offset == 5);
    auto size_checked = whole.check([](const auto& v) { return v.size() == 2; });
    CHECK(a::parse("v=[1,2]", size_checked));
    CHECK(a::parse("v=[1]", size_checked).error().code == a::EErrorCode::VALIDATION_FAILED);
    auto checked = multi.check(a::inRange(0, 4));
    CHECK(a::parse("v=5", checked).error().code == a::EErrorCode::VALIDATION_FAILED);
    CHECK(
        a::parse("", width.def(-1).check(a::inRange(0, 100))).error().code == a::EErrorCode::INVALID_SCHEMA
    );
    auto map_option = a::Option<std::map<std::string, int>, "map">{}.convert(
        a::structured({.keys = a::EDuplicateKeys::LAST})
    );
    auto map_result = a::parse("map={a:1,a:2}", map_option);
    CHECK(map_result && map_result.get<"map">()->at("a") == 2);
    CHECK(a::parse("--position=[1,2}", position).error().code == a::EErrorCode::INVALID_STRUCTURE);
    CHECK(a::parse("names=[\"a,b\",\"x\"]", names));
    CHECK(a::parse("names='[\"a,b\", \"x y\"]'", names));
    CHECK(a::parse("--position=[1,2,3] --label='[literal] not an array'", position, label));

    const auto custom = a::Option<SmallBuffer<int, 4>, "small">{};
    auto small = a::parse("small=[1,2,3]", custom);
    CHECK(small && small.get<"small">()->used == 3 && small.get<"small">()->data[2] == 3);
    CHECK(a::parse("small=[1,2,3,4,5]", custom).error().code == a::EErrorCode::CAPACITY_EXCEEDED);
    auto small_multi = a::parse("--small 1 2 3", custom.multi());
    CHECK(small_multi && small_multi.get<"small">()->used == 3);
    auto colors = a::parse("colors=[red,blue]", a::Option<std::vector<Color>, "colors">{});
    CHECK(colors && colors.get<"colors">()->at(1).value == 2);
    auto override = a::parse("o=special", a::Option<Override, "o">{});
    CHECK(override && override.get<"o">()->value == 77);
    auto nested_override = a::parse("o=[special,special]", a::Option<std::vector<Override>, "o">{});
    CHECK(nested_override && nested_override.get<"o">()->at(1).value == 77);
    const auto converted = a::Option<Wrapped, "wrapped">{}.convert([](std::string_view s, Wrapped& out) {
        return a::ValueParser<int>::parse(s, out.value);
    });
    auto wrapped = a::parse("wrapped=8", converted);
    CHECK(wrapped && wrapped.get<"wrapped">()->value == 8);

    // Own all nested strings even after the input and option descriptors disappear.
    auto independent = [&] {
        std::string input = "dyn={key:[\"long string longer than small string storage\"]}";
        a::Option<std::map<std::string, std::vector<std::string>>> dyn("dyn");
        return a::parse(input, dyn);
    }();
    std::map<std::string, std::vector<std::string>> extracted;
    CHECK(independent.get<decltype(extracted)>("dyn", &extracted) == a::EReadStatus::OK);
    CHECK(extracted.at("key")[0] == "long string longer than small string storage");
    // Preserve caller-owned resources, including nested PMR mapped values.
    std::byte memory[8192];
    std::pmr::monotonic_buffer_resource resource(memory, sizeof(memory), std::pmr::null_memory_resource());
    std::pmr::vector<std::pmr::string> strings(&resource);
    CHECK(!a::parseStructured(R"(["a moderately long string that exceeds SSO"])", strings));
    CHECK(
        strings.get_allocator().resource() == &resource &&
        strings[0].get_allocator().resource() == &resource
    );
    std::pmr::map<std::pmr::string, std::pmr::vector<int>> pmr_map(&resource);
    CHECK(!a::parseStructured(R"({"long key that requires allocated storage":[1,2,3]})", pmr_map));
    CHECK(pmr_map.begin()->first.get_allocator().resource() == &resource);
    CHECK(pmr_map.begin()->second.get_allocator().resource() == &resource);
    // A descriptor without .def() is independent of the output value's size.
    static_assert(sizeof(a::Option<std::array<int, 1000000>, "large">) == sizeof(a::Option<int, "large">));
    constexpr auto constant_option = a::Option<int, "constant">{}.required().check(a::inRange(1, 10));
    static_assert(constant_option.isRequired());
    auto constant_result = a::parse("constant=4", constant_option);
    CHECK(constant_result && *constant_result.get<"constant">() == 4);
    const auto owning =
        a::Option<std::unique_ptr<int>, "owner">{}
            .convert([factor = std::make_unique<int>(2)](std::string_view text, std::unique_ptr<int>& out) {
                int value{};
                if (auto e = a::ValueParser<int>::parse(text, value); e != a::EErrorCode::OK)
                    return e;
                out = std::make_unique<int>(value * *factor);
                return a::EErrorCode::OK;
            })
            .required();
    auto owner_result = a::parse("owner=21", owning);
    CHECK(owner_result && **owner_result.get<"owner">() == 42);
    std::unique_ptr<int> moved;
    CHECK(owner_result.take<"owner">(&moved) && *moved == 42);
    CHECK(!owner_result.get<"owner">());
    const auto owners = a::Option<std::vector<std::unique_ptr<int>>, "owners">{}.multi().convert(
        [](std::string_view text, std::unique_ptr<int>& out) {
            int value{};
            if (auto e = a::ValueParser<int>::parse(text, value); e != a::EErrorCode::OK)
                return e;
            out = std::make_unique<int>(value);
            return a::EErrorCode::OK;
        }
    );
    auto owners_result = a::parse("--owners 1 2 3", owners);
    CHECK(owners_result && *owners_result.get<"owners">()->at(2) == 3);
    std::vector<std::unique_ptr<int>> owner_targets;
    CHECK(!a::parseInto("--owners 4 5", a::bind(owners, owner_targets)));
    CHECK(owner_targets.size() == 2 && *owner_targets[1] == 5);
    enum class Number : int
    {
        one = 1,
        two = 2
    };
    std::array<Number, 2> enum_values{};
    CHECK(!a::parseStructured("[1,2]", enum_values));
    CHECK(enum_values[0] == Number::one && enum_values[1] == Number::two);
    std::printf("structured: %d checks PASS\n", checks);
}
