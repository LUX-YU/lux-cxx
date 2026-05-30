#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace lux::cxx::archtype {

    /// Maximum number of distinct component types. Signature is a uint64_t so this must stay <= 64.
    constexpr std::size_t kMaxComponents = 64;

    /// Entities per chunk. Power of two so row -> chunk/index is a shift.
    constexpr std::size_t kChunkSize  = 512;
    static_assert((kChunkSize & (kChunkSize - 1)) == 0, "kChunkSize must be a power of two");
    constexpr std::size_t kChunkShift = std::countr_zero(kChunkSize);
    constexpr std::size_t kChunkMask  = kChunkSize - 1;

    using ComponentTypeID = std::uint16_t;
    constexpr ComponentTypeID kInvalidComponentTypeID = std::numeric_limits<ComponentTypeID>::max();

    /// Sentinel value used in EntityRecord::arch_id (uint32 form) to signal "no archetype".
    constexpr std::uint32_t kInvalidArchId = std::numeric_limits<std::uint32_t>::max();

    /// 16-bit sentinel used inside the compact EntitySlot for "no archetype".
    /// Limits the number of archetypes to 65 535 (one is reserved for the
    /// sentinel) — generous for any real game.
    constexpr std::uint16_t kNoArchSlot = 0xFFFFu;

    /// End-of-list marker for Registry's intrusive entity free list.
    /// When a slot is on the free list, its `row` field stores the next
    /// recycled id, or `kFreeListEnd` for the last slot in the chain.
    constexpr std::uint32_t kFreeListEnd = std::numeric_limits<std::uint32_t>::max();

    /**
     * @brief 32-bit entity handle: 22-bit id + 10-bit generation.
     *
     * generation is bumped on destroy, so a stale handle can be detected in O(1)
     * via Registry::valid(Entity).
     */
    struct Entity {
        std::uint32_t value = 0xFFFFFFFFu;

        static constexpr std::uint32_t kIdBits  = 22;
        static constexpr std::uint32_t kGenBits = 10;
        static constexpr std::uint32_t kIdMask  = (1u << kIdBits) - 1;
        static constexpr std::uint32_t kGenMask = ((1u << kGenBits) - 1) << kIdBits;
        static constexpr std::uint32_t kInvalidValue = 0xFFFFFFFFu;

        constexpr Entity() noexcept = default;
        constexpr Entity(std::uint32_t id, std::uint32_t gen) noexcept
            : value((id & kIdMask) | ((gen & ((1u << kGenBits) - 1)) << kIdBits)) {}

        /// Build an Entity from a raw packed value (no validation).
        static constexpr Entity from_raw(std::uint32_t raw) noexcept {
            Entity e; e.value = raw; return e;
        }

        constexpr std::uint32_t id()  const noexcept { return value & kIdMask; }
        constexpr std::uint32_t gen() const noexcept { return (value & kGenMask) >> kIdBits; }
        constexpr std::uint32_t raw() const noexcept { return value; }

        constexpr bool valid() const noexcept { return value != kInvalidValue; }

        constexpr bool operator==(const Entity&) const noexcept = default;
        constexpr bool operator!=(const Entity&) const noexcept = default;

        explicit constexpr operator bool() const noexcept { return valid(); }
    };

    inline constexpr Entity kInvalidEntity{};

    /// Number of 64-bit blocks needed to hold kMaxComponents bits.
    /// Auto-scales: kMaxComponents=64 → 1 block (same memory as the previous
    /// uint64_t Signature). Set kMaxComponents > 64 to unlock more component
    /// types; signature operations become per-block but each block compiles to
    /// a single uint64 op.
    constexpr std::size_t kSignatureBlocks = (kMaxComponents + 63) / 64;

    /**
     * @brief Bitset signature carrying up to kMaxComponents component bits.
     *        Stored as kSignatureBlocks 64-bit words; for the common 64-component
     *        case this is identical in size and speed to the original uint64_t.
     */
    struct Signature {
        std::uint64_t bits[kSignatureBlocks] = {};

        constexpr Signature() noexcept = default;

        constexpr void set(std::uint32_t i) noexcept {
            bits[i >> 6] |=  (std::uint64_t{1} << (i & 63));
        }
        constexpr void reset(std::uint32_t i) noexcept {
            bits[i >> 6] &= ~(std::uint64_t{1} << (i & 63));
        }
        constexpr bool test(std::uint32_t i) const noexcept {
            return (bits[i >> 6] >> (i & 63)) & 1ull;
        }

        constexpr bool empty() const noexcept {
            for (auto b : bits) if (b != 0) return false;
            return true;
        }

        constexpr int count() const noexcept {
            int n = 0;
            for (auto b : bits) n += std::popcount(b);
            return n;
        }

        /// True iff every bit set in @p req is also set in *this.
        constexpr bool contains(const Signature& req) const noexcept {
            for (std::size_t i = 0; i < kSignatureBlocks; ++i) {
                if ((bits[i] & req.bits[i]) != req.bits[i]) return false;
            }
            return true;
        }

        /// True iff no bit set in @p excl is set in *this.
        constexpr bool excludes(const Signature& excl) const noexcept {
            for (std::size_t i = 0; i < kSignatureBlocks; ++i) {
                if ((bits[i] & excl.bits[i]) != 0) return false;
            }
            return true;
        }

        constexpr bool operator==(const Signature& o) const noexcept {
            for (std::size_t i = 0; i < kSignatureBlocks; ++i) {
                if (bits[i] != o.bits[i]) return false;
            }
            return true;
        }
        constexpr bool operator!=(const Signature& o) const noexcept { return !(*this == o); }
    };

    struct SignatureHash {
        std::size_t operator()(const Signature& s) const noexcept {
            // splitmix64 per block, XOR-mixed into running hash.
            std::uint64_t h = 0;
            for (std::size_t i = 0; i < kSignatureBlocks; ++i) {
                std::uint64_t x = s.bits[i];
                x ^= x >> 33;
                x *= 0xff51afd7ed558ccdULL;
                x ^= x >> 33;
                x *= 0xc4ceb9fe1a85ec53ULL;
                x ^= x >> 33;
                // boost-style hash combiner so block order matters.
                h ^= x + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            }
            return static_cast<std::size_t>(h);
        }
    };

} // namespace lux::cxx::archtype

namespace lux::cxx::archtype {

    /**
     * @brief Back-compat helper. Equivalent to arch_sig.contains(query_sig).
     *        Kept so older call sites (`matchSignature(a, b)`) still compile.
     */
    inline constexpr bool matchSignature(const Signature& arch_sig, const Signature& query_sig) noexcept {
        return arch_sig.contains(query_sig);
    }

} // namespace lux::cxx::archtype

namespace std {
    template<> struct hash<lux::cxx::archtype::Entity> {
        std::size_t operator()(const lux::cxx::archtype::Entity& e) const noexcept {
            return std::hash<std::uint32_t>{}(e.value);
        }
    };
}
