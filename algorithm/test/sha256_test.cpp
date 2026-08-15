#include <lux/cxx/algorithm/sha256.hpp>
#include <lux/cxx/algorithm/ContentId.hpp>

#include <array>
#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    template<std::size_t Size>
    consteval std::array<char, Size> repeatedA()
    {
        std::array<char, Size> result{};
        result.fill('a');
        return result;
    }

    bool check(
        const bool condition,
        std::string_view description
    )
    {
        if (condition) return true;
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }

    constexpr bool testConstexprSha256()
    {
        using lux::cxx::algorithm::Sha256;
        using lux::cxx::algorithm::Sha256Digest;

        constexpr auto digest = Sha256::hash("abc");
        std::array<char, Sha256Digest::hex_size> output{};
        digest.formatHex(output);
        constexpr std::string_view expected =
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad";
        if (std::string_view(output.data(), output.size()) != expected)
        {
            return false;
        }
        const auto parsed = Sha256Digest::fromHex(expected);
        return parsed && *parsed == digest;
    }

    constexpr bool testConstexprBoundary()
    {
        constexpr auto input = repeatedA<56>();
        constexpr auto digest = lux::cxx::algorithm::Sha256::hash(
            std::string_view(input.data(), input.size())
        );
        std::array<char, lux::cxx::algorithm::Sha256Digest::hex_size> output{};
        digest.formatHex(output);
        return std::string_view(output.data(), output.size()) ==
            "b35439a4ac6f0948b6d6f9e3c6af0f5"
            "f590ce20f1bde7090ef7970686ec6738a";
    }
}

int main()
{
    static_assert(testConstexprSha256());
    static_assert(testConstexprBoundary());
    static_assert(sizeof(lux::cxx::algorithm::Sha256Digest) == 32);

    using lux::cxx::algorithm::Sha256;
    using lux::cxx::algorithm::toHex;

    bool success = true;
    success &= check(
        toHex(Sha256::hash("")) ==
            "e3b0c44298fc1c149afbf4c8996fb924"
            "27ae41e4649b934ca495991b7852b855",
        "empty SHA-256 vector"
    );

    success &= check(
        !lux::cxx::algorithm::Sha256Digest::fromHex("abc"),
        "reject short digest"
    );
    success &= check(
        !lux::cxx::algorithm::Sha256Digest::fromHex(
            "zz7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad"
        ),
        "reject invalid digest character"
    );

    struct ContentTag;
    using Content = lux::cxx::algorithm::ContentId<ContentTag>;
    constexpr Content content(Sha256::hash("content"));
    static_assert(content.digest() == Sha256::hash("content"));
    success &= check(
        toHex(Sha256::hash("abc")) ==
            "ba7816bf8f01cfea414140de5dae2223"
            "b00361a396177a9cb410ff61f20015ad",
        "abc SHA-256 vector"
    );

    Sha256 incremental;
    incremental.update("a");
    incremental.update("b");
    incremental.update("c");
    success &= check(
        incremental.digest() == Sha256::hash("abc"),
        "incremental updates equal one-shot hashing"
    );
    success &= check(
        incremental.digest() == incremental.digest(),
        "digest is non-destructive"
    );

    incremental.reset();
    incremental.update(
        "abcdbcdecdefdefgefghfghighijhijk"
        "ijkljklmklmnlmnomnopnopq"
    );
    success &= check(
        toHex(incremental.digest()) ==
            "248d6a61d20638b8e5c026930c3e6039"
            "a33ce45964ff2167f6ecedd419db06c1",
        "multi-block SHA-256 vector"
    );

    constexpr std::array<std::pair<std::size_t, std::string_view>, 5> boundaries{{
        {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
    }};
    for (const auto& [size, expected] : boundaries)
    {
        const std::string input(size, 'a');
        const auto actual = toHex(Sha256::hash(input));
        if (actual != expected)
        {
            std::cerr << "padding boundary " << size << ": " << actual << '\n';
            success = false;
        }

        Sha256 chunked;
        chunked.update(std::string_view(input).substr(0, 1));
        chunked.update(std::string_view(input).substr(1, size / 2));
        chunked.update(std::string_view(input).substr(1 + size / 2));
        success &= check(
            chunked.digest() == Sha256::hash(input),
            "chunked boundary equals one-shot"
        );
    }

    return success ? 0 : 1;
}
