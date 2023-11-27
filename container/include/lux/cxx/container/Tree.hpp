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
        [[nodiscard]] size_t depth()
        {
            if (_parent == nullptr)
            {
                return 0;
            }
            return 1 + _parent->depth();
        }

        template<class U>
        void set(U&& value)
        {
            _value = std::forward<U>(value);
        }

        constexpr T& get()
        {
            return _value;
        }

        constexpr const T& get() const
        {
            return _value;
        }

        TTreeNodeBase()
            : _parent(nullptr) {}

        template<class U>
        explicit TTreeNodeBase(U&& value)
            : _parent(nullptr), _value(std::forward<U>(value)) {}

    protected:
        TTreeNodeBase*  _parent;
        T               _value;
    };

    template<class T, size_t N, bool isStatic = true>
    class TreeNode : public TTreeNodeBase<T>
    {
    public:
        using TTreeNodeBase<T>::TTreeNodeBase;

        template<size_t I>
        void TSetChild(TreeNode* child)
        {
            static_assert(I < N, "Index out of bound");
            _children[I] = child;
            child->_parent = this;
        }

        TreeNode* getChild(size_t index)
        {
            if (index >= N)
                return nullptr;

            return _children[index];
        }

        const TreeNode* getChild(size_t index) const
        {
            if (index >= N)
                return nullptr;

            return _children[index];
        }

        template<size_t I>
        TreeNode* TGetChild()
        {
            if (I >= N)
                return nullptr;

            return _children[I];
        }

        template<size_t I>
        const TreeNode* TGetChild() const
        {
            if (I >= N)
                return nullptr;

            return _children[I];
        }

        [[nodiscard]] bool setChild(size_t index, TreeNode* child)
        {
            if (index >= N)
                return false;
            _children[index] = child;
            child->_parent = this;
            return true;
        }
    private:
        std::array<TreeNode*, N> _children;
    };

    template<class T, size_t N>
    class TreeNode<T, N, false> : public TTreeNodeBase<T>
    {
    public:
        using TTreeNodeBase<T>::TTreeNodeBase;

        [[nodiscard]] bool addChild(TreeNode* child)
        {
            if (_children.size() >= N)
                return false;
            child->_parent = this;
            _children.push_back(child);
            return true;
        }

        TreeNode* getChild(size_t index)
        {
            if (index >= _children.size())
                return nullptr;

            return _children[index];
        }

        const TreeNode* getChild(size_t index) const
        {
            if (index >= _children.size())
                return nullptr;

            return _children[index];
        }

    private:
        std::vector<TreeNode*> _children;
    };
}
