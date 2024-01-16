#pragma once
#include <array>
#include <vector>
#include <cstddef>

namespace lux::cxx
{
    template<class T>
    class TTreeNodeBase
    {
    public:
        [[nodiscard]] size_t depth() const
        {
            if (_parent == nullptr)
            {
                return 0;
            }
            return 1 + _parent->depth();
        }

        bool isRoot() const
        {
            return _parent == nullptr;
        }

        template<class U>
        void set(U&& value)
        {
            _value = std::forward<U>(value);
        }

        constexpr inline T& value()
        {
            return _value;
        }

        constexpr inline const T& value() const
        {
            return _value;
        }

        constexpr TTreeNodeBase()
            : _parent(nullptr) {}

        template<class U> constexpr
        explicit TTreeNodeBase(U&& value)
            : _parent(nullptr), _value(std::forward<U>(value)) {}

    protected:
        TTreeNodeBase* _parent;
        T              _value;
    };

    template<class T, size_t N, bool isStatic = true>
    class TTreeNode : public TTreeNodeBase<T>
    {
    public:
        using TTreeNodeBase<T>::TTreeNodeBase;

        template<size_t I>
        void TSetChild(TTreeNode* child)
        {
            static_assert(I < N, "Index out of bound");
            _children[I] = child;
            child->_parent = this;
        }

        TTreeNode* getChild(size_t index)
        {
            if (index >= N)
                return nullptr;

            return _children[index];
        }

        const TTreeNode* getChild(size_t index) const
        {
            if (index >= N)
                return nullptr;

            return _children[index];
        }

        template<size_t I>
        TTreeNode* TGetChild()
        {
            if (I >= N)
                return nullptr;

            return _children[I];
        }

        template<size_t I>
        const TTreeNode* TGetChild() const
        {
            if (I >= N)
                return nullptr;

            return _children[I];
        }

        [[nodiscard]] bool setChild(size_t index, TTreeNode* child)
        {
            if (index >= N)
                return false;
            _children[index] = child;
            child->_parent = this;
            return true;
        }

    private:
        std::array<TTreeNode*, N> _children;
    };

    template<class T, class Alloc = std::allocator<T>>
    class DynamicTreeNode : public TTreeNodeBase<T>
    {
    public:
        using TTreeNodeBase<T>::TTreeNodeBase;

        [[nodiscard]] bool addChild(DynamicTreeNode* child)
        {
            if (child == nullptr)
                return false;

            child->_parent = this;
            _children.push_back(child);
            return true;
        }

        constexpr inline DynamicTreeNode* getChild(size_t index)
        {
            return index >= _children.size() ?
                nullptr : _children[index];
        }

        constexpr inline const DynamicTreeNode* getChild(size_t index) const
        {
            return index >= _children.size() ?
                nullptr : _children[index];
        }

    private:
        std::vector<DynamicTreeNode*, Alloc> _children;
    };

    template<typename T> using BinaryTree   = TTreeNode<T, 2, true>;
    template<typename T> using Octree       = TTreeNode<T, 8, true>;
}
