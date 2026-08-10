#include <lux/cxx/algorithm/sha256.hpp>

#include <iostream>
#include <string_view>

namespace
{
    bool check(
        const bool condition,
        std::string_view description
    )
    {
        if (condition) return true;
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
}

int main()
{
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

    return success ? 0 : 1;
}
