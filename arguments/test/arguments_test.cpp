#include <lux/cxx/arguments/Arguments.hpp>
#include <cassert>
#include <iostream>

using namespace lux::cxx;
namespace 
{
    /// 方便地把命令行字符串拆分为 argv 形式
    std::vector<char*> argv_from_string(const std::string& cmd, std::vector<std::string>& storage)
    {
        std::istringstream iss(cmd);
        std::string token;
        while (iss >> token) storage.push_back(token);
        std::vector<char*> argv;
        for (auto& s : storage) argv.push_back(s.data());
        return argv;
    }

    /// 注册一组选项，返回已就绪的 parser
    Parser build_parser()
    {
        Parser p("demo");

        // 必选整数
        p.add<int>("count", "c")
            .desc("Number of iterations")
            .required();

        // 单个字符串，带默认值
        p.add<std::string>("file", "f")
            .desc("Output file")
            .def(std::string{ "out.txt" });

        // 浮点数，演示 --opt=value 语法
        p.add<double>("threshold", "t")
            .desc("Match threshold")
            .def(0.5);

        // 多值整型
        p.add<std::vector<int>>("values", "v")
            .desc("List of integers")
            .multi();

        // 布尔旗标，可写作 -V 或 --verbose，也允许 --verbose=false
        p.add<bool>("verbose", "V")
            .desc("Verbose mode");

        return p;
    }

    /// 单个成功用例断言
    void test_success_case()
    {
        Parser p = build_parser();

        std::cout << p.usage() << std::endl;
        const std::string cmd = R"(demo -c 3 --file="data.bin" -v 1 2 3 -V --threshold=0.75)";
        auto expected_opts = p.parse(cmd);
        assert(expected_opts.has_value());
        auto& opts = expected_opts.value();
        assert(opts.contains("count") && opts.get("count").as<int>().value() == 3);
        assert(opts.get("file").as<std::string>().value() == "data.bin");
        assert(opts.get("verbose").as<bool>().value() == true);
        assert(opts.get("threshold").as<double>().value() == 0.75);
        auto vec = opts.get("values").as<std::vector<int>>().value();
        assert((vec == std::vector<int>{1, 2, 3}));
    }

    /// 用默认值解析
    void test_defaults()
    {
        Parser p = build_parser();

        const std::string cmd = R"(demo -c 1)"; // 只给了必需项
        auto expected_opts = p.parse(cmd);
        assert(expected_opts.has_value());
        auto& opts = expected_opts.value();

        assert(opts.get("file").as<std::string>().value() == "out.txt");   // 默认值
        assert(opts.get("threshold").as<double>().value() == 0.5);
        assert(!opts.get("verbose")); // 未出现
        assert(!opts.contains("values")); // 未出现
    }

    /// 布尔旗标显式 false
    void test_bool_explicit()
    {
        Parser p = build_parser();

        const std::string cmd = R"(demo -c 4 --verbose=false)";
        auto expected_opts = p.parse(cmd);
        assert(expected_opts.has_value());
        auto& opts = expected_opts.value();

        assert(opts.get("verbose").as<bool>().value() == false);
    }
} // namespace

int main()
{
    try {
        test_success_case();
        test_defaults();
        test_bool_explicit();
        std::cout << "ALL TESTS PASSED ✅\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "TEST FAILED: " << ex.what() << '\n';
        return 1;
    }
}
