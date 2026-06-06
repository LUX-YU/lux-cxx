#include <lux/cxx/container/Tree.hpp>
#include <iostream>
#include <cassert>
#include <memory>

template<typename T> using BinaryTreeNode = lux::cxx::StaticTreeNode<T, 2>;
template<typename T> using OctreeNode = lux::cxx::StaticTreeNode<T, 8>;

int main()
{
    using namespace lux::cxx;

    {
        // 在栈上创建一个根节点，二叉树节点
        BinaryTreeNode<int> root(10);

        // 我们希望给根节点设置左右孩子，都在堆上用unique_ptr管理
        // 不需要手动 new / delete
        auto left = std::make_unique<BinaryTreeNode<int>>(20);
        auto right = std::make_unique<BinaryTreeNode<int>>(30);

        // 静态节点 N=2, 下标只能是0或1
        root.setChild(0, std::move(left));
        root.setChild(1, std::move(right));

        // 访问子节点
        auto* lc = root.getChild(0);
        auto* rc = root.getChild(1);

        if (lc) {
            std::cout << "Left child value = " << lc->value() << "\n";
        }
        if (rc) {
            std::cout << "Right child value = " << rc->value() << "\n";
        }
    }

    {
        // 构造一个二叉树 (N=2)
        IndexedNaryTreeSoA<int, 2> tree;

        // 创建根节点
        int root = tree.emplaceRoot(10);
        // 创建左子
        int left = tree.createChild(root, 0, 20);
        // 创建右子
        int right = tree.createChild(root, 1, 30);

        std::cout << "--- Preorder ---\n";
        tree.preorderTraverse(root, 
            [&](int idx, const int& val) {
                std::cout << "node=" << idx << ", value=" << val << "\n";
            }
        );

        std::cout << "\n--- Inorder ---\n";
        tree.inorderTraverse(root, 
            [&](int idx, const int& val) {
                std::cout << "node=" << idx << ", value=" << val << "\n";
            }
        );

        std::cout << "\n--- Postorder ---\n";
        tree.postorderTraverse(root,
            [&](int idx, const int& val) {
                std::cout << "node=" << idx << ", value=" << val << "\n";
            }
        );
    }

    {
        // DynamicTreeNode: this instantiation is what surfaced the ill-formed
        // `~DynamicTreeNode() override` on a non-virtual base destructor. It must
        // now compile and behave correctly.
        auto root = std::make_unique<DynamicTreeNode<int>>(1);
        root->addChild(std::make_unique<DynamicTreeNode<int>>(2));
        root->addChild(std::make_unique<DynamicTreeNode<int>>(3));

        assert(root->childCount() == 2);
        assert(root->getChild(0)->value() == 2);
        assert(root->getChild(1)->value() == 3);

        auto removed = root->removeChild(0);
        assert(removed && removed->value() == 2);
        assert(root->childCount() == 1);
        assert(root->getChild(0)->value() == 3);

        std::cout << "\nDynamicTreeNode OK\n";
    }

    return 0;
}