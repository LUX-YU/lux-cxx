#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <span>
#include <type_traits>

namespace lux::cxx
{
    template <typename Allocator = std::allocator<std::byte>>
    class SharedBytes final
    {
      public:
        using allocator_type = Allocator;
        using value_type = std::byte;
        using size_type = std::size_t;

        SharedBytes() noexcept = default;

        [[nodiscard]] static SharedBytes copyOf(
            std::span<const std::byte> source,
            const Allocator& allocator = Allocator{}
        )
        {
            if (source.empty()) return {};
            using ByteAllocator = typename std::allocator_traits<
                Allocator
            >::template rebind_alloc<std::byte>;
            auto owner = std::allocate_shared<std::byte[]>(
                ByteAllocator(allocator),
                source.size()
            );
            std::copy(source.begin(), source.end(), owner.get());
            return SharedBytes(
                std::shared_ptr<const std::byte>(owner, owner.get()),
                source.size()
            );
        }

        template <typename Owner>
        [[nodiscard]] static SharedBytes fromOwner(
            std::shared_ptr<Owner> owner,
            std::span<const std::byte> bytes
        ) noexcept
        {
            if (!owner || bytes.empty()) return {};
            return SharedBytes(
                std::shared_ptr<const std::byte>(
                    std::move(owner),
                    bytes.data()
                ),
                bytes.size()
            );
        }

        [[nodiscard]] SharedBytes subspan(
            std::size_t offset,
            std::size_t count = std::dynamic_extent
        ) const noexcept
        {
            if (offset > size_) return {};
            const auto available = size_ - offset;
            const auto result_size = count == std::dynamic_extent
                ? available
                : (std::min)(available, count);
            if (result_size == 0) return {};
            return SharedBytes(
                std::shared_ptr<const std::byte>(
                    data_,
                    data_.get() + offset
                ),
                result_size
            );
        }

        [[nodiscard]] std::span<const std::byte> view() const noexcept
        {
            return {data_.get(), size_};
        }

        [[nodiscard]] const std::byte* data() const noexcept
        {
            return data_.get();
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return size_ == 0;
        }

        [[nodiscard]] long use_count() const noexcept
        {
            return data_.use_count();
        }

      private:
        SharedBytes(
            std::shared_ptr<const std::byte> data,
            std::size_t size
        ) noexcept
            : data_(std::move(data)), size_(size)
        {
        }

        std::shared_ptr<const std::byte> data_;
        std::size_t size_ = 0;
    };
} // namespace lux::cxx
