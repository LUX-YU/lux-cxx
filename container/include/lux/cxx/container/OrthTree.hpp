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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>
#include <memory_resource>

namespace lux::cxx
{
    /**
     * @brief Axis-aligned bounding box (AABB) in @p Dim dimensions.
     *
     * Stores per-axis minimum and maximum values. Provides utility methods for
     * validity checking, center computation, containment, and intersection tests
     * against other boxes and hyperspheres.
     *
     * @tparam Scalar Numeric type used for coordinates (e.g., float, double).
     * @tparam Dim    Number of spatial dimensions.
     */
    template <typename Scalar, std::size_t Dim>
    struct Box
    {
        using scalar_type = Scalar;
        static constexpr std::size_t dim = Dim;

        std::array<Scalar, Dim> min{};
        std::array<Scalar, Dim> max{};

        constexpr Box() = default;

        /**
         * @brief Constructs a box from explicit min/max arrays.
         * @param mn Per-axis minimum values.
         * @param mx Per-axis maximum values.
         */
        constexpr Box(const std::array<Scalar, Dim>& mn, const std::array<Scalar, Dim>& mx)
            : min(mn), max(mx) {
        }

        /**
         * @brief Checks whether the box is geometrically valid (min <= max on all axes).
         * @return True if valid, false otherwise.
         */
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            for (std::size_t i = 0; i < Dim; ++i)
                if (min[i] > max[i]) return false;
            return true;
        }

        /**
         * @brief Computes the center point of the box.
         * @return An array of Scalar values representing the per-axis center.
         */
        [[nodiscard]] constexpr std::array<Scalar, Dim> center() const noexcept
        {
            std::array<Scalar, Dim> c{};
            for (std::size_t i = 0; i < Dim; ++i)
                c[i] = (min[i] + max[i]) * Scalar(0.5);
            return c;
        }

        /**
         * @brief Tests whether a point lies within (or on the boundary of) this box.
         * @tparam VecLike A vector-like type whose elements are accessible via operator[].
         * @param p The point to test.
         * @return True if the point is contained in the box, false otherwise.
         */
        template <class VecLike>
        [[nodiscard]] constexpr bool contains(const VecLike& p) const noexcept
        {
            for (std::size_t i = 0; i < Dim; ++i)
            {
                if (p[i] < min[i]) return false;
                if (p[i] > max[i]) return false;
            }
            return true;
        }

        /**
         * @brief Tests whether this box overlaps another box.
         * @param other The other bounding box.
         * @return True if the two boxes intersect, false if they are disjoint.
         */
        [[nodiscard]] constexpr bool intersects(const Box& other) const noexcept
        {
            for (std::size_t i = 0; i < Dim; ++i)
            {
                if (max[i] < other.min[i]) return false;
                if (min[i] > other.max[i]) return false;
            }
            return true;
        }

        /**
         * @brief Tests whether this box intersects a hypersphere (circle in 2D, sphere in 3D).
         *
         * Uses the squared-distance clamping technique for efficiency. An early-out is
         * applied once the accumulated squared distance exceeds the squared radius.
         *
         * @tparam VecLike A vector-like type whose elements are accessible via operator[].
         * @param center The center of the hypersphere.
         * @param radius The radius of the hypersphere.
         * @return True if this box intersects the hypersphere, false otherwise.
         */
        template <class VecLike>
        [[nodiscard]] constexpr bool intersectsBall(const VecLike& center, Scalar radius) const noexcept
        {
            Scalar dist2 = Scalar(0);
            const Scalar r2 = radius * radius;
            for (std::size_t i = 0; i < Dim; ++i)
            {
                Scalar v = Scalar(center[i]);
                Scalar clamped = (v < min[i]) ? min[i] : (v > max[i] ? max[i] : v);
                Scalar d = v - clamped;
                dist2 += d * d;
                if (dist2 > r2) return false; // early-out
            }
            return dist2 <= r2;
        }
    };

    /**
     * @brief Default position accessor functor.
     *
     * Assumes the point type has a public @c .position member that supports
     * element access via @c operator[]. Users are encouraged to provide a
     * custom @c GetPosition functor when working with point types that have
     * a different layout.
     *
     * @tparam PointT The point type stored in the tree.
     */
    template <class PointT>
    struct DefaultGetPosition
    {
        constexpr decltype(auto) operator()(const PointT& p) const noexcept
        {
            return (p.position);
        }
    };

    /**
     * @brief Generic Dim-dimensional orthographic tree (quadtree for Dim=2, octree for Dim=3).
     *
     * Subdivides space recursively using axis-aligned hyperplanes. Each internal node has
     * exactly @c 2^Dim children. Leaf nodes store points together with an alive-flag
     * array to support soft-deletion without immediate compaction.
     *
     * The tree is parameterised over an allocator so that node and point storage can be
     * backed by any standard-compliant allocator (including PMR allocators).
     *
     * @tparam PointT        Type of the spatial points stored in the tree.
     * @tparam Dim           Number of spatial dimensions (must be >= 1).
     * @tparam Scalar        Scalar type used for bounding-box coordinates.
     * @tparam GetPosition   Functor that extracts a position from a PointT value.
     * @tparam BaseAllocator Standard allocator used for all internal storage.
     */
    template <
        class PointT,
        std::size_t Dim,
        class Scalar = float,
        class GetPosition = DefaultGetPosition<PointT>,
        class BaseAllocator = std::allocator<std::byte>>
    class Orthtree
    {
        static_assert(Dim >= 1, "Dim must be >= 1");

    public:
        using point_type = PointT;
        using scalar_type = Scalar;
        using box_type = Box<Scalar, Dim>;
        using node_id = std::uint32_t;

        static constexpr std::size_t kChildCount = (std::size_t(1) << Dim);
        static constexpr node_id kInvalidNode = std::numeric_limits<node_id>::max();

    private:
        // allocator rebinding
        using base_alloc_traits = std::allocator_traits<BaseAllocator>;

        template <class T>
        using rebind_alloc_t = typename base_alloc_traits::template rebind_alloc<T>;

        struct Node;

        using node_allocator = rebind_alloc_t<Node>;
        using point_allocator = rebind_alloc_t<PointT>;
        using byte_allocator = rebind_alloc_t<std::uint8_t>;

        struct Node
        {
            /// Axis-aligned bounding box of this node.
            box_type bounds{};
            /// Child node IDs (kInvalidNode means the child slot is empty).
            std::array<node_id, kChildCount> children{};
            /// True when this node is a leaf (stores points directly).
            bool is_leaf = true;
            /// Subdivision depth from the root.
            std::uint32_t depth = 0;

            /// Points stored in this leaf.
            std::vector<PointT, point_allocator> points;
            /// Alive flags parallel to @c points (1 = alive, 0 = soft-deleted).
            std::vector<std::uint8_t, byte_allocator> alive;

            /// Dirty flag consumed by external systems (e.g., for GPU upload or synchronisation).
            bool dirty = false;
            /// Indicates that soft-deleted (dead) points are present and compaction is needed.
            bool need_compact = false;

            /**
             * @brief Constructs a Node with the given per-point and alive-byte allocators.
             */
            explicit Node(const point_allocator& pa = point_allocator{},
                const byte_allocator& ba = byte_allocator{})
                : points(pa), alive(ba)
            {
                children.fill(kInvalidNode);
            }

            /// Returns the number of alive (non-deleted) points in this node.
            [[nodiscard]] std::size_t aliveCount() const noexcept
            {
                std::size_t c = 0;
                for (std::uint8_t f : alive) if (f) ++c;
                return c;
            }

            /// Returns true if no alive points exist in this node.
            [[nodiscard]] bool emptyAlive() const noexcept
            {
                for (std::uint8_t f : alive) if (f) return false;
                return true;
            }
        };

    public:
        /**
         * @brief Configuration parameters for constructing an Orthtree.
         */
        struct Config
        {
            /// Bounding box of the tree's root node.
            box_type      root_bounds{};
            /// Maximum subdivision depth.
            std::uint32_t max_depth = 10;
            /// Maximum number of alive points a leaf may hold before it is split.
            std::uint32_t max_points_per_leaf = 512;
        };

        /**
         * @brief A deep-copy snapshot of a single leaf node.
         *
         * Captures all point and alive-flag data at the time of creation.
         * Safe to use without any lock after capture.
         */
        struct LeafView
        {
            node_id id = kInvalidNode;
            box_type bounds{};
            bool dirty = false;

            std::vector<PointT> points;
            std::vector<std::uint8_t> alive;

            /// Returns the total number of points (including soft-deleted ones) in this view.
            [[nodiscard]] std::size_t count() const noexcept { return points.size(); }
            /// Returns true if there are no points in this view.
            [[nodiscard]] bool empty() const noexcept { return points.empty(); }
        };

        /**
         * @brief A versioned snapshot containing deep copies of all qualifying leaf views.
         */
        struct Snapshot
        {
            std::vector<LeafView> leaves;
            std::uint64_t version = 0;
        };

    public:
        /**
         * @brief Constructs an Orthtree with the given configuration.
         *
         * If @p cfg.root_bounds is invalid, the root bounds default to the Dim-dimensional
         * unit hypercube [-1, 1]^Dim.
         *
         * @param cfg     Tree configuration (bounds, depth limit, leaf capacity).
         * @param get_pos Position accessor functor.
         * @param alloc   Base allocator for all internal storage.
         */
        explicit Orthtree(const Config& cfg,
            const GetPosition& get_pos = GetPosition{},
            const BaseAllocator& alloc = BaseAllocator{})
            : config_(cfg),
            get_pos_(get_pos),
            base_alloc_(alloc),
            nodes_(node_allocator(base_alloc_))
        {
            if (!config_.root_bounds.isValid())
            {
                // Default to the [-1, 1]^Dim hypercube when no valid bounds are provided.
                std::array<Scalar, Dim> mn{};
                std::array<Scalar, Dim> mx{};
                for (std::size_t i = 0; i < Dim; ++i) { mn[i] = Scalar(-1); mx[i] = Scalar(1); }
                config_.root_bounds = box_type(mn, mx);
            }

            nodes_.reserve(1024);
            root_ = allocateNode();
            nodes_[root_].bounds = config_.root_bounds;
            nodes_[root_].is_leaf = true;
            nodes_[root_].depth = 0;
        }

        // ---------------------- Accessors ---------------------- //

        /// Returns the current configuration.
        [[nodiscard]] const Config& config() const noexcept { return config_; }
        /// Returns the monotonically increasing version counter (incremented on every mutation).
        [[nodiscard]] std::uint64_t version() const noexcept { return version_; }
        /// Returns the node ID of the root node.
        [[nodiscard]] node_id root() const noexcept { return root_; }
        /// Returns the total number of allocated nodes (including internal nodes).
        [[nodiscard]] std::uint32_t nodeCount() const noexcept { return static_cast<std::uint32_t>(nodes_.size()); }

        // ---------------------- Modification ---------------------- //

        /**
         * @brief Removes all points and resets the tree to a single empty root leaf.
         *
         * Increments the version counter.
         */
        void clear()
        {
            nodes_.clear();
            nodes_.reserve(1024);
            root_ = allocateNode();
            nodes_[root_].bounds = config_.root_bounds;
            nodes_[root_].is_leaf = true;
            nodes_[root_].depth = 0;
            ++version_;
        }

        // ---------------------- Insertion ---------------------- //

        /**
         * @brief Inserts a point into the tree.
         *
         * Returns false if the point lies outside the root bounds or the root is invalid.
         * Increments the version counter on success.
         *
         * @param p The point to insert.
         * @return True if the point was inserted, false otherwise.
         */
        bool insert(const PointT& p)
        {
            if (root_ == kInvalidNode) return false;
            bool inserted = insertInternal(root_, p, 0);
            if (inserted) ++version_;
            return inserted;
        }

        /**
         * @brief Constructs a @c PointT from @p args and inserts it via move.
         *
         * The point is constructed on the stack from the forwarded arguments (one
         * construction, zero copies), the position is extracted for tree routing, and
         * the object is then **moved** into the target leaf's storage (one move).
         *
         * This is cheaper than @c insert when the position accessor returns by reference
         * (avoids one copy of PointT), but it is *not* in-place construction directly
         * into the leaf vector storage — the point lives on the stack until the target
         * leaf is found. For true in-place construction use @c emplaceAt.
         *
         * Returns false if the point lies outside the root bounds.
         * Increments the version counter on success.
         *
         * @tparam Args Constructor argument types for @c PointT.
         * @param args  Arguments forwarded to @c PointT's constructor.
         * @return True if the point was inserted, false otherwise.
         */
        template <class... Args>
        bool emplace(Args&&... args)
        {
            if (root_ == kInvalidNode) return false;
            PointT p(std::forward<Args>(args)...);
            bool inserted = emplaceInternal(root_, std::move(p), 0);
            if (inserted) ++version_;
            return inserted;
        }

        /**
         * @brief True in-place construction: routes via @p pos then constructs directly
         *        inside the target leaf's @c points vector via @c emplace_back.
         *
         * Because the position is provided separately, the tree can locate the correct
         * leaf *before* constructing the @c PointT object at all. This means the point
         * is constructed exactly once, directly in the vector's storage (no intermediate
         * object, no move). Only a vector reallocation can trigger a subsequent
         * move/copy, governed by @c PointT's move constructor.
         *
         * @code
         * // Typical usage:
         * tree.emplaceAt(pt.position, pt.position, pt.id, pt.payload);
         * @endcode
         *
         * Returns false if @p pos lies outside the root bounds.
         * Increments the version counter on success.
         *
         * @tparam VecLike A vector-like type accessible via @c operator[] (e.g.
         *                 @c std::array<float,Dim>).
         * @tparam Args    Constructor argument types for @c PointT.
         * @param pos  Position used solely for tree routing (not stored separately).
         * @param args Arguments forwarded directly to @c PointT's constructor at the
         *             target leaf via @c emplace_back.
         * @return True if the point was inserted, false otherwise.
         */
        template <class VecLike, class... Args>
        bool emplaceAt(const VecLike& pos, Args&&... args)
        {
            if (root_ == kInvalidNode) return false;
            bool inserted = emplaceAtInternal(root_, pos, 0, std::forward<Args>(args)...);
            if (inserted) ++version_;
            return inserted;
        }

        /**
         * @brief Inserts multiple points in bulk.
         * @tparam Range Any range whose value type is @c PointT.
         * @param pts The range of points to insert.
         */
        template <class Range>
        void insertMany(const Range& pts)
        {
            if (root_ == kInvalidNode) return;
            bool any = false;
            for (const auto& p : pts)
                any |= insertInternal(root_, p, 0);
            if (any) ++version_;
        }

        // ---------------------- Soft Deletion ---------------------- //

        /**
         * @brief Soft-deletes all alive points whose positions fall within @p region.
         *
         * Points are marked dead (alive flag set to 0). Call @c compactDirtyLeaves() to
         * physically remove them. Increments the version counter if any points were marked.
         *
         * @param region The axis-aligned box defining the deletion region.
         * @return The number of points that were marked as deleted.
         */
        std::size_t markDeletedInBox(const box_type& region)
        {
            if (root_ == kInvalidNode) return 0;

            auto prune = [&](const box_type& b) { return b.intersects(region); };
            auto pred = [&](const PointT& p) {
                const auto& pos = get_pos_(p);
                return region.contains(pos);
            };

            std::size_t removed = markDeletedRecursive(root_, pred, prune);
            if (removed > 0) ++version_;
            return removed;
        }

        /**
         * @brief Soft-deletes all alive points within a hypersphere.
         *
         * @tparam VecLike A vector-like type whose elements are accessible via operator[].
         * @param center The center of the hypersphere.
         * @param radius The radius of the hypersphere.
         * @return The number of points that were marked as deleted.
         */
        template <class VecLike>
        std::size_t markDeletedInBall(const VecLike& center, Scalar radius)
        {
            if (root_ == kInvalidNode) return 0;
            const Scalar r2 = radius * radius;

            auto prune = [&](const box_type& b) { return b.intersectsBall(center, radius); };
            auto pred = [&](const PointT& p) {
                const auto& pos = get_pos_(p);
                Scalar d2 = Scalar(0);
                for (std::size_t i = 0; i < Dim; ++i)
                {
                    Scalar d = Scalar(pos[i]) - Scalar(center[i]);
                    d2 += d * d;
                    if (d2 > r2) return false;
                }
                return d2 <= r2;
            };

            std::size_t removed = markDeletedRecursive(root_, pred, prune);
            if (removed > 0) ++version_;
            return removed;
        }

        /**
         * @brief Soft-deletes all points that satisfy an arbitrary predicate.
         *
         * No bounding-box early-out is applied; every leaf is visited.
         *
         * @tparam Predicate Callable with signature: bool(const PointT&).
         * @param pred The predicate; returns true for points that should be deleted.
         * @return The number of points that were marked as deleted.
         */
        template <class Predicate>
        std::size_t markDeletedIf(Predicate pred)
        {
            if (root_ == kInvalidNode) return 0;

            auto prune = [](const box_type&) { return true; };

            std::size_t removed = markDeletedRecursive(root_, pred, prune);
            if (removed > 0) ++version_;
            return removed;
        }

        // ---------------------- Compaction ---------------------- //

        /**
         * @brief Physically removes all soft-deleted points from dirty leaf nodes.
         *
         * After compaction, point indices within leaves may change. The version counter
         * is incremented if any points were removed.
         *
         * @return The total number of points physically removed.
         */
        std::size_t compactDirtyLeaves()
        {
            std::size_t total_removed = 0;
            for (auto& node : nodes_)
            {
                if (!node.is_leaf) continue;
                if (!node.need_compact) continue;

                std::size_t write = 0;
                for (std::size_t read = 0; read < node.points.size(); ++read)
                {
                    if (node.alive[read])
                    {
                        if (write != read)
                        {
                            node.points[write] = std::move(node.points[read]);
                            node.alive[write]  = 1;
                        }
                        ++write;
                    }
                    else
                    {
                        ++total_removed;
                    }
                }
                node.points.resize(write);
                node.alive.resize(write);
                node.need_compact = false;
                node.dirty = true;
            }

            if (total_removed > 0) ++version_;
            return total_removed;
        }

        // ---------------------- Queries ---------------------- //

        /**
         * @brief Visits every alive point inside @p region, invoking @p fn on each.
         *
         * @tparam Func Callable with signature: void(const PointT&).
         * @param region The axis-aligned query box.
         * @param fn     The visitor function.
         */
        template <class Func>
        void forEachPointInBox(const box_type& region, Func&& fn) const
        {
            if (root_ == kInvalidNode) return;
            forEachPointInBoxRecursive(root_, region, fn);
        }

        /**
         * @brief Writes every alive point inside @p region to an output iterator.
         *
         * Equivalent to calling @c forEachPointInBox with an appending lambda.
         *
         * @tparam OutputIt An iterator satisfying OutputIterator requirements.
         * @param region The axis-aligned query box.
         * @param out    The output iterator to write results to.
         */
        template <class OutputIt>
        void queryPointsInBox(const box_type& region, OutputIt out) const
        {
            forEachPointInBox(region, [&](const PointT& p) { *out++ = p; });
        }

        // ---------------------- Snapshot ---------------------- //

        /**
         * @brief Creates a deep-copy snapshot of all leaf nodes.
         *
         * The snapshot captures point data and alive flags for each qualifying leaf.
         * Note: @p skip_empty_leaves skips leaves where @c aliveCount()==0, not where
         * @c points is empty.
         *
         * @param skip_empty_leaves If true, leaves with no alive points are omitted.
         * @return A @c Snapshot containing the captured leaf views.
         */
        Snapshot captureSnapshot(bool skip_empty_leaves = true) const
        {
            Snapshot snap;
            snap.version = version_;

            if (root_ == kInvalidNode) return snap;

            snap.leaves.reserve(nodes_.size());
            for (node_id id = 0; id < static_cast<node_id>(nodes_.size()); ++id)
            {
                const Node& node = nodes_[id];
                if (!node.is_leaf) continue;

                if (skip_empty_leaves && node.emptyAlive()) continue;

                LeafView view;
                view.id = id;
                view.bounds = node.bounds;
                view.dirty = node.dirty;

                view.points.assign(node.points.begin(), node.points.end());
                view.alive.assign(node.alive.begin(), node.alive.end());

                snap.leaves.push_back(std::move(view));
            }
            return snap;
        }

        // ---------------------- Statistics ---------------------- //

        /// Returns the total number of alive (non-deleted) points across all leaf nodes.
        std::uint64_t totalAlivePoints() const
        {
            std::uint64_t total = 0;
            for (const auto& node : nodes_)
            {
                if (!node.is_leaf) continue;
                for (std::uint8_t a : node.alive) if (a) ++total;
            }
            return total;
        }

        /// Returns the number of leaf nodes that have at least one alive point.
        std::uint32_t leafCountNonEmptyAlive() const
        {
            std::uint32_t c = 0;
            for (const auto& node : nodes_)
            {
                if (!node.is_leaf) continue;
                if (!node.emptyAlive()) ++c;
            }
            return c;
        }

        /**
         * @brief Clears the dirty flag on a selected set of leaf nodes.
         *
         * Intended to be called by an external consumer (e.g., after GPU upload) to
         * acknowledge that the dirty data has been processed.
         *
         * @param leaf_ids The IDs of the leaf nodes whose dirty flag should be cleared.
         */
        template <class Range>
        void clearLeafDirtyFlags(const Range& leaf_ids)
        {
            for (node_id id : leaf_ids)
            {
                if (id == kInvalidNode) continue;
                if (id >= nodes_.size()) continue;
                auto& node = nodes_[id];
                if (!node.is_leaf) continue;
                node.dirty = false;
            }
        }

        /// Clears the dirty flag on all leaf nodes.
        void clearAllLeafDirtyFlags()
        {
            for (auto& node : nodes_)
                if (node.is_leaf) node.dirty = false;
        }

    private:
        // ---------------------- Data Members ---------------------- //

        /// Tree configuration (bounds, depth limit, leaf point capacity).
        Config config_{};
        /// Position accessor functor.
        GetPosition get_pos_{};

        /// Base allocator used to rebind storage for nodes, points, and alive flags.
        BaseAllocator base_alloc_{};
        /// Flat array of all allocated nodes (indexed by node_id).
        std::vector<Node, node_allocator> nodes_;
        /// Node ID of the root node.
        node_id root_ = kInvalidNode;

        /// Monotonically increasing mutation version.
        std::uint64_t version_ = 0;

    private:
        // ---------------------- Internal Helpers ---------------------- //

        /// Allocates a new node and returns its ID.
        node_id allocateNode()
        {
            node_id id = static_cast<node_id>(nodes_.size());
            nodes_.emplace_back(point_allocator(base_alloc_), byte_allocator(base_alloc_));
            return id;
        }

        static box_type computeChildBounds(const box_type& parent, std::size_t child_index)
        {
            auto c = parent.center();
            box_type r;

            for (std::size_t axis = 0; axis < Dim; ++axis)
            {
                bool positive = (child_index & (std::size_t(1) << axis)) != 0;
                r.min[axis] = positive ? c[axis] : parent.min[axis];
                r.max[axis] = positive ? parent.max[axis] : c[axis];
            }
            return r;
        }

        /**
         * @brief Determines which child index a position maps to within a bounding box.
         *
         * Bit @a i of the returned index is set when @p pos[i] >= center[i].
         *
         * @tparam VecLike A vector-like type accessible via operator[].
         * @param bounds The node bounding box.
         * @param pos    The position to classify.
         * @return The child index in [0, 2^Dim).
         */
        template <class VecLike>
        static std::size_t childIndexForPosition(const box_type& bounds, const VecLike& pos)
        {
            return childIndexForPosition(bounds.center(), pos);
        }

        /// Overload that accepts a precomputed center to avoid recomputing it in tight loops.
        template <class VecLike>
        static std::size_t childIndexForPosition(const std::array<Scalar, Dim>& center, const VecLike& pos)
        {
            std::size_t idx = 0;
            for (std::size_t axis = 0; axis < Dim; ++axis)
            {
                if (Scalar(pos[axis]) >= center[axis])
                    idx |= (std::size_t(1) << axis);
            }
            return idx;
        }

        /**
         * @brief Recursive insertion helper.
         *
         * Inserts @p p into the subtree rooted at @p node_id_value. Splits the leaf
         * when the point capacity is exceeded and the depth limit allows.
         *
         * @param node_id_value Root of the subtree.
         * @param p             Point to insert.
         * @param depth         Current depth of @p node_id_value.
         * @return True if the point was inserted.
         */
        bool insertInternal(node_id node_id_value, const PointT& p, std::uint32_t depth)
        {
            const auto& pos = get_pos_(p);

            if (!nodes_[node_id_value].bounds.contains(pos))
                return false;

            if (nodes_[node_id_value].is_leaf)
            {
                nodes_[node_id_value].points.push_back(p);
                nodes_[node_id_value].alive.push_back(1);
                nodes_[node_id_value].dirty = true;

                if (depth < config_.max_depth)
                {
                    // Compare alive count (not raw size) to avoid spurious splits when dead
                    // points inflate the total count.
                    std::size_t alive_cnt = 0;
                    for (std::uint8_t f : nodes_[node_id_value].alive) if (f) ++alive_cnt;
                    if (alive_cnt > config_.max_points_per_leaf)
                        splitLeaf(node_id_value, depth);
                }
                return true;
            }

            // internal node: delegate to the appropriate child
            const std::size_t child_idx = childIndexForPosition(nodes_[node_id_value].bounds, pos);
            node_id child_id = nodes_[node_id_value].children[child_idx];
            if (child_id == kInvalidNode)
            {
                // Save bounds before allocateNode() potentially reallocates nodes_.
                const box_type parent_bounds = nodes_[node_id_value].bounds;
                child_id = allocateNode();
                nodes_[node_id_value].children[child_idx] = child_id;

                Node& child = nodes_[child_id];
                child.bounds = computeChildBounds(parent_bounds, child_idx);
                child.is_leaf = true;
                child.depth = depth + 1;
            }

            return insertInternal(child_id, p, depth + 1);
        }

        /**
         * @brief Move-insertion recursive helper used by @c emplace.
         *
         * Identical in logic to @c insertInternal but takes @p p by rvalue reference so
         * that no copy of the point is made when it is stored in a leaf.
         * The position is copied upfront (typically a small array) to avoid a dangling
         * reference after the point is moved.
         *
         * @param node_id_value Root of the subtree.
         * @param p             Point to move-insert.
         * @param depth         Current depth of @p node_id_value.
         * @return True if the point was inserted.
         */
        bool emplaceInternal(node_id node_id_value, PointT&& p, std::uint32_t depth)
        {
            // Copy the position now; p will be moved into a leaf before the call unwinds.
            auto pos = get_pos_(p);

            if (!nodes_[node_id_value].bounds.contains(pos))
                return false;

            if (nodes_[node_id_value].is_leaf)
            {
                nodes_[node_id_value].points.push_back(std::move(p));
                nodes_[node_id_value].alive.push_back(1);
                nodes_[node_id_value].dirty = true;

                if (depth < config_.max_depth)
                {
                    std::size_t alive_cnt = 0;
                    for (std::uint8_t f : nodes_[node_id_value].alive) if (f) ++alive_cnt;
                    if (alive_cnt > config_.max_points_per_leaf)
                        splitLeaf(node_id_value, depth);
                }
                return true;
            }

            // Internal node: delegate to the appropriate child.
            const std::size_t child_idx = childIndexForPosition(nodes_[node_id_value].bounds, pos);
            node_id child_id = nodes_[node_id_value].children[child_idx];
            if (child_id == kInvalidNode)
            {
                const box_type parent_bounds = nodes_[node_id_value].bounds;
                child_id = allocateNode();
                nodes_[node_id_value].children[child_idx] = child_id;

                Node& child = nodes_[child_id];
                child.bounds = computeChildBounds(parent_bounds, child_idx);
                child.is_leaf = true;
                child.depth = depth + 1;
            }

            return emplaceInternal(child_id, std::move(p), depth + 1);
        }

        /**
         * @brief In-place construction recursive helper used by @c emplaceAt.
         *
         * Routes through the tree using the caller-provided @p pos, then calls
         * @c emplace_back(std::forward<Args>(args)...) directly on the target leaf's
         * @c points vector. The @c PointT object is constructed exactly once, directly
         * in the vector's allocated storage — no intermediate object, no move.
         *
         * @tparam VecLike  Vector-like type for the position.
         * @tparam Args     Constructor argument types for @c PointT.
         * @param node_id_value  Root of the subtree.
         * @param pos            Position used for routing (must remain valid for the
         *                       duration of the call).
         * @param depth          Current depth of @p node_id_value.
         * @param args           Constructor arguments forwarded to @c PointT.
         * @return True if the point was inserted.
         */
        template <class VecLike, class... Args>
        bool emplaceAtInternal(node_id node_id_value, const VecLike& pos,
                               std::uint32_t depth, Args&&... args)
        {
            if (!nodes_[node_id_value].bounds.contains(pos))
                return false;

            if (nodes_[node_id_value].is_leaf)
            {
                // Construct the point directly inside the vector's storage.
                nodes_[node_id_value].points.emplace_back(std::forward<Args>(args)...);
                nodes_[node_id_value].alive.push_back(1);
                nodes_[node_id_value].dirty = true;

                if (depth < config_.max_depth)
                {
                    std::size_t alive_cnt = 0;
                    for (std::uint8_t f : nodes_[node_id_value].alive) if (f) ++alive_cnt;
                    if (alive_cnt > config_.max_points_per_leaf)
                        splitLeaf(node_id_value, depth);
                }
                return true;
            }

            // Internal node: delegate to the appropriate child.
            const std::size_t child_idx = childIndexForPosition(nodes_[node_id_value].bounds, pos);
            node_id child_id = nodes_[node_id_value].children[child_idx];
            if (child_id == kInvalidNode)
            {
                const box_type parent_bounds = nodes_[node_id_value].bounds;
                child_id = allocateNode();
                nodes_[node_id_value].children[child_idx] = child_id;

                Node& child = nodes_[child_id];
                child.bounds = computeChildBounds(parent_bounds, child_idx);
                child.is_leaf = true;
                child.depth = depth + 1;
            }

            return emplaceAtInternal(child_id, pos, depth + 1, std::forward<Args>(args)...);
        }

        /**
         * @brief Converts a leaf node into an internal node by redistributing its points
         *        to newly created children.
         *
         * @param node_id_value The ID of the leaf to split.
         * @param depth         Current depth of the leaf node.
         */
        void splitLeaf(node_id node_id_value, std::uint32_t depth)
        {
            if (!nodes_[node_id_value].is_leaf) return;
            if (depth >= config_.max_depth) return;

            // Move points/alive out to avoid iterator invalidation while allocating children.
            auto old_points = std::move(nodes_[node_id_value].points);
            auto old_alive  = std::move(nodes_[node_id_value].alive);

            nodes_[node_id_value].points.clear();
            nodes_[node_id_value].alive.clear();

            // Convert to an internal node (internal nodes do not store points directly).
            nodes_[node_id_value].is_leaf = false;
            nodes_[node_id_value].dirty = false;
            nodes_[node_id_value].need_compact = false;

            // Precompute center to avoid recomputing it for every point in the loop.
            const auto split_center = nodes_[node_id_value].bounds.center();

            // Redistribute alive points to child nodes (children are created on demand).
            for (std::size_t i = 0; i < old_points.size(); ++i)
            {
                if (!old_alive[i]) continue;

                const PointT& p = old_points[i];
                const auto& pos = get_pos_(p);

                const std::size_t child_idx = childIndexForPosition(split_center, pos);
                node_id child_id = nodes_[node_id_value].children[child_idx];
                if (child_id == kInvalidNode)
                {
                    // Save bounds before allocateNode() potentially reallocates nodes_.
                    const box_type parent_bounds = nodes_[node_id_value].bounds;
                    child_id = allocateNode();
                    nodes_[node_id_value].children[child_idx] = child_id;

                    Node& child = nodes_[child_id];
                    child.bounds = computeChildBounds(parent_bounds, child_idx);
                    child.is_leaf = true;
                    child.depth = depth + 1;
                }

                Node& child = nodes_[child_id];
                child.points.push_back(p);
                child.alive.push_back(1);
                child.dirty = true;
            }
        }

        /**
         * @brief Recursive query helper for box range searches.
         *
         * Prunes subtrees whose bounds do not intersect @p region.
         *
         * @tparam Func Callable with signature: void(const PointT&).
         * @param node_id_value Root of the subtree to search.
         * @param region        The axis-aligned query box.
         * @param fn            Visitor invoked for each matching alive point.
         */
        template <class Func>
        void forEachPointInBoxRecursive(node_id node_id_value, const box_type& region, Func& fn) const
        {
            const Node& node = nodes_[node_id_value];
            if (!node.bounds.intersects(region))
                return;

            if (node.is_leaf)
            {
                // Skip per-point contains check when the leaf is fully inside the query region.
                const bool fully_inside = region.contains(node.bounds.min) && region.contains(node.bounds.max);
                for (std::size_t i = 0; i < node.points.size(); ++i)
                {
                    if (!node.alive[i]) continue;
                    if (fully_inside || region.contains(get_pos_(node.points[i])))
                        fn(node.points[i]);
                }
                return;
            }

            for (std::size_t i = 0; i < kChildCount; ++i)
            {
                node_id cid = node.children[i];
                if (cid == kInvalidNode) continue;
                forEachPointInBoxRecursive(cid, region, fn);
            }
        }

        /**
         * @brief Recursive soft-deletion helper.
         *
         * @tparam Predicate Callable: bool(const PointT&) — returns true for points to delete.
         * @tparam Prune     Callable: bool(const box_type&) — returns true when a node should
         *                   be visited (bounding-box early-out).
         * @param nid   Root of the subtree to process.
         * @param pred  Deletion predicate.
         * @param prune Pruning functor.
         * @return The number of points that were soft-deleted.
         */
        template <class Predicate, class Prune>
        std::size_t markDeletedRecursive(node_id nid, Predicate& pred, const Prune& prune)
        {
            Node& node = nodes_[nid];

            if (!prune(node.bounds))
                return 0;

            std::size_t removed = 0;

            if (node.is_leaf)
            {
                for (std::size_t i = 0; i < node.points.size(); ++i)
                {
                    if (!node.alive[i]) continue;
                    if (pred(node.points[i]))
                    {
                        node.alive[i] = 0;
                        node.need_compact = true;
                        node.dirty = true;
                        ++removed;
                    }
                }
                return removed;
            }

            for (std::size_t i = 0; i < kChildCount; ++i)
            {
                node_id cid = node.children[i];
                if (cid == kInvalidNode) continue;
                removed += markDeletedRecursive(cid, pred, prune);
            }
            return removed;
        }
    };  // class Orthtree

    /**
     * @brief Convenience alias for @c Orthtree backed by @c std::pmr::polymorphic_allocator.
     *
     * @c std::pmr::polymorphic_allocator<std::byte> is constructible from
     * @c std::pmr::memory_resource*, so existing construction code works unchanged:
     * @code
     *   std::pmr::unsynchronized_pool_resource pool;
     *   OrthtreePmr<Point3f, 3> tree(cfg, {}, &pool);
     * @endcode
     *
     * All APIs of @c Orthtree are available, including template @c insertMany,
     * template @c clearLeafDirtyFlags, and @c queryPointsInBox.
     *
     * @tparam PointT      Type of spatial points stored in the tree.
     * @tparam Dim         Number of spatial dimensions (must be >= 1).
     * @tparam Scalar      Scalar type used for bounding-box coordinates.
     * @tparam GetPosition Functor that extracts a position from a PointT value.
     */
    template <
        class PointT,
        std::size_t Dim,
        class Scalar      = float,
        class GetPosition = DefaultGetPosition<PointT>>
    using OrthtreePmr = Orthtree<
        PointT, Dim, Scalar, GetPosition,
        std::pmr::polymorphic_allocator<std::byte>>;

} // namespace lux::cxx
