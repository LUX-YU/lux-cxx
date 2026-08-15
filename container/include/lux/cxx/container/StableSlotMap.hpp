#pragma once

#include <lux/cxx/container/SlotMap.hpp>

#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace lux::cxx
{
    struct NoAux final
    {
    };

    template <
        typename Value,
        typename Tag = void,
        typename Aux = NoAux,
        std::size_t BlockSize = 256,
        typename Allocator = std::allocator<Value>,
        std::unsigned_integral Index = std::uint32_t,
        std::unsigned_integral Generation = std::uint32_t
    >
    class StableSlotMap final
    {
        static_assert(BlockSize > 0);

      private:
        static constexpr Index INVALID_INDEX =
            (std::numeric_limits<Index>::max)();

        struct Slot final
        {
            [[no_unique_address]] Aux aux{};
            std::size_t dense_position = 0;
            Index next_free = INVALID_INDEX;
            Generation generation = 1;
            bool occupied = false;
        };

        struct Block final
        {
            alignas(Value) std::byte values[sizeof(Value) * BlockSize];
            std::array<Slot, BlockSize> slots;

            [[nodiscard]] Value* value(std::size_t offset) noexcept
            {
                return std::launder(reinterpret_cast<Value*>(
                    values + sizeof(Value) * offset
                ));
            }

            [[nodiscard]] const Value* value(std::size_t offset) const noexcept
            {
                return std::launder(reinterpret_cast<const Value*>(
                    values + sizeof(Value) * offset
                ));
            }
        };

        using block_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<Block>;
        using block_traits = std::allocator_traits<block_allocator>;
        using pointer_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<Block*>;
        using index_allocator = typename std::allocator_traits<
            Allocator
        >::template rebind_alloc<Index>;

      public:
        using key_type = SlotKey<Tag, Index, Generation>;
        using value_type = Value;
        using aux_type = Aux;
        using allocator_type = Allocator;
        using size_type = std::size_t;

        StableSlotMap()
            : StableSlotMap(Allocator{})
        {
        }

        explicit StableSlotMap(const Allocator& allocator)
            : allocator_(allocator),
              blocks_(pointer_allocator(allocator)),
              dense_indices_(index_allocator(allocator))
        {
        }

        StableSlotMap(const StableSlotMap&) = delete;
        StableSlotMap& operator=(const StableSlotMap&) = delete;
        StableSlotMap& operator=(StableSlotMap&&) = delete;

        StableSlotMap(StableSlotMap&& other) noexcept
            : allocator_(std::move(other.allocator_)),
              blocks_(std::move(other.blocks_)),
              dense_indices_(std::move(other.dense_indices_)),
              free_head_(other.free_head_)
        {
            other.blocks_.clear();
            other.dense_indices_.clear();
            other.free_head_ = INVALID_INDEX;
        }

        ~StableSlotMap()
        {
            destroyAndDeallocate();
        }

        [[nodiscard]] allocator_type get_allocator() const noexcept
        {
            return allocator_;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return dense_indices_.size();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return dense_indices_.empty();
        }

        [[nodiscard]] std::size_t capacity() const noexcept
        {
            return blocks_.size() * BlockSize;
        }

        void reserve(std::size_t count)
        {
            while (capacity() < count) addBlock();
            dense_indices_.reserve(count);
        }

        template <typename... Args>
        key_type emplace(Args&&... args)
        {
            if (free_head_ == INVALID_INDEX) addBlock();
            const Index index = free_head_;
            Slot& target = slot(index);

            std::construct_at(
                value(index),
                std::forward<Args>(args)...
            );
            try
            {
                dense_indices_.push_back(index);
            }
            catch (...)
            {
                std::destroy_at(value(index));
                throw;
            }

            free_head_ = target.next_free;
            target.next_free = INVALID_INDEX;
            target.dense_position = dense_indices_.size() - 1;
            target.occupied = true;
            return key_type{index, target.generation};
        }

        key_type insert(const Value& value)
        {
            return emplace(value);
        }

        key_type insert(Value&& value)
        {
            return emplace(std::move(value));
        }

        bool erase(key_type key) noexcept(std::is_nothrow_destructible_v<Value>)
        {
            if (!isValid(key)) return false;
            Slot& target = slot(key.index);
            std::destroy_at(value(key.index));

            const auto position = target.dense_position;
            const Index moved_index = dense_indices_.back();
            dense_indices_[position] = moved_index;
            dense_indices_.pop_back();
            if (position < dense_indices_.size())
            {
                slot(moved_index).dense_position = position;
            }

            target.occupied = false;
            if (target.generation != (std::numeric_limits<Generation>::max)())
            {
                ++target.generation;
                if (target.generation != (std::numeric_limits<Generation>::max)())
                {
                    target.next_free = free_head_;
                    free_head_ = key.index;
                }
            }
            return true;
        }

        void clear() noexcept(std::is_nothrow_destructible_v<Value>)
        {
            for (const Index index : dense_indices_)
            {
                Slot& target = slot(index);
                std::destroy_at(value(index));
                target.occupied = false;
            }
            dense_indices_.clear();
            free_head_ = INVALID_INDEX;

            for (std::size_t index = capacity(); index > 0; --index)
            {
                Slot& target = slot(static_cast<Index>(index - 1));
                if (target.generation == (std::numeric_limits<Generation>::max)())
                {
                    target.next_free = INVALID_INDEX;
                    continue;
                }
                ++target.generation;
                if (target.generation != (std::numeric_limits<Generation>::max)())
                {
                    target.next_free = free_head_;
                    free_head_ = static_cast<Index>(index - 1);
                }
            }
        }

        [[nodiscard]] bool isValid(key_type key) const noexcept
        {
            if (key.isNull() || key.index >= capacity()) return false;
            const Slot& target = slot(key.index);
            return target.occupied && target.generation == key.gen;
        }

        [[nodiscard]] Value* find(key_type key) noexcept
        {
            return isValid(key) ? value(key.index) : nullptr;
        }

        [[nodiscard]] const Value* find(key_type key) const noexcept
        {
            return isValid(key) ? value(key.index) : nullptr;
        }

        [[nodiscard]] Value& at(key_type key)
        {
            if (auto* value = find(key)) return *value;
            throw std::out_of_range("StableSlotMap::at: invalid key");
        }

        [[nodiscard]] const Value& at(key_type key) const
        {
            if (const auto* value = find(key)) return *value;
            throw std::out_of_range("StableSlotMap::at: invalid key");
        }

        [[nodiscard]] Aux* aux(key_type key) noexcept
        {
            return isValid(key) ? std::addressof(slot(key.index).aux) : nullptr;
        }

        [[nodiscard]] const Aux* aux(key_type key) const noexcept
        {
            return isValid(key) ? std::addressof(slot(key.index).aux) : nullptr;
        }

        template <bool IsConst>
        class BasicIterator final
        {
            using map_type = std::conditional_t<
                IsConst,
                const StableSlotMap,
                StableSlotMap
            >;

          public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Value;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t<IsConst, const Value&, Value&>;
            using pointer = std::conditional_t<IsConst, const Value*, Value*>;

            constexpr BasicIterator() noexcept = default;

            constexpr BasicIterator(map_type* map, std::size_t position) noexcept
                : map_(map), position_(position)
            {
            }

            [[nodiscard]] reference operator*() const noexcept
            {
                return *map_->value(map_->dense_indices_[position_]);
            }

            [[nodiscard]] pointer operator->() const noexcept
            {
                return map_->value(map_->dense_indices_[position_]);
            }

            BasicIterator& operator++() noexcept
            {
                ++position_;
                return *this;
            }

            BasicIterator operator++(int) noexcept
            {
                BasicIterator copy = *this;
                ++*this;
                return copy;
            }

            [[nodiscard]] bool operator==(
                const BasicIterator&
            ) const noexcept = default;

          private:
            map_type* map_ = nullptr;
            std::size_t position_ = 0;
        };

        using iterator = BasicIterator<false>;
        using const_iterator = BasicIterator<true>;

        [[nodiscard]] iterator begin() noexcept
        {
            return iterator(this, 0);
        }

        [[nodiscard]] iterator end() noexcept
        {
            return iterator(this, dense_indices_.size());
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return const_iterator(this, 0);
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return const_iterator(this, dense_indices_.size());
        }

      private:
        [[nodiscard]] Slot& slot(Index index) noexcept
        {
            return blocks_[index / BlockSize]->slots[index % BlockSize];
        }

        [[nodiscard]] const Slot& slot(Index index) const noexcept
        {
            return blocks_[index / BlockSize]->slots[index % BlockSize];
        }

        [[nodiscard]] Value* value(Index index) noexcept
        {
            return blocks_[index / BlockSize]->value(index % BlockSize);
        }

        [[nodiscard]] const Value* value(Index index) const noexcept
        {
            return blocks_[index / BlockSize]->value(index % BlockSize);
        }

        void addBlock()
        {
            if (capacity() > static_cast<std::size_t>(INVALID_INDEX) - BlockSize)
            {
                throw std::length_error("StableSlotMap index space exhausted");
            }

            Block* block = block_traits::allocate(allocator_, 1);
            try
            {
                block_traits::construct(allocator_, block);
            }
            catch (...)
            {
                block_traits::deallocate(allocator_, block, 1);
                throw;
            }
            try
            {
                blocks_.push_back(block);
            }
            catch (...)
            {
                block_traits::destroy(allocator_, block);
                block_traits::deallocate(allocator_, block, 1);
                throw;
            }

            const auto base = (blocks_.size() - 1) * BlockSize;
            for (std::size_t offset = 0; offset < BlockSize; ++offset)
            {
                Slot& target = block->slots[offset];
                target.next_free = free_head_;
                free_head_ = static_cast<Index>(base + offset);
            }
        }

        void destroyAndDeallocate() noexcept
        {
            for (const Index index : dense_indices_)
            {
                std::destroy_at(value(index));
            }
            for (Block* block : blocks_)
            {
                block_traits::destroy(allocator_, block);
                block_traits::deallocate(allocator_, block, 1);
            }
        }

        block_allocator allocator_;
        std::vector<Block*, pointer_allocator> blocks_;
        std::vector<Index, index_allocator>    dense_indices_;
        Index free_head_ = INVALID_INDEX;
    };
} // namespace lux::cxx
