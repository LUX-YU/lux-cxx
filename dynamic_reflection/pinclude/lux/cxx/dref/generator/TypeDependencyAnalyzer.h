#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <functional>
#include <algorithm>
#include <lux/cxx/dref/runtime/Type.hpp>
#include <lux/cxx/dref/runtime/Declaration.hpp>

namespace lux::cxx::dref 
{
    /**
     * @brief 收集某个 Type 对其它 Type 的直接依赖关系。
     *
     * @param t 待分析的类型指针 (非空)
     * @return std::vector<Type*> t 依赖的类型列表
     */
    inline std::vector<Type*> gatherDependencies(Type* t)
    {
        std::vector<Type*> deps;
        if (!t) return deps;

        switch (t->kind) {
        case ETypeKind::POINTER: {
            auto ptrTy = static_cast<PointerType*>(t);
            if (ptrTy->pointee) {
                deps.push_back(ptrTy->pointee);
            }
            break;
        }
        case ETypeKind::LVALUE_REFERENCE:
        case ETypeKind::RVALUE_REFERENCE: {
            auto refTy = static_cast<ReferenceType*>(t);
            if (refTy->referred_type) {
                deps.push_back(refTy->referred_type);
            }
            break;
        }
        case ETypeKind::FUNCTION: {
            auto funcTy = static_cast<FunctionType*>(t);
            // 返回类型
            if (funcTy->result_type) {
                deps.push_back(funcTy->result_type);
            }
            // 参数类型
            for (auto* p : funcTy->param_types) {
                if (p) deps.push_back(p);
            }
            break;
        }
        case ETypeKind::RECORD: {
            auto recTy = static_cast<RecordType*>(t);
            // 1) 基类
            for (auto* baseRec : recTy->bases) {
                if (baseRec) deps.push_back(baseRec);
            }
            // 2) 构造函数
            for (auto* ctor : recTy->constructor_decls) {
                if (ctor) {
                    deps.push_back(ctor); // FunctionType
                }
            }
            // 3) 析构函数
            if (recTy->destructor_decl) {
                deps.push_back(recTy->destructor_decl);
            }
            // 4) 普通方法
            for (auto* m : recTy->method_decls) {
                if (m) deps.push_back(m);
            }
            // 5) 静态方法
            for (auto* sm : recTy->static_method_decls) {
                if (sm) deps.push_back(sm);
            }
            // 6) 字段 (可能是 Type* 指向字段类型)
            for (auto* fldTy : recTy->field_decls) {
                if (fldTy) deps.push_back(fldTy);
            }
            break;
        }
        case ETypeKind::ENUM:
        case ETypeKind::BUILTIN:
        case ETypeKind::ARRAY:
        case ETypeKind::CXX_RECORD:
        case ETypeKind::UNSUPPORTED:
        default:
            // 暂不收集或无特定依赖
            // 注意：若 ARRAY 有 elementType 指针，也要放到这里处理
            break;
        }

        return deps;
    }

    /**
     * @brief 构建图：将 types 列表映射到索引，然后收集依赖边 adjacency。
     *
     * @param types  所有需要排序的 Type*
     * @param outIndexMap  (输出) Type* -> index
     * @param outAdjList   (输出) index -> list of indices(所依赖的类型)
     */
    inline void buildGraph(
        const std::vector<Type*>& types,
        std::unordered_map<Type*, int>& outIndexMap,
        std::vector<std::vector<int>>& outAdjList
    ) {
        outIndexMap.clear();
        outAdjList.clear();

        outIndexMap.reserve(types.size());
        for (int i = 0; i < (int)types.size(); i++) {
            outIndexMap[types[i]] = i;
        }

        outAdjList.resize(types.size());

        // 收集依赖
        for (int i = 0; i < (int)types.size(); i++) {
            Type* t = types[i];
            auto depList = gatherDependencies(t);
            for (auto* d : depList) {
                auto it = outIndexMap.find(d);
                if (it != outIndexMap.end()) {
                    outAdjList[i].push_back(it->second);
                }
                else {
                    // 依赖不在当前列表 -> 视情况可忽略或放入外部依赖
                }
            }
        }
    }

    /**
     * @brief 使用 Tarjan 算法获取强连通分量 (SCC)。
     *
     * @param adjList 邻接表
     * @return std::vector<std::vector<int>>  每个元素是一组 index，表示一个SCC
     *                                       这些SCC按拓扑顺序(后序)排列
     */
    inline std::vector<std::vector<int>> tarjanSCC(const std::vector<std::vector<int>>& adjList)
    {
        int n = (int)adjList.size();
        std::vector<int> index(n, -1), lowlink(n, -1);
        std::vector<bool> onStack(n, false);
        std::vector<int> stack_;
        stack_.reserve(n);

        int currentIndex = 0;
        std::vector<std::vector<int>> sccList;

        std::function<void(int)> strongconnect = [&](int v) {
            index[v] = currentIndex;
            lowlink[v] = currentIndex;
            currentIndex++;
            stack_.push_back(v);
            onStack[v] = true;

            // 遍历 v 的相邻节点
            for (int w : adjList[v]) {
                if (index[w] == -1) {
                    // w尚未访问
                    strongconnect(w);
                    lowlink[v] = std::min(lowlink[v], lowlink[w]);
                }
                else if (onStack[w]) {
                    // w在栈中 => w与v在同一个SCC
                    lowlink[v] = std::min(lowlink[v], index[w]);
                }
            }

            // 如果 v 是root
            if (lowlink[v] == index[v]) {
                // 弹栈直到 v
                std::vector<int> scc;
                while (true) {
                    int w = stack_.back();
                    stack_.pop_back();
                    onStack[w] = false;
                    scc.push_back(w);
                    if (w == v) break;
                }
                sccList.push_back(std::move(scc));
            }
            };

        for (int i = 0; i < n; i++) {
            if (index[i] == -1) {
                strongconnect(i);
            }
        }

        return sccList;
    }

    /**
     * @brief 对给定 Type* 列表做拓扑排序。
     *        如果图中存在循环依赖，会将它们分到同一个 SCC (强连通分量)。
     *        同一个 SCC 内的类型需要先做前置声明，然后再一起输出定义。
     *
     * @param types 所有需要排序的类型
     * @return std::vector<std::vector<Type*>>  每个 vector<Type*> 表示一个SCC
     */
    inline std::vector<std::vector<Type*>> topologicalSortTypes(const std::vector<Type*>& types)
    {
        // 1. 构建邻接表
        std::unordered_map<Type*, int> indexMap;
        std::vector<std::vector<int>> adjList;
        buildGraph(types, indexMap, adjList);

        // 2. Tarjan 获取 SCC
        auto sccList = tarjanSCC(adjList);

        // 3. 转换回 Type* 
        //    Tarjan 返回的 sccList 里每个 scc 的元素是节点索引
        //    我们需要一个 index->Type* 的反向映射
        std::vector<Type*> idx2Type(types.size(), nullptr);
        for (auto& [tptr, idx] : indexMap) {
            idx2Type[idx] = tptr;
        }

        std::vector<std::vector<Type*>> result;
        result.reserve(sccList.size());
        for (auto& scc : sccList) {
            std::vector<Type*> group;
            group.reserve(scc.size());
            for (int idx : scc) {
                group.push_back(idx2Type[idx]);
            }
            result.push_back(std::move(group));
        }

        return result;
    }

} // namespace lux::cxx::dref
