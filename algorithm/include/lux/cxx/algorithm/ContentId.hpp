#pragma once

#include <lux/cxx/algorithm/sha256.hpp>

#include <compare>

namespace lux::cxx::algorithm
{
    template <typename Tag, typename Digest = Sha256Digest>
    class ContentId final
    {
      public:
        using tag_type = Tag;
        using digest_type = Digest;

        constexpr ContentId() noexcept = default;
        constexpr explicit ContentId(Digest digest) noexcept
            : digest_(digest)
        {
        }

        [[nodiscard]] constexpr const Digest& digest() const noexcept
        {
            return digest_;
        }

        [[nodiscard]] constexpr auto operator<=>(
            const ContentId&
        ) const noexcept = default;

      private:
        Digest digest_{};
    };
} // namespace lux::cxx::algorithm
