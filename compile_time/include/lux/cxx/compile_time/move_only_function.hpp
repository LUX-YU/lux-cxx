#pragma once
#include <type_traits>
#include <utility>
#include <cstring>
#include <cassert>
#include <new>
#include <cstdlib>

namespace lux::cxx
{
    /**
     * @brief move_only_function<R(Args...)> - Similar to C++23 std::move_only_function
     *
     * - Supports move semantics only, copy is not allowed
     * - Small object optimization (SBO): If the callable object is small enough and can be moved without exceptions, the heap is not used
     * - For larger objects, dynamic allocation is used
     */
    template <class>
    class move_only_function; // Undefined

    //-------------------------------------------------------------
    // Specialization: Supports return type R and arguments (Args...)
    //-------------------------------------------------------------
    template <class R, class... Args>
    class move_only_function<R(Args...)>
    {
    public:
        /**
         * @brief Default constructor: Creates an empty move_only_function
         */
        move_only_function() noexcept
            : _manager(nullptr)
        {
        }

        /**
         * @brief Destructor: Destroys the currently held callable object (if any)
         */
        ~move_only_function()
        {
            _destroy();
        }

        // Copy is disabled
        move_only_function(const move_only_function &) = delete;
        move_only_function &operator=(const move_only_function &) = delete;

        // Move is allowed
        move_only_function(move_only_function &&other) noexcept
            : _manager(nullptr)
        {
            _move_assign_from(std::move(other));
        }

        move_only_function &operator=(move_only_function&& other) noexcept
        {
            if (this != &other)
            {
                _destroy();
                _move_assign_from(std::move(other));
            }
            return *this;
        }

        /**
         * @brief Constructs from a callable object
         *
         * @tparam F  Any callable type (lambda, functor, function pointer, etc.)
         *            Must satisfy invoke<F, Args...> -> R
         */
        template <class F, class = std::enable_if_t<
                std::is_invocable_r_v<R, F&, Args...>>>
        move_only_function(F &&f)
            : _manager(nullptr)
        {
            _emplace(std::forward<F>(f));
        }

        /**
         * @brief Callable operator
         *
         * Invokes the held callable object with the given arguments.
         * Behavior is undefined if the object is empty; an assert is used to catch this.
         */
        R operator()(Args... args)
        {
            assert(_manager && "move_only_function is empty!");
            return _manager->invoke(_get_storage(), std::forward<Args>(args)...);
        }

        R operator()(Args... args) const
        {
            assert(_manager && "move_only_function is empty!");
            return _manager->invoke(_get_storage(), std::forward<Args>(args)...);
        }

        /**
         * @brief Resets the current object to be empty
         */
        void reset() noexcept
        {
            _destroy();
        }

        /**
         * @brief Conversion to bool to check if the object contains a callable
         */
        explicit operator bool() const noexcept
        {
            return (_manager != nullptr);
        }

    private:
        // Threshold for small object optimization
        static constexpr std::size_t SBO_SIZE  = 32; // Can be adjusted
        static constexpr std::size_t SBO_ALIGN = alignof(std::max_align_t);

        // ---------------------------------------------------------
        // Manager Table: Stores operation functions for the target object (destroy/move/invoke)
        // ---------------------------------------------------------
        struct manager
        {
            // Destroy
            void (*destroy)(void *) noexcept;
            // Move from src to dst and clean up src
            void (*move)(void *dst, void *src) noexcept;
            // Invoke
            R (*invoke)(void *storage, Args &&...args);
        };

        // ---------------------------------------------------------
        // Storage Area: Inline buffer for small object optimization or dynamically allocated pointer
        // ---------------------------------------------------------
        union storage_t
        {
            void *ptr;                              // Pointer for dynamically allocated objects
            alignas(SBO_ALIGN) unsigned char sbo[SBO_SIZE]; // Inline buffer for small objects
        } _storage{};

        // Pointer to the current object's manager. If null, no object is present.
        const manager *_manager;

    private:
        // Get the raw storage address (read-only)
        const void *_get_storage() const noexcept
        {
            return (_manager && _is_heap_alloc())
                ? static_cast<const void *>(&_storage.ptr)
                : static_cast<const void *>(&_storage.sbo);
        }

        // Get the writable storage address
        void *_get_storage() noexcept
        {
            return const_cast<void *>(static_cast<const move_only_function *>(this)->_get_storage());
        }

        // Check if the current object is dynamically allocated
        bool _is_heap_alloc() const noexcept
        {
            return _manager && _manager->move == &_move_heap;
        }

        // Destroy the current object (if any)
        void _destroy() noexcept
        {
            if (_manager)
            {
                _manager->destroy(_get_storage());
                _manager = nullptr;
            }
        }

        // Move from `other` to `*this`
        void _move_assign_from(move_only_function&& other) noexcept
        {
            if (!other._manager)
            {
                _manager = nullptr; // `other` is empty
            }
            else
            {
                _manager = other._manager;
                _manager->move(_get_storage(), other._get_storage());
                other._manager = nullptr;
            }
        }

        // ---------------------------------------------------------
        // Management functions for dynamically allocated objects
        // ---------------------------------------------------------
        template<class T>
        static void _destroy_heap(void *p) noexcept
        {
            void** real_ptr = static_cast<void**>(p);
            delete static_cast<T*>(*real_ptr); // Pointer `p` is to a dynamically allocated object
        }

        static void _move_heap(void *dst, void *src) noexcept
        {
            *static_cast<void **>(dst) = *static_cast<void **>(src);
            *static_cast<void **>(src) = nullptr;
        }

        // ---------------------------------------------------------
        // Management functions for small object optimization
        // ---------------------------------------------------------
        template <class T>
        static void _destroy_sbo(void *s) noexcept
        {
            reinterpret_cast<T *>(s)->~T(); // Placement-delete
        }

        template <class T>
        static void _move_sbo(void *dst, void *src) noexcept
        {
            T *s_ptr = reinterpret_cast<T *>(src);
            new (dst) T(std::move(*s_ptr)); // Move-construct T at `dst`
            s_ptr->~T(); // Destroy `src`
        }

        // ---------------------------------------------------------
        // Invoke functions - required for both small and large objects
        // ---------------------------------------------------------
        template <class T>
        static R _invoke_sbo(void* s, Args&&... args) 
        {
            auto ptr = reinterpret_cast<T*>(s);
            if constexpr (std::is_void_v<R>) 
            {
                std::invoke(*ptr, static_cast<Args&&>(args)...);
                return; 
            } 
            else 
            {
                return std::invoke(*ptr, static_cast<Args&&>(args)...);
            }
        }

        template <class T>
        static R _invoke_heap(void* p, Args&&... args) 
        {
            void** real_ptr = static_cast<void**>(p);
            auto real_obj = reinterpret_cast<T*>(*real_ptr);
            if constexpr (std::is_void_v<R>) 
            {
                std::invoke(*real_obj, static_cast<Args&&>(args)...);
                return;
            } 
            else 
            {
                return std::invoke(*real_obj, static_cast<Args&&>(args)...);
            }
        }

        // ---------------------------------------------------------
        // General `emplace` - Determines whether to use SBO based on the size, alignment, and nothrow move constructibility of T
        // ---------------------------------------------------------
        template <class F>
        void _emplace(F&& f)
        {
            using decayF = std::decay_t<F>;

            constexpr bool nothrow_mv = std::is_nothrow_move_constructible_v<decayF>;
            constexpr bool fits_sbo = (sizeof(decayF) <= SBO_SIZE) && (alignof(decayF) <= SBO_ALIGN) && nothrow_mv;

            if constexpr(fits_sbo)
            {
                // Small object optimization
                new (&_storage.sbo) decayF(std::forward<F>(f)); // Placement-new
                static const manager s_manager = {
                    &_destroy_sbo<decayF>,
                    &_move_sbo<decayF>,
                    &_invoke_sbo<decayF>
                };
                _manager = &s_manager;
            }
            else
            {
                // Dynamic allocation
                char *mem = new char[sizeof(decayF)];
                new (mem) decayF(std::forward<F>(f)); // Placement-new on the allocated memory

                _storage.ptr = mem;
                static const manager s_manager = {
                    &_destroy_heap<decayF>,
                    &_move_heap,
                    &_invoke_heap<decayF>
                };
                _manager = &s_manager;
            }
        }
    };

} // end namespace lux
