#include <lux/cxx/binary/Binary.hpp>

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace
{
    constexpr bool testConstexprBinary()
    {
        std::array<std::byte, 64> storage{};
        lux::cxx::BinarySpanWriter writer(storage);
        if (!writer.writeUnsigned<std::uint32_t>(0x12345678U)) return false;
        if (!writer.writeSigned<std::int16_t>(-123)) return false;
        if (!writer.writeBool(true)) return false;
        if (!writer.writeVarUint(300)) return false;
        if (!writer.writeVarInt(-42)) return false;

        lux::cxx::BinaryReader reader(writer.written());
        std::uint32_t unsigned_value = 0;
        std::int16_t signed_value = 0;
        bool boolean = false;
        std::uint64_t variable_unsigned = 0;
        std::int64_t variable_signed = 0;
        return reader.readUnsigned(unsigned_value) &&
               unsigned_value == 0x12345678U &&
               reader.readSigned(signed_value) && signed_value == -123 &&
               reader.readBool(boolean) && boolean &&
               reader.readVarUint(variable_unsigned) && variable_unsigned == 300 &&
               reader.readVarInt(variable_signed) && variable_signed == -42 &&
               reader.requireFullyConsumed();
    }
} // namespace

int main()
{
    static_assert(testConstexprBinary());
    static_assert(sizeof(lux::cxx::ScalarSchema) == 3);
    static_assert(lux::cxx::ScalarSchema{lux::cxx::EScalarKind::F64}.isKnown());
    static_assert(lux::cxx::binary_floating_point<float>);
    static_assert(lux::cxx::binary_floating_point<double>);
    static_assert(!lux::cxx::binary_floating_point<int>);

    std::array<std::byte, 8> big_endian_storage{};
    lux::cxx::BigEndianBinarySpanWriter big_writer(big_endian_storage);
    assert(big_writer.writeUnsigned<std::uint32_t>(0x01020304U));
    assert(big_endian_storage[0] == std::byte{1});
    assert(big_endian_storage[3] == std::byte{4});

    lux::cxx::BigEndianBinaryReader big_reader(big_writer.written());
    std::uint32_t big_value = 0;
    assert(big_reader.readUnsigned(big_value));
    assert(big_value == 0x01020304U);

    lux::cxx::BinaryVectorWriter vector_writer;
    assert(vector_writer.writeFloat(3.5F));
    assert(vector_writer.writeString("lux"));
    lux::cxx::BinaryReader vector_reader(vector_writer.data());
    float float_value = 0.0F;
    std::string_view text;
    assert(vector_reader.readFloat(float_value));
    assert(float_value == 3.5F);
    assert(vector_reader.readString(3, text));
    assert(text == "lux");
    assert(vector_reader.requireFullyConsumed());

    std::array<std::byte, 16> floating_storage{};
    lux::cxx::BinarySpanWriter floating_writer(floating_storage);
    assert(floating_writer.writeFloat(-0.0F));
    assert(floating_writer.writeFloat((std::numeric_limits<float>::quiet_NaN)()));
    lux::cxx::BinaryReader floating_reader(floating_writer.written());
    float zero = 1.0F;
    float nan = 0.0F;
    assert(floating_reader.readFloat(zero));
    assert(zero == 0.0F && !std::signbit(zero));
    assert(floating_reader.readFloat(nan));
    assert(std::isnan(nan));

    constexpr std::array negative_zero{
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x80}
    };
    lux::cxx::BinaryReader non_canonical_float(negative_zero);
    float rejected_zero = 0.0F;
    assert(!non_canonical_float.readFloat(rejected_zero));
    assert(
        non_canonical_float.error().code
        == lux::cxx::EBinaryErrorCode::NON_CANONICAL_VALUE
    );

    lux::cxx::BinaryReader preserved_float(negative_zero);
    assert(preserved_float.readFloat(
        rejected_zero,
        lux::cxx::EFloatingPointPolicy::PRESERVE_BITS
    ));
    assert(std::signbit(rejected_zero));

    const std::array non_canonical{std::byte{0x80}, std::byte{0x00}};
    lux::cxx::BinaryReader invalid_varuint(non_canonical);
    std::uint64_t value = 0;
    assert(!invalid_varuint.readVarUint(value));
    assert(
        invalid_varuint.error().code ==
        lux::cxx::EBinaryErrorCode::NON_CANONICAL_VALUE
    );

    const std::array invalid_bool{std::byte{2}};
    lux::cxx::BinaryReader bool_reader(invalid_bool);
    bool bool_value = false;
    assert(!bool_reader.readBool(bool_value));
    assert(bool_reader.error().code == lux::cxx::EBinaryErrorCode::INVALID_VALUE);

    std::array<std::byte, 1> small_output{};
    lux::cxx::BinarySpanWriter small_writer(small_output);
    assert(!small_writer.writeUnsigned<std::uint32_t>(42));
    assert(
        small_writer.error().code ==
        lux::cxx::EBinaryErrorCode::OUTPUT_EXHAUSTED
    );
    return 0;
}
