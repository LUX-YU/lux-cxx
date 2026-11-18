#include <lux/cxx/arguments/ContainerTraits.hpp>
#include <lux/cxx/arguments/TypedOptions.hpp>
#include <lux/cxx/arguments/DirectBinding.hpp>
#include <lux/cxx/arguments/Help.hpp>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace a = lux::cxx::args;
#define REQUIRE(expr)                                                                                      \
    do                                                                                                     \
    {                                                                                                      \
        if (!(expr))                                                                                       \
        {                                                                                                  \
            std::fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #expr);                          \
            std::abort();                                                                                  \
        }                                                                                                  \
    } while (false)

struct Renderer
{
    int width = 1280;
    bool vsync = true;
    std::vector<int> devices{42};
};
struct Camera
{
    double gain = 1.0;
};
struct Application
{
    std::string name = "default";
};

static_assert(std::same_as<decltype(a::Option<int, "width", "w">{}.name()), std::string_view>);

struct Multiplier
{
    int scale;
    a::EErrorCode operator()(std::string_view s, int& out) const noexcept
    {
        int v{};
        if (const auto error = a::ValueParser<int>::parse(s, v); error != a::EErrorCode::OK)
            return error;
        out = v * scale;
        return a::EErrorCode::OK;
    }
};

int main()
{
    constexpr a::Option<int, "width", "w"> width{};
    constexpr a::Option<double, "gain", "g"> gain{};
    constexpr a::Option<bool, "vsync", "v"> vsync{};
    const auto name = a::Option<std::string, "name", "n">{};
    const auto devices = a::Option<std::vector<int>, "devices", "d">{}.multi();

    {
        auto r = a::parse(
            R"(--width=1920 -g 1.25 --no-vsync name="Camera One" --devices=1,2 --devices 3)",
            width,
            gain,
            vsync,
            name,
            devices.delimiter(',')
        );
        REQUIRE(r);
        static_assert(std::same_as<decltype(r.get<"width">()), std::optional<int>&>);
        REQUIRE(r.get<"width">().value() == 1920);
        REQUIRE(r.get<"gain">().value() == 1.25);
        REQUIRE(r.get<"vsync">().value() == false);
        REQUIRE(r.get<"name">().value() == "Camera One");
        REQUIRE((r.get<"devices">().value() == std::vector<int>{1, 2, 3}));
        REQUIRE(r.supplied<"width">());
        Renderer renderer;
        Camera camera;
        Application app;
        REQUIRE(r.get<"width">(&renderer.width));
        REQUIRE(r.get<"gain">(&camera.gain));
        REQUIRE(r.get(name, &app.name));
        REQUIRE(renderer.width == 1920 && camera.gain == 1.25 && app.name == "Camera One");
        REQUIRE(r.get<int>("width", &renderer.width) == a::EReadStatus::OK);
        REQUIRE(r.get<double>("width", &camera.gain) == a::EReadStatus::WRONG_TYPE);
        REQUIRE(r.get<int>("missing", &renderer.width) == a::EReadStatus::UNKNOWN_NAME);
        REQUIRE(r.get<"width">(&renderer.width) && renderer.width == 1920);
        REQUIRE(!r.get<"width">(static_cast<int*>(nullptr)));
    }
    {
        Renderer renderer;
        Camera camera;
        Application app;
        auto error = a::parseInto(
            R"(width=1600 gain=2.5 --no-vsync name="Scene One" devices=4,5)",
            a::bind(width, renderer.width),
            a::bind(gain, camera.gain),
            a::bind(vsync, renderer.vsync),
            a::bind(name, app.name),
            a::bind(devices.delimiter(','), renderer.devices)
        );
        REQUIRE(!error);
        REQUIRE(renderer.width == 1600 && camera.gain == 2.5 && !renderer.vsync);
        REQUIRE(app.name == "Scene One" && (renderer.devices == std::vector<int>{4, 5}));
    }
    {
        const a::Option<int> runtime("port", "p");
        auto r = a::parse("-p 8080", runtime);
        REQUIRE(r);
        int x{};
        REQUIRE(r.get<int>("port", &x) == a::EReadStatus::OK && x == 8080);
        REQUIRE(r.get<int>("other", &x) == a::EReadStatus::UNKNOWN_NAME);
        REQUIRE(r.get<int>("port", nullptr) == a::EReadStatus::WRONG_TYPE);
    }
    {
        auto r = a::parse("", width.def(150).required(), name);
        REQUIRE(!r);
        REQUIRE(r.error().code == a::EErrorCode::MISSING_REQUIRED);
        auto r2 = a::parse("", width.def(150), name);
        REQUIRE(r2 && r2.get<"width">().value() == 150 && !r2.supplied<"width">());
    }
    {
        REQUIRE(a::parse("--width=3garbage", width).error().code == a::EErrorCode::INVALID_INTEGER);
        REQUIRE(a::parse("width=", width).error().code == a::EErrorCode::INVALID_INTEGER);
        REQUIRE(a::parse("width=2147483648", width).error().code == a::EErrorCode::INVALID_INTEGER);
        REQUIRE(a::parse("--width", width).error().code == a::EErrorCode::MISSING_VALUE);
        REQUIRE(a::parse("--unknown=8", width).error().code == a::EErrorCode::UNKNOWN_OPTION);
        REQUIRE(
            a::parse("--width=3 --width=4", width.onRepeat(a::ERepeat::REJECT)).error().code ==
            a::EErrorCode::DUPLICATE_OPTION
        );
        auto first = a::parse("width=3 width=4", width.onRepeat(a::ERepeat::FIRST));
        REQUIRE(first && first.get<"width">().value() == 3);
        auto last = a::parse("width=3 width=4", width);
        REQUIRE(last && last.get<"width">().value() == 4);
        auto missing = a::parse("", width.required());
        REQUIRE(missing.error().code == a::EErrorCode::MISSING_REQUIRED);
    }
    {
        auto r = a::parse("--devices 1 2 --devices 3", devices);
        REQUIRE(r && (r.get<"devices">().value() == std::vector<int>{1, 2, 3}));
        auto r2 = a::parse("--devices 1 2 --devices 3", devices.onRepeat(a::ERepeat::LAST));
        REQUIRE(r2 && (r2.get<"devices">().value() == std::vector<int>{3}));
        auto r3 = a::parse("--devices 1 2 --devices 3", devices.onRepeat(a::ERepeat::FIRST));
        REQUIRE(r3 && (r3.get<"devices">().value() == std::vector<int>{1, 2}));
        REQUIRE(a::parse("--devices", devices).error().code == a::EErrorCode::MISSING_VALUE);
        REQUIRE(
            a::parse("--devices=1,2,3", devices.delimiter(',').arity(2, 2)).error().code ==
            a::EErrorCode::TOO_MANY_VALUES
        );
    }
    {
        const auto tag = a::Option<int, "tag", "t">{}.check(a::oneOf(1, 2, 3));
        REQUIRE(a::parse("tag=2", tag));
        REQUIRE(a::parse("tag=4", tag).error().code == a::EErrorCode::VALIDATION_FAILED);
        int factor = 4;
        const auto captured_converter = [factor](std::string_view s, int& out) -> a::EErrorCode {
            int base{};
            if (auto e = a::ValueParser<int>::parse(s, base); e != a::EErrorCode::OK)
                return e;
            out = base * factor;
            return a::EErrorCode::OK;
        };
        const auto captured_validator = [limit = 100](const int& n) { return n <= limit; };
        auto scaled = a::Option<int, "scaled">{}.convert(captured_converter).check(captured_validator);
        auto r = a::parse("scaled=20", scaled);
        REQUIRE(r && r.get<"scaled">().value() == 80);
        REQUIRE(a::parse("scaled=30", scaled).error().code == a::EErrorCode::VALIDATION_FAILED);
    }
    {
        auto a1 = a::Option<bool, "alpha", "a">{};
        auto b1 = a::Option<bool, "beta", "b">{};
        auto r = a::parse("-ab", a1, b1);
        REQUIRE(r && r.get<"alpha">().value() && r.get<"beta">().value());
        auto m = a::parse("-w -25 -g -.5", width, gain);
        REQUIRE(m && m.get<"width">().value() == -25 && m.get<"gain">().value() == -0.5);
    }
    {
        const auto src = a::Option<std::string, "source">{}.positional();
        const auto dst = a::Option<std::string, "target">{}.positional();
        auto r = a::parse("source.txt target.txt", src, dst);
        REQUIRE(
            r && r.get<"source">().value() == "source.txt" && r.get<"target">().value() == "target.txt"
        );
        auto dash = a::parse("-- --flag", src);
        REQUIRE(dash && dash.get<"source">().value() == "--flag");
        REQUIRE(a::parse("--flag", src).error().code == a::EErrorCode::UNKNOWN_OPTION);
        REQUIRE(a::parse("extra", width).error().code == a::EErrorCode::UNEXPECTED_POSITIONAL);
    }
    {
        const char* argv[] = {"prog", "--width", "1024", "-v", "--name=argv"};
        auto r = a::parse(5, argv, width, vsync, name);
        REQUIRE(r && r.get<"width">().value() == 1024 && r.get<"vsync">().value());
        REQUIRE(r.get<"name">().value() == "argv");
        std::array<std::string_view, 3> tokens{"--width=2", "--name=test", "--no-vsync"};
        auto from_views = a::parse(std::span<const std::string_view>{tokens}, width, vsync, name);
        REQUIRE(from_views && from_views.get<"width">().value() == 2 && !from_views.get<"vsync">().value());
    }
    {
        auto result = a::parse("name='first second'", name);
        REQUIRE(result && result.get<"name">().value() == "first second");
        auto result2 = a::parse("name=\"first\\\"second\"", name);
        REQUIRE(result2 && result2.get<"name">().value() == "first\"second");
        REQUIRE(a::parse("name='oops", name).error().code == a::EErrorCode::UNCLOSED_QUOTE);
        REQUIRE(a::parse("name=bad\\", name).error().code == a::EErrorCode::INVALID_ESCAPE);
    }
    {
        auto h = a::usage("app", width.desc("Output width"), gain);
        REQUIRE(h.find("Output width") != std::string::npos);
        REQUIRE(h.find("--width") != std::string::npos);
        auto message = a::describe(a::parse("width=bad", width).error(), width);
        REQUIRE(message.find("--width") != std::string::npos);
    }
    {
        auto r = a::parse("width=5", width);
        REQUIRE(r.get<"width">().has_value());
        int moved{};
        REQUIRE(r.take<"width">(&moved) && moved == 5 && !r.get<"width">().has_value());
    }
    std::puts("main test PASS");
}
