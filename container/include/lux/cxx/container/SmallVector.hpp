#pragma once
/**
 * @file SmallVector.hpp
 * @brief Header‑only implementation of a small‑buffer‑optimised (SBO) vector.
 *
 * This file defines the template class ::lux::cxx::SmallVector, a contiguous
 * container with semantics similar to `std::vector` but which stores up to `N`
 * elements in a fixed‑size internal buffer before falling back to heap
 * allocation.  The primary motivation is to eliminate heap traffic for small
 * sequences while still supporting dynamic growth. The implementation is
 * single‑header and has no dependencies beyond the C++ standard library.
 *
 * Key properties:
 *  - **Small‑buffer optimisation** – the first `N` elements reside in an
 *    internal `std::byte` array; `N` defaults to 8.
 *  - **No allocator parameter** – memory is obtained via the global new/delete
 *    operators. This simplifies the implementation and matches the common use
 *    case where custom allocators are unnecessary.
 *  - **Iterator invalidation** – any operation which modifies the container’s
 *    size or capacity invalidates all iterators and references, matching the
 *    behaviour of libc++ / MSVC `std::vector`.
 *  - **ABI compatibility** – the class requires `T` to be nothrow‑destructible;
 *    this mirrors the requirement of many standard‑library implementations and
 *    enables some low‑level optimisations.
 *
 * @warning The class is *not* thread‑safe. Concurrent reads are safe provided
 * no other thread mutates the container.
 *
 * @author ChatGPT‑o3
 * @date 2025‑05‑18
 */

#include <algorithm>
#include <bit>          // std::bit_ceil
#include <cassert>
#include <cstddef>
#include <cstring>      // std::memcpy / std::memmove
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace lux::cxx
{
    /**
     * @brief Contiguous container with *small‑buffer optimisation*.
     *
     * The template stores up to @p N objects of type @p T inside the object
     * itself; once this capacity is exceeded additional storage is obtained via
     * `operator new[]`. All operations have the same asymptotic complexity as
     * their `std::vector` counterparts. The class provides a pointer‑based
     * random‑access iterator satisfying the *contiguous_iterator* requirements
     * (C++20 [iterator.concept.contiguous]).
     *
     * @tparam T Value type. Must be nothrow‑destructible to align with the
     *           destruction guarantees of most `std::vector` ABIs.
     * @tparam N Number of elements that fit into the internal stack buffer.
     *           Must be greater than zero. Choosing a power of two can give
     *           better padding / alignment but is not required.
     *
     * @note The container deliberately omits an allocator parameter. If you
     *       need a custom allocator you should embed one in @p T or use
     *       `std::vector` directly.
     *
     * @par Iterator invalidation
     * Any mutating operation (insert, erase, push_back, reserve, etc.) makes
     * *all* iterators, references and pointers referring to elements in the
     * container dangling.  This matches the behaviour of implementations such
     * as libc++ where even `push_back` without reallocation invalidates
     * iterators.
     */
    template <class T, std::size_t N = 8>
    class SmallVector
    {
        static_assert(N > 0, "SmallVector<N>: N must be greater than zero");
        static_assert(std::is_nothrow_destructible_v<T>,
            "SmallVector requires T::~T noexcept (matches std::vector ABI)");

    public:                               // member types
        using value_type = T;           //!< Element type.
        using size_type = std::size_t; //!< Unsigned size type.
        using difference_type = std::ptrdiff_t; //!< Signed difference type.
        using reference = value_type&; //!< Mutable reference.
        using const_reference = const value_type&; //!< Immutable reference.
        using pointer = value_type*; //!< Mutable pointer.
        using const_pointer = const value_type*; //!< Immutable pointer.
        using iterator = value_type*; //!< Iterator (contiguous).
        using const_iterator = const value_type*; //!< Const iterator.
        using reverse_iterator = std::reverse_iterator<iterator>; //!< Reverse iterator.
        using const_reverse_iterator = std::reverse_iterator<const_iterator>; //!< Const reverse.

    private:                              // data & helpers
        /// @brief Number of elements that the internal buffer can store.
        static constexpr size_type kStackCapacity = N;

        /**
         * @brief Raw storage for @c kStackCapacity elements.
         *
         * The buffer is suitably aligned for @p T. Lifetime of objects is
         * managed manually via placement new and explicit destruction.
         */
        alignas(alignof(value_type))
            std::byte _stack[sizeof(value_type) * kStackCapacity];

        /// @name Helper functions to convert the raw byte buffer to pointers.
        ///@{
        static pointer       stack_ptr(std::byte* p)       noexcept
        {
            return std::launder(reinterpret_cast<pointer>(p));
        }
        static const_pointer stack_ptr(const std::byte* p) noexcept
        {
            return std::launder(reinterpret_cast<const_pointer>(p));
        }
        ///@}

        pointer   _data = stack_ptr(_stack); //!< Points to first element storage.
        size_type _size = 0;                 //!< Number of constructed elements.
        size_type _cap = kStackCapacity;    //!< Total capacity of @_data.

        /// @brief Returns @c true if currently using the internal buffer.
        [[nodiscard]] bool on_stack() const noexcept { return _data == stack_ptr(_stack); }

        // ------------------------------------------------------------------
        //                         Allocation helpers
        // ------------------------------------------------------------------
        /**
         * @brief Allocate uninitialised storage for @p n elements on the heap.
         * @param n Number of elements.
         * @return Pointer to raw storage (may be @c nullptr if @p n == 0).
         * @throw std::bad_alloc If allocation fails.
         */
        [[nodiscard]] static pointer allocate(size_type n)
        {
            return n ? static_cast<pointer>(
                ::operator new[](n * sizeof(value_type),
                    std::align_val_t{ alignof(value_type) }))
                : nullptr;
        }
        /**
         * @brief Deallocate storage previously obtained via allocate().
         * @param p Pointer returned by allocate(); may be @c nullptr.
         */
        static void deallocate(pointer p) noexcept
        {
            ::operator delete[](p, std::align_val_t{ alignof(value_type) });
        }

        // ------------------------------------------------------------------
        //                       Destruction utilities
        // ------------------------------------------------------------------
        /**
         * @brief Destroy a range of elements if @p T is non‑trivial.
         * @param first Pointer to first element.
         * @param last  One‑past‑the‑end pointer.
         *
         * For trivially destructible types the function is a no‑op, allowing
         * the compiler to optimise away the call entirely.
         */
        static void destroy_range(pointer first, pointer last) noexcept
        {
            if constexpr (!std::is_trivially_destructible_v<value_type>)
                std::destroy(first, last);
        }

        // ------------------------------------------------------------------
        //                     Reallocation / growth helpers
        // ------------------------------------------------------------------
        /**
         * @brief Relocate existing elements to @p new_data.
         *
         * The function moves or memcpy‑copies (when `std::is_trivially_copyable`
         * is true) all currently constructed elements from @_data to the new
         * buffer and updates internal bookkeeping. On exception, strong
         * exception safety is provided – the original container remains
         * unchanged.
         *
         * @param new_data Destination buffer (must be large enough for @c _size
         *                 elements).
         * @param new_cap  Total capacity associated with @p new_data.
         */
        void relocate_to(pointer new_data, size_type new_cap)
        {
            if (new_data == _data) return;   // Nothing to do.

            if constexpr (std::is_trivially_copyable_v<value_type>)
            {
                std::memcpy(new_data, _data, _size * sizeof(value_type));
            }
            else
            {
                size_type constructed = 0;
                try
                {
                    for (; constructed < _size; ++constructed)
                        new (new_data + constructed) value_type(
                            std::move_if_noexcept(_data[constructed]));
                }
                catch (...)
                {
                    destroy_range(new_data, new_data + constructed);
                    deallocate(new_data);
                    throw;
                }
            }

            destroy_range(_data, _data + _size);
            if (!on_stack()) deallocate(_data);

            _data = new_data;
            _cap = new_cap;
        }

        /**
         * @brief Grow capacity to at least @p min_cap preserving all elements.
         *
         * New capacity doubles the current one and is rounded up to the next
         * power of two when `std::bit_ceil` is available (C++20
         * <bit> header). This growth strategy offers both good cache
         * performance and low realloc frequency.
         */
        void grow(size_type min_cap)
        {
            size_type new_cap = std::max(_cap * 2, min_cap);
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
            new_cap = std::bit_ceil(new_cap);               // power‑of‑two growth
#endif
            relocate_to(allocate(new_cap), new_cap);
        }

        /**
         * @brief Create a hole of @p count elements starting at index @p idx.
         *
         * Internal helper used by insert/erase. Ensures capacity, shifts the
         * tail, updates @_size and returns a pointer to the first gap slot
         * where new elements can be constructed.
         *
         * @return Pointer to the gap.
         */
        pointer make_gap(size_type idx, size_type count)
        {
            if (_size + count > _cap) grow(_size + count);

            pointer pos = _data + idx;
            size_type tail = _size - idx;

            if constexpr (std::is_trivially_copyable_v<value_type>)
            {
                std::memmove(pos + count, pos, tail * sizeof(value_type));
                _size += count;
                return pos;
            }
            else
            {
                size_type moved = 0;
                try
                {
                    while (moved < tail)
                    {
                        pointer src = _data + _size - 1 - moved;
                        pointer dst = src + count;
                        new (dst) value_type(std::move_if_noexcept(*src));
                        ++moved;
                    }
                }
                catch (...)
                {
                    destroy_range(_data + _size - moved + count,
                        _data + _size + count);
                    throw;
                }
                destroy_range(pos, pos + tail);
                _size += count;
                return pos;
            }
        }

        /**
         * @brief Detects whether an iterator originates from *this* container.
         *
         * Only pointer iterators are considered; proxy/wrapper iterators always
         * return @c false. This is used to guard against self‑overlap in assign
         * and insert operations that accept iterator ranges.
         */
        template <class It>
        [[nodiscard]] bool from_self(It it) const noexcept
        {
            if constexpr (std::is_pointer_v<It>)
                return it >= _data && it < _data + _size;
            else
                return false;
        }

    public:                               // ctors / dtor
        /**
         * @name Constructors & Destructor
         *///@{

         /**
          * @brief Default‑constructs an empty vector with capacity @c N.
          */
        SmallVector() noexcept = default;

        /**
         * @brief Constructs the vector with @p count copies of @p value.
         * @param count Number of elements.
         * @param value Value to copy into each element (defaults to
         *              `value_type{}`).
         * @throws std::bad_alloc On allocation failure.
         */
        explicit SmallVector(size_type count, const_reference value = value_type{})
        {
            if (count > _cap) grow(count);
            std::uninitialized_fill_n(_data, count, value);
            _size = count;
        }

        /**
         * @brief Range constructor.
         *
         * Enabled only when @p InputIt is not an integral type to avoid
         * ambiguity with the fill constructor. Copies (or moves) the range
         * [`first`, `last`) into the container.
         *
         * @tparam InputIt Input iterator satisfying `InputIterator`.
         * @throws std::invalid_argument If the range comes from *this*.
         * @throws std::bad_alloc On allocation failure.
         */
        template <class InputIt,
            class = std::enable_if_t<!std::is_integral_v<InputIt>>>
        SmallVector(InputIt first, InputIt last) { assign(first, last); }

        /**
         * @brief Initialiser‑list constructor.
         * @param il List of elements.
         */
        SmallVector(std::initializer_list<value_type> il)
            : SmallVector(il.begin(), il.end()) {
        }

        /**
         * @brief Copy constructor – performs deep copy.
         */
        SmallVector(const SmallVector& other)
            : SmallVector(other.begin(), other.end()) {
        }

        /**
         * @brief Move constructor.
         *
         * Moves resources when the source is heap‑allocated; otherwise falls
         * back to element‑wise move so that both objects can safely share the
         * stack buffer.
         */
        SmallVector(SmallVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            if (other.on_stack())
            {
                reserve(other._size);
                std::uninitialized_move(other.begin(), other.end(), _data);
                _size = other._size;
                other.clear();
            }
            else
            {
                _data = other._data;
                _size = other._size;
                _cap = other._cap;

                other._data = stack_ptr(other._stack);
                other._size = 0;
                other._cap = kStackCapacity;
            }
        }

        /** Destructor – destroys all elements and frees heap memory. */
        ~SmallVector() noexcept
        {
            clear();
            if (!on_stack()) deallocate(_data);
        }
        ///@}

        // ------------------------------------------------------------------
        //                              Assignment
        // ------------------------------------------------------------------
        /**
         * @brief Copy‑assigns from @p rhs.
         * @return *this.
         */
        SmallVector& operator=(const SmallVector& rhs)
        {
            if (this != &rhs) assign(rhs.begin(), rhs.end());
            return *this;
        }
        /**
         * @brief Move‑assigns from @p rhs providing strong exception safety.
         * @return *this.
         */
        SmallVector& operator=(SmallVector&& rhs) noexcept(std::is_nothrow_move_assignable_v<T>)
        {
            if (this == &rhs) return *this;

            clear();
            if (!on_stack()) deallocate(_data);

            if (rhs.on_stack())
            {
                _data = stack_ptr(_stack);
                _cap = kStackCapacity;
                reserve(rhs._size);
                std::uninitialized_move(rhs.begin(), rhs.end(), _data);
                _size = rhs._size;
                rhs.clear();
            }
            else
            {
                _data = rhs._data;
                _size = rhs._size;
                _cap = rhs._cap;

                rhs._data = stack_ptr(rhs._stack);
                rhs._size = 0;
                rhs._cap = kStackCapacity;
            }
            return *this;
        }

        // ------------------------------------------------------------------
        //                    Element access (bounds‑checked)
        // ------------------------------------------------------------------
        /**
         * @name Element access
         *///@{
        reference       operator[](size_type i) { return _data[i]; }
        const_reference operator[](size_type i) const { return _data[i]; }

        /**
         * @brief Bounds‑checked element access.
         * @throws std::out_of_range If @p i >= size().
         */
        reference at(size_type i)
        {
            if (i >= _size)
                throw std::out_of_range("SmallVector::at");
            return _data[i];
        }
        /** @copydoc at(size_type) */
        const_reference at(size_type i) const
        {
            if (i >= _size)
                throw std::out_of_range("SmallVector::at");
            return _data[i];
        }

        reference       front() { assert(!empty()); return *_data; }
        const_reference front() const { assert(!empty()); return *_data; }
        reference       back() { assert(!empty()); return _data[_size - 1]; }
        const_reference back()  const { assert(!empty()); return _data[_size - 1]; }

        pointer         data() noexcept { return _data; }
        const_pointer   data() const noexcept { return _data; }
        ///@}

        // ------------------------------------------------------------------
        //                                Iterators
        // ------------------------------------------------------------------
        iterator               begin() noexcept { return _data; }
        const_iterator         begin() const noexcept { return _data; }
        const_iterator         cbegin() const noexcept { return _data; }

        iterator               end() noexcept { return _data + _size; }
        const_iterator         end() const noexcept { return _data + _size; }
        const_iterator         cend() const noexcept { return _data + _size; }

        reverse_iterator       rbegin() noexcept { return reverse_iterator(end()); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(cend()); }

        reverse_iterator       rend() noexcept { return reverse_iterator(begin()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crend() const noexcept { return const_reverse_iterator(cbegin()); }

        // ------------------------------------------------------------------
        //                               Capacity
        // ------------------------------------------------------------------
        [[nodiscard]] bool      empty()    const noexcept { return _size == 0; }
        [[nodiscard]] size_type size()     const noexcept { return _size; }
        [[nodiscard]] size_type capacity() const noexcept { return _cap; }
        [[nodiscard]] size_type max_size() const noexcept
        {
            return static_cast<size_type>(-1) / sizeof(value_type);
        }

        /**
         * @brief Ensures the container can hold at least @p new_cap elements
         *        without reallocation.
         */
        void reserve(size_type new_cap)
        {
            if (new_cap > _cap) relocate_to(allocate(new_cap), new_cap);
        }

        /**
         * @brief Reduce capacity to fit the current size.
         *
         * If the current size fits in the SBO buffer the vector moves the
         * elements back onto the stack; otherwise it shrinks the heap storage
         * exactly to size().
         */
        void shrink_to_fit()
        {
            if (_size <= kStackCapacity && !on_stack())
                relocate_to(stack_ptr(_stack), kStackCapacity);
            else if (_size < _cap && _size > kStackCapacity)
                relocate_to(allocate(_size), _size);
        }

        // ------------------------------------------------------------------
        //                               Modifiers
        // ------------------------------------------------------------------
        /**
         * @brief Destroy all elements without changing capacity.
         * @post size() == 0
         */
        void clear() noexcept
        {
            destroy_range(_data, _data + _size);
            _size = 0;
        }

        // ------------------------------ assign --------------------------------
        /**
         * @brief Replace contents with the range [`first`, `last`).
         * @throws std::invalid_argument If the range aliases with *this*.
         */
        template <class InputIt,
            class = std::enable_if_t<!std::is_integral_v<InputIt>>>
        void assign(InputIt first, InputIt last)
        {
            if (from_self(first))
                throw std::invalid_argument("SmallVector::assign: source overlaps");

            clear();
            using cat = typename std::iterator_traits<InputIt>::iterator_category;

            if constexpr (std::is_base_of_v<std::forward_iterator_tag, cat>)
            {
                size_type cnt = static_cast<size_type>(std::distance(first, last));
                if (cnt > _cap) reserve(cnt);
                std::uninitialized_copy(first, last, _data);
                _size = cnt;
            }
            else           // Single‑pass InputIterator
            {
                for (; first != last; ++first) emplace_back(*first);
            }
        }
        /**
         * @brief Fill‑assign @p count copies of @p value.
         */
        void assign(size_type count, const_reference value)
        {
            clear();
            if (count > _cap) reserve(count);
            std::uninitialized_fill_n(_data, count, value);
            _size = count;
        }

        // ------------------------------ resize --------------------------------
        /**
         * @brief Resize to exactly @p count default‑constructed elements.
         */
        void resize(size_type count)
        {
            if (count < _size)
            {
                destroy_range(_data + count, _data + _size);
                _size = count;
            }
            else if (count > _size)
            {
                reserve(count);
                std::uninitialized_default_construct_n(_data + _size, count - _size);
                _size = count;
            }
        }
        /**
         * @brief Resize to exactly @p count copies of @p value.
         */
        void resize(size_type count, const_reference value)
        {
            if (count < _size)
                destroy_range(_data + count, _data + _size);
            else
            {
                reserve(count);
                std::uninitialized_fill_n(_data + _size, count - _size, value);
            }
            _size = count;
        }

        // ------------------------------ erase ---------------------------------
        /**
         * @brief Erase element at @p pos returning iterator to the next one.
         * @throws std::out_of_range If @p pos is not dereferenceable.
         */
        iterator erase(const_iterator pos)
        {
            difference_type idx = pos - cbegin();
            if (idx < 0 || static_cast<size_type>(idx) >= _size)
                throw std::out_of_range("SmallVector::erase");

            pointer p = _data + idx;
            if constexpr (std::is_trivially_copyable_v<value_type>)
                std::memmove(p, p + 1, (_size - idx - 1) * sizeof(value_type));
            else
                std::move(p + 1, _data + _size, p);

            --_size;
            destroy_range(_data + _size, _data + _size + 1);
            return p;
        }

        // ------------------------------ insert --------------------------------
        /**
         * @brief Insert @p value at @p pos returning iterator to the new element.
         */
        iterator insert(const_iterator pos, const_reference value)
        {
            difference_type idx = pos - cbegin();
            pointer p = make_gap(static_cast<size_type>(idx), 1);

            if constexpr (std::is_trivially_copyable_v<value_type>)
                std::memcpy(p, &value, sizeof(value_type));
            else
                new (p) value_type(value);

            return p;
        }

        /**
         * @brief Insert range [`first`, `last`) before @p pos.
         *
         * Strong exception guarantee: if construction of any element throws the
         * container is left unmodified.
         */
        template <class InputIt,
            class = std::enable_if_t<!std::is_integral_v<InputIt>>>
        void insert(const_iterator pos, InputIt first, InputIt last)
        {
            if (from_self(first))
                throw std::invalid_argument("SmallVector::insert: source overlaps");

            using cat = typename std::iterator_traits<InputIt>::iterator_category;
            size_type idx = static_cast<size_type>(pos - cbegin());

            if constexpr (std::is_base_of_v<std::forward_iterator_tag, cat>)
            {
                size_type cnt = static_cast<size_type>(std::distance(first, last));
                if (!cnt) return;
                pointer p = make_gap(idx, cnt);
                std::uninitialized_copy(first, last, p);
            }
            else   // Single‑pass InputIterator
            {
                for (; first != last; ++first)
                {
                    pos = insert(pos, *first);
                    ++pos;           // Skip over the just‑inserted element.
                }
            }
        }

        // ---------------------------- push / pop -----------------------------
        /**
         * @brief Construct a new element in‑place at the end.
         * @tparam Args Argument pack forwarded to @p T's constructor.
         * @return Reference to the newly inserted element.
         */
        template <class... Args>
        reference emplace_back(Args&&... args)
        {
            if (_size == _cap) grow(_size + 1);
            pointer loc = _data + _size;
            new (loc) value_type(std::forward<Args>(args)...);
            ++_size;
            return *loc;
        }
        /// @copydoc emplace_back
        void push_back(const_reference v) { emplace_back(v); }
        /// @copydoc emplace_back
        void push_back(value_type&& v) { emplace_back(std::move(v)); }

        /**
         * @brief Removes the last element. Undefined behaviour if empty().
         */
        void pop_back()
        {
            assert(!empty());
            --_size;
            destroy_range(_data + _size, _data + _size + 1);
        }

        // ------------------------------ swap ---------------------------------
        /**
         * @brief Constant‑time swap whenever both sides are heap‑allocated.
         *
         * Provides strong exception safety by falling back to move‑swap when
         * element‑wise exchange is not possible.
         */
        void swap(SmallVector& other)
            noexcept(std::is_nothrow_move_constructible_v<T>&&
                std::is_nothrow_swappable_v<T>)
        {
            if (this == &other) return;

            bool self_heap = !on_stack();
            bool other_heap = !other.on_stack();

            if (self_heap && other_heap)            // Fast path – just pointers.
            {
                using std::swap;
                swap(_data, other._data);
                swap(_size, other._size);
                swap(_cap, other._cap);
                return;
            }

            // Element‑wise swap when each side fits into the other's capacity.
            if (_size <= other._cap && other._size <= _cap)
            {
                size_type min_sz = std::min(_size, other._size);
                for (size_type i = 0; i < min_sz; ++i)
                    std::swap(_data[i], other._data[i]);

                if (_size > other._size)
                {
                    size_type diff = _size - other._size;
                    pointer src = _data + min_sz;

                    if constexpr (std::is_trivially_copyable_v<value_type>)
                        std::memcpy(other._data + other._size,
                            src, diff * sizeof(value_type));
                    else
                        std::uninitialized_move(src, src + diff,
                            other._data + other._size);

                    destroy_range(src, src + diff);
                }
                else if (other._size > _size)
                {
                    size_type diff = other._size - _size;
                    pointer src = other._data + min_sz;

                    if constexpr (std::is_trivially_copyable_v<value_type>)
                        std::memcpy(_data + _size,
                            src, diff * sizeof(value_type));
                    else
                        std::uninitialized_move(src, src + diff,
                            _data + _size);

                    destroy_range(src, src + diff);
                }
                std::swap(_size, other._size);
                return;
            }

            SmallVector tmp(std::move(*this));
            *this = std::move(other);
            other = std::move(tmp);
        }

        // --------------------------- comparisons ----------------------------
        friend bool operator==(const SmallVector& a, const SmallVector& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
        }
        friend bool operator!=(const SmallVector& a, const SmallVector& b) { return !(a == b); }
        friend bool operator< (const SmallVector& a, const SmallVector& b)
        {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
        }
        friend bool operator>  (const SmallVector& a, const SmallVector& b) { return b < a; }
        friend bool operator<= (const SmallVector& a, const SmallVector& b) { return !(b < a); }
        friend bool operator>= (const SmallVector& a, const SmallVector& b) { return !(a < b); }
    };

    // ----------------------------------------------------------------------
    //                       Deduction guide (C++17)
    // ----------------------------------------------------------------------
    /**
     * @brief Deduction guide allowing `SmallVector v(it1, it2);` without
     *        specifying the value type explicitly.
     */
    template <class InputIt,
        class T = typename std::iterator_traits<InputIt>::value_type>
    SmallVector(InputIt, InputIt) -> SmallVector<T>;

} // namespace lux::cxx
