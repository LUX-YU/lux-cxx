#include <lux/cxx/abi/Abi.hpp>
#include <lux/cxx/abi/BuildInfo.hpp>

#include <cassert>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace
{
    constexpr auto kFingerprint =
        lux::cxx::AbiFingerprint::fromCanonical("api_major=1;compiler=test");
    constexpr auto kExpected = lux::cxx::algorithm::Sha256Digest::fromHex(
        "e4e2182c677a0133ee7351522a69b603ba50b0e3fc7a7f09a27d90878a270e0d"
    );

    static_assert(kExpected.has_value());
    static_assert(kFingerprint.digest() == *kExpected);
    static_assert(lux::cxx::SemanticVersion{1, 2, 3}
        < lux::cxx::SemanticVersion{2, 0, 0});
    static_assert(std::is_standard_layout_v<lux::cxx::AbiStringView>);
    static_assert(std::is_trivially_copyable_v<lux::cxx::AbiByteView>);
    static_assert(sizeof(lux::cxx::AbiFingerprint)
        == lux::cxx::algorithm::Sha256Digest::byte_size);
}

int main()
{
    constexpr std::string_view text = "lux-cxx";
    constexpr lux::cxx::AbiStringView string_view{text};
    static_assert(string_view.view() == text);

    const std::uint8_t bytes[]{1, 2, 3};
    const lux::cxx::AbiByteView byte_view{bytes, 3};
    assert(byte_view.view().size() == 3);
    assert(!lux::cxx::AbiBuildInfo::canonical.empty());
    assert(lux::cxx::AbiBuildInfo::fingerprint()
        == lux::cxx::AbiFingerprint::fromCanonical(
            lux::cxx::AbiBuildInfo::canonical
        ));
}
