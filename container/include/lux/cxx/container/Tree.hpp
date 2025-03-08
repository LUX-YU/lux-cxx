#pragma once
/*
 * Copyright (c) 2025 Chenhui Yu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <array>
#include <vector>
#include <cstddef>
#include <memory>
#include <stdexcept>

namespace lux::cxx
{
    /**
     * @brief CRTP-based tree node base class.
     *
     * This class leverages the Curiously Recurring Template Pattern (CRTP) so that
     * derived types (e.g., StaticTreeNode or DynamicTreeNode) can inherit from this
     * base and make use of its functionality. Each node stores:
     *   - A value of type T.
     *   - A pointer to its parent node (using a raw pointer).
     *
     * Child nodes are managed via std::unique_ptr in the derived classes. Since the
     * parent pointer is raw, care must be taken to ensure it is correctly set and
     * reset when transferring child ownership or removing children.
     *
     * @tparam Derived The derived class type (CRTP).
     * @tparam T       The data type stored in each node.
     */
    template<class Derived, class T>
    class TreeNodeBase
    {
    public:
        using value_type = T;

        /// Virtual destructor to allow polymorphic deletion.
        virtual ~TreeNodeBase() = default;

        // ---------------------- Constructors / Assignment ---------------------- //

        /**
         * @brief Default constructor. Sets the parent pointer to nullptr.
         */
        TreeNodeBase()
            : parent_(nullptr)
        {
        }

        /**
         * @brief Constructor that accepts a value of type U and forwards it to T.
         * @tparam U Type convertible to T.
         * @param val The value to be stored in this node.
         */
        template<class U>
        explicit TreeNodeBase(U&& val)
            : value_(std::forward<U>(val)), parent_(nullptr)
        {
        }

        // Copy construction/assignment is deleted to avoid ambiguous ownership.
        TreeNodeBase(const TreeNodeBase&) = delete;
        TreeNodeBase& operator=(const TreeNodeBase&) = delete;

        // Move construction/assignment is also deleted here for simplicity.
        TreeNodeBase(TreeNodeBase&&) = delete;
        TreeNodeBase& operator=(TreeNodeBase&&) = delete;

        // ---------------------- Public Methods ---------------------- //

        /**
         * @brief Provides mutable access to the node's stored value.
         * @return A reference to the stored value of type T.
         */
        T& value() noexcept
        {
            return value_;
        }

        /**
         * @brief Provides read-only access to the node's stored value.
         * @return A const reference to the stored value of type T.
         */
        const T& value() const noexcept
        {
            return value_;
        }

        /**
         * @brief Checks if this node is a root node (i.e., has no parent).
         * @return True if this node is root, otherwise false.
         */
        bool isRoot() const noexcept
        {
            return parent_ == nullptr;
        }

        /**
         * @brief Calculates the depth of this node (distance to the root).
         *
         * Iteratively climbs up the parent pointers to count how many
         * levels exist between this node and the root.
         *
         * @return The depth of this node as a std::size_t.
         */
        std::size_t depth() const noexcept
        {
            std::size_t d = 0;
            auto p = parent_;
            while (p != nullptr)
            {
                p = p->parent_;
                ++d;
            }
            return d;
        }

    protected:
        /// CRTP: Derived is the child class type, e.g., StaticTreeNode<T,N> or DynamicTreeNode<T>.
        Derived* parent_;
        T        value_;
    };

    /**
     * @brief A static N-ary tree node with a fixed number of children.
     *
     * This node manages its children via std::unique_ptr, and the maximum
     * number of children is fixed at compile time (N). Each child can be
     * assigned or removed using specific slots.
     *
     * @tparam T The data type stored in each node.
     * @tparam N The fixed number of child pointers this node can hold.
     */
    template<class T, std::size_t N>
    class StaticTreeNode
        : public TreeNodeBase<StaticTreeNode<T, N>, T>
    {
    public:
        using base_type = TreeNodeBase<StaticTreeNode<T, N>, T>;
        using self_type = StaticTreeNode<T, N>;

        using value_type = typename base_type::value_type;

        // Allow the base class (CRTP) to access private members if needed.
        friend base_type;

        // ---------------------- Constructors / Destructor ---------------------- //

        /**
         * @brief Default constructor. Initializes all child pointers to nullptr.
         */
        StaticTreeNode()
            : base_type()
        {
            for (auto& elem : children_) {
                elem = nullptr;
            }
        }

        /**
         * @brief Constructs a node by forwarding a value of type U to T.
         * @tparam U A type convertible to T.
         * @param val The value used to initialize this node.
         */
        template<class U>
        explicit StaticTreeNode(U&& val)
            : base_type(std::forward<U>(val))
        {
            for (auto& elem : children_) {
                elem = nullptr;
            }
        }

        /// Virtual destructor; inherited from base.
        ~StaticTreeNode() override = default;

        // ---------------------- Child Management ---------------------- //

        /**
         * @brief Sets the child at the specified index, taking ownership via std::unique_ptr.
         *
         * @param index The slot where the child will be placed (0 <= index < N).
         * @param child A unique_ptr to the new child.
         * @return True if successful; false if the index is out of bounds.
         */
        bool setChild(std::size_t index, std::unique_ptr<self_type> child)
        {
            if (index >= N)
                return false;

            if (child) {
                // Set the parent pointer of the new child to this node
                child->parent_ = this;
            }
            // Transfer ownership
            children_[index] = std::move(child);
            return true;
        }

        /**
         * @brief Creates and initializes a child in-place using the given constructor arguments.
         *
         * This function constructs a new child with the provided arguments
         * and places it in the specified index.
         *
         * @tparam Args Variadic template types for child construction.
         * @param index The slot where the child will be placed (0 <= index < N).
         * @param val   Constructor parameters for the child's value.
         * @return True if successful; false if the index is out of bounds.
         */
        template<typename... Args>
        bool emplaceChild(std::size_t index, Args&&... val)
        {
            if (index >= N)
                return false;
            children_[index] = std::make_unique<self_type>(std::forward<Args>(val)...);
            children_[index]->parent_ = this;
            return true;
        }

        /**
         * @brief Retrieves a const pointer to the child at the given index.
         * @param index The index of the child.
         * @return A const pointer to the child, or nullptr if out of range or empty.
         */
        const self_type* getChild(std::size_t index) const noexcept
        {
            if (index >= N)
                return nullptr;
            return children_[index].get();
        }

        /**
         * @brief Retrieves a mutable pointer to the child at the given index.
         * @param index The index of the child.
         * @return A pointer to the child, or nullptr if out of range or empty.
         */
        self_type* getChild(std::size_t index) noexcept
        {
            if (index >= N)
                return nullptr;
            return children_[index].get();
        }

        /**
         * @brief Removes a child at the specified index and returns it as a std::unique_ptr.
         *
         * This effectively disconnects the child from the tree. The child’s parent
         * pointer will be set to nullptr before returning.
         *
         * @param index The index of the child to be removed.
         * @return A std::unique_ptr to the removed child, or nullptr if out of range.
         */
        std::unique_ptr<self_type> removeChild(std::size_t index)
        {
            if (index >= N)
                return nullptr;

            // Reset the child's parent pointer
            if (children_[index]) {
                children_[index]->parent_ = nullptr;
            }
            auto ptr = std::move(children_[index]);
            return ptr;
        }

        /**
         * @brief Returns the fixed capacity of children.
         * @return A constexpr std::size_t equal to N.
         */
        static constexpr std::size_t childCapacity() noexcept { return N; }

    private:
        /// An array of unique_ptrs to child nodes, with size N.
        std::array<std::unique_ptr<self_type>, N> children_;
    };

    /**
     * @brief A dynamic multi-branch (N-ary) tree node with variable number of children.
     *
     * In contrast to StaticTreeNode, this node allows a dynamic number of children
     * by storing them in a std::vector<std::unique_ptr<DynamicTreeNode<T>>>. Children
     * can be added, retrieved, or removed at any index within the vector.
     *
     * @tparam T The data type stored in each node.
     */
    template<class T>
    class DynamicTreeNode
        : public TreeNodeBase<DynamicTreeNode<T>, T>
    {
    public:
        using base_type = TreeNodeBase<DynamicTreeNode<T>, T>;
        using self_type = DynamicTreeNode<T>;

        using value_type = typename base_type::value_type;

        friend base_type;

        // ---------------------- Constructors / Destructor ---------------------- //

        /**
         * @brief Default constructor. The parent pointer is set to nullptr.
         */
        DynamicTreeNode()
            : base_type()
        {
        }

        /**
         * @brief Constructs a node by forwarding a value of type U to T.
         * @tparam U A type convertible to T.
         * @param val The value used to initialize this node.
         */
        template<class U>
        explicit DynamicTreeNode(U&& val)
            : base_type(std::forward<U>(val))
        {
        }

        /// Virtual destructor; inherited from base.
        ~DynamicTreeNode() override = default;

        // ---------------------- Child Management ---------------------- //

        /**
         * @brief Adds a child node, taking ownership via std::unique_ptr.
         *
         * If the provided unique_ptr is null, no child is added.
         *
         * @param child A unique_ptr pointing to the new child.
         * @return A raw pointer to the newly added child, or nullptr if child was null.
         */
        DynamicTreeNode<T>* addChild(std::unique_ptr<self_type> child)
        {
            if (!child)
                return nullptr;

            child->parent_ = this;
            auto ptr = child.get();
            children_.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Retrieves a mutable pointer to the child at the specified index.
         * @param index The index of the child in the children vector.
         * @return A pointer to the child, or nullptr if out of range.
         */
        self_type* getChild(std::size_t index) noexcept
        {
            if (index >= children_.size())
                return nullptr;
            return children_[index].get();
        }

        /**
         * @brief Retrieves a const pointer to the child at the specified index.
         * @param index The index of the child in the children vector.
         * @return A const pointer to the child, or nullptr if out of range.
         */
        const self_type* getChild(std::size_t index) const noexcept
        {
            if (index >= children_.size())
                return nullptr;
            return children_[index].get();
        }

        /**
         * @brief Returns the number of child nodes currently stored in this node.
         * @return The number of children as a std::size_t.
         */
        std::size_t childCount() const noexcept
        {
            return children_.size();
        }

        /**
         * @brief Removes the child at the specified index, returning it as a std::unique_ptr.
         *
         * The removed child’s parent pointer is reset to nullptr. The child is then
         * removed from the internal vector.
         *
         * @param index The index of the child in the children vector.
         * @return A std::unique_ptr to the removed child, or nullptr if out of range.
         */
        std::unique_ptr<self_type> removeChild(std::size_t index)
        {
            if (index >= children_.size())
                return nullptr;

            // Reset the child's parent pointer
            children_[index]->parent_ = nullptr;

            // Move the unique_ptr out of the vector
            std::unique_ptr<self_type> ret = std::move(children_[index]);

            // Erase the entry from the vector
            children_.erase(children_.begin() + index);

            return ret;
        }

    private:
        /// A vector of children managed by std::unique_ptr.
        std::vector<std::unique_ptr<self_type>> children_;
    };

    /**
     * @brief An indexed N-ary tree (with fixed branching factor N) stored in a single array (SoA structure).
     *
     * This approach uses a "Structure of Arrays" (SoA) layout, separating node values,
     * parent indices, and child indices into parallel arrays. Each node is identified by
     * an integer index rather than by pointer. This can improve data locality for certain
     * access patterns and reduce pointer-related overhead.
     *
     * @tparam T Data type stored in each node.
     * @tparam N Maximum number of children per node.
     */
    template<typename T, std::size_t N>
    class IndexedNaryTreeSoA
    {
    public:
        using value_type = T;
        using size_type = std::size_t;

        // ---------------------- Constructors ---------------------- //

        /**
         * @brief Constructs an empty tree with no root.
         */
        IndexedNaryTreeSoA()
            : rootIndex_(-1)
        {
        }

        /**
         * @brief Creates a root node with the specified value.
         *
         * Throws a std::runtime_error if a root already exists.
         *
         * @param val The value for the root node.
         * @return The integer index of the newly created root node.
         */
        int createRoot(const T& val)
        {
            if (rootIndex_ != -1) {
                throw std::runtime_error("Root already exists!");
            }
            int newIndex = static_cast<int>(values_.size());

            values_.push_back(val);
            parents_.push_back(-1);    // Root has no parent
            children_.push_back({});
            children_[newIndex].fill(-1);

            rootIndex_ = newIndex;
            return newIndex;
        }

        /**
         * @brief Creates a root node in-place by forwarding constructor arguments to T.
         *
         * Throws a std::runtime_error if a root already exists.
         *
         * @tparam Args Parameter pack for constructing the root node's value.
         * @param args  Parameters forwarded to T's constructor.
         * @return The index of the newly created root node.
         */
        template<typename... Args>
        int emplaceRoot(Args&&... args)
        {
            if (rootIndex_ != -1) {
                throw std::runtime_error("Root already exists!");
            }
            int newIndex = static_cast<int>(values_.size());

            values_.emplace_back(std::forward<Args>(args)...);
            parents_.push_back(-1);    // Root has no parent
            children_.push_back({});
            children_[newIndex].fill(-1);

            rootIndex_ = newIndex;
            return newIndex;
        }

        /**
         * @brief Creates a child node under the given parent in the specified slot, storing the given value.
         *
         * If the child slot is already occupied, throws a std::runtime_error.
         *
         * @param parentIdx Index of the parent node.
         * @param childSlot Which child slot to use (range: [0..N-1]).
         * @param val       The value to store in the new child node.
         * @return The index of the newly created child node.
         */
        int createChild(int parentIdx, int childSlot, const T& val)
        {
            checkIndexValid(parentIdx);
            checkChildSlot(childSlot);

            if (children_[parentIdx][childSlot] != -1) {
                throw std::runtime_error("Child slot is already occupied.");
            }

            int newIndex = static_cast<int>(values_.size());

            values_.push_back(val);
            parents_.push_back(parentIdx);
            children_.push_back({});
            children_[newIndex].fill(-1);

            children_[parentIdx][childSlot] = newIndex;
            return newIndex;
        }

        /**
         * @brief Creates a child node in-place by forwarding constructor arguments to T.
         *
         * Throws a std::runtime_error if the child slot is already occupied.
         *
         * @tparam Args Parameter pack for constructing the child's value.
         * @param parentIdx Index of the parent node.
         * @param childSlot Which child slot to use (range: [0..N-1]).
         * @param args      Parameters forwarded to T's constructor.
         * @return The index of the newly created child node.
         */
        template<typename... Args>
        int emplaceChild(int parentIdx, int childSlot, Args&&... args)
        {
            checkIndexValid(parentIdx);
            checkChildSlot(childSlot);

            if (children_[parentIdx][childSlot] != -1) {
                throw std::runtime_error("Child slot is already occupied.");
            }

            int newIndex = static_cast<int>(values_.size());

            values_.emplace_back(std::forward<Args>(args)...);
            parents_.push_back(parentIdx);
            children_.push_back({});
            children_[newIndex].fill(-1);

            children_[parentIdx][childSlot] = newIndex;
            return newIndex;
        }

        // ---------------------- Accessors ---------------------- //

        /**
         * @brief Provides mutable access to the value of the node at the given index.
         * @param idx The index of the node in the SoA.
         * @return A reference to the value of type T.
         */
        T& value(int idx)
        {
            checkIndexValid(idx);
            return values_[idx];
        }

        /**
         * @brief Provides read-only access to the value of the node at the given index.
         * @param idx The index of the node in the SoA.
         * @return A const reference to the value of type T.
         */
        const T& value(int idx) const
        {
            checkIndexValid(idx);
            return values_[idx];
        }

        /**
         * @brief Retrieves the parent index of the node at the given index.
         * @param idx The index of the node.
         * @return The index of the parent node, or -1 if the node is root or if invalid.
         */
        int parentIndex(int idx) const
        {
            checkIndexValid(idx);
            return parents_[idx];
        }

        /**
         * @brief Retrieves the index of the child in the given child slot.
         * @param idx   The index of the node.
         * @param slot  The child slot (range: [0..N-1]).
         * @return The child index, or -1 if the slot is empty.
         */
        int childIndex(int idx, int slot) const
        {
            checkIndexValid(idx);
            checkChildSlot(slot);
            return children_[idx][slot];
        }

        /**
         * @brief Removes a child from its parent's slot but does not erase that child from the arrays.
         *
         * This effectively disconnects the child, setting its parent to -1 and resetting the
         * parent’s child slot to -1. The child node itself remains in the SoA, potentially
         * becoming a new root or an orphaned subtree.
         *
         * @param parentIdx The index of the parent.
         * @param childSlot The child slot to remove (range: [0..N-1]).
         */
        void removeChild(int parentIdx, int childSlot)
        {
            checkIndexValid(parentIdx);
            checkChildSlot(childSlot);

            int cIdx = children_[parentIdx][childSlot];
            if (cIdx == -1) {
                return; // No child at this slot.
            }
            // Reset child's parent index
            parents_[cIdx] = -1;
            // Reset parent slot
            children_[parentIdx][childSlot] = -1;
        }

        /**
         * @brief Returns the total number of nodes currently stored in the tree.
         * @return The size as a std::size_t.
         */
        size_type size() const noexcept
        {
            return values_.size();
        }

        /**
         * @brief Retrieves the index of the root node.
         * @return The root index, or -1 if no root has been created.
         */
        int rootIndex() const noexcept
        {
            return rootIndex_;
        }

        /**
         * @brief Checks if the given index is within the valid range.
         * @param idx The node index to check.
         * @return True if valid, false otherwise.
         */
        bool isValidIndex(int idx) const noexcept
        {
            return (idx >= 0 && static_cast<size_type>(idx) < values_.size());
        }

        // ---------------------- Tree Traversals ---------------------- //

        /**
         * @brief Performs a pre-order traversal (current node, then children).
         *
         * @tparam F A callable with signature: void(int nodeIdx, const T& val).
         * @param nodeIdx The index of the node from which to start.
         * @param visit   The visiting function to be applied to each node.
         */
        template<typename F>
        void preorderTraverse(int nodeIdx, F&& visit)
        {
            if (nodeIdx == -1) {
                return;
            }
            // (1) Visit current node
            visit(nodeIdx, values_[nodeIdx]);

            // (2) Recursively traverse children
            for (std::size_t slot = 0; slot < N; ++slot) {
                int c = children_[nodeIdx][slot];
                if (c != -1) {
                    preorderTraverse(c, visit);
                }
            }
        }

        /**
         * @brief Performs a post-order traversal (children, then current node).
         *
         * @tparam F A callable with signature: void(int nodeIdx, const T& val).
         * @param nodeIdx The index of the node from which to start.
         * @param visit   The visiting function to be applied to each node.
         */
        template<typename F>
        void postorderTraverse(int nodeIdx, F&& visit)
        {
            if (nodeIdx == -1) {
                return;
            }
            // (1) Traverse children
            for (std::size_t slot = 0; slot < N; ++slot) {
                int c = children_[nodeIdx][slot];
                if (c != -1) {
                    postorderTraverse(c, visit);
                }
            }
            // (2) Visit current node
            visit(nodeIdx, values_[nodeIdx]);
        }

        /**
         * @brief Performs an in-order traversal. For N=2, this becomes "left, current, right".
         *
         * For N>2, there is no universal standard for in-order traversal, so this method
         * splits the children into two halves: the first half is visited before the current
         * node, and the second half is visited after the current node.
         *
         * @tparam F A callable with signature: void(int nodeIdx, const T& val).
         * @param nodeIdx The index of the node from which to start.
         * @param visit   The visiting function to be applied to each node.
         */
        template<typename F>
        void inorderTraverse(int nodeIdx, F&& visit)
        {
            if (nodeIdx == -1) {
                return;
            }
            // For multi-way trees with N>2, define mid = N/2 for demonstration.
            std::size_t mid = N / 2;

            // (1) Traverse the first half of the children
            for (std::size_t slot = 0; slot < mid; ++slot) {
                int c = children_[nodeIdx][slot];
                if (c != -1) {
                    inorderTraverse(c, visit);
                }
            }
            // (2) Visit the current node
            visit(nodeIdx, values_[nodeIdx]);

            // (3) Traverse the second half of the children
            for (std::size_t slot = mid; slot < N; ++slot) {
                int c = children_[nodeIdx][slot];
                if (c != -1) {
                    inorderTraverse(c, visit);
                }
            }
        }

    private:
        // ---------------------- Validation Helpers ---------------------- //

        /**
         * @brief Checks if the node index is valid. Throws std::out_of_range if invalid.
         * @param idx The node index to check.
         */
        void checkIndexValid(int idx) const
        {
            if (!isValidIndex(idx)) {
                throw std::out_of_range("Node index out of range");
            }
        }

        /**
         * @brief Checks if the child slot is valid. Throws std::out_of_range if invalid.
         * @param slot The child slot to check.
         */
        void checkChildSlot(int slot) const
        {
            if (slot < 0 || static_cast<size_type>(slot) >= N) {
                throw std::out_of_range("Child slot out of range");
            }
        }

    private:
        // ---------------------- SoA Data Members ---------------------- //

        /// (1) Stores the values of all nodes.
        std::vector<T> values_;

        /// (2) Stores the parent index of each node. A value of -1 indicates no parent.
        std::vector<int> parents_;

        /// (3) Stores the child indices for each node in a fixed-size array of length N.
        std::vector<std::array<int, N>> children_;

        /// The index of the root node. -1 indicates that no root has been created.
        int rootIndex_;
    };

} // namespace lux::cxx
