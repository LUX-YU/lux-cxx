#pragma once

#include "tuple_traits.hpp"

/**
 * This file provides:
 *   1) dependency_map<...> to describe an edge from one node’s output to another’s input,
 *   2) helper meta-functions to check dependencies,
 *   3) topological_sort for a linear order,
 *   4) layered_topological_sort for a multi-level (layered) order.
 */
namespace lux::cxx
{
	/**
	 * Describes that TargetNode’s in<TargetInLoc> depends on SourceNode’s out<SourceOutLoc>.
	 * This is the external "edge" info for building the DAG of node dependencies.
	 */
	template <typename TargetNode, std::size_t TargetInLoc, typename SourceNode, std::size_t SourceOutLoc>
	struct dependency_map
	{
		using target_node_t = TargetNode;
		static constexpr std::size_t target_in_loc = TargetInLoc;

		using source_node_t = SourceNode;
		static constexpr std::size_t source_out_loc = SourceOutLoc;
	};


	//--------------------------------------------------------------------------
	// 1) Helper meta-functions to manipulate tuples and check membership
	//--------------------------------------------------------------------------

	// in_tuple<X, Tuple>: checks if X appears in Tuple<T...>.
	template <typename X, typename Tup>
	struct in_tuple;

	template <typename X>
	struct in_tuple<X, std::tuple<>> : std::false_type {};

	template <typename X, typename Head, typename...Tail>
	struct in_tuple<X, std::tuple<Head, Tail...>>
		: std::conditional_t<
		std::is_same_v<X, Head>,
		std::true_type,
		in_tuple<X, std::tuple<Tail...>>
		>
	{
	};

	template <typename X, typename Tup>
	static constexpr bool in_tuple_v = in_tuple<X, Tup>::value;


	// filter_tuple: keeps only elements in the tuple that satisfy a predicate Pred<T>::value
	template <typename Tuple, template <typename> class Pred>
	struct filter_tuple;

	template <template <typename> class Pred>
	struct filter_tuple<std::tuple<>, Pred>
	{
		using type = std::tuple<>;
	};

	template <typename Head, typename... Tail, template <typename> class Pred>
	struct filter_tuple<std::tuple<Head, Tail...>, Pred>
	{
	private:
		using tail_filtered = typename filter_tuple<std::tuple<Tail...>, Pred>::type;
		static constexpr bool keep = Pred<Head>::value;
	public:
		using type = std::conditional_t<
			keep,
			decltype(std::tuple_cat(std::declval<std::tuple<Head>>(), std::declval<tail_filtered>())),
			tail_filtered
		>;
	};

	template <typename Tup, template <typename> class Pred>
	using filter_tuple_t = typename filter_tuple<Tup, Pred>::type;

	//--------------------------------------------------------------------------
	// 2) node_dependencies: from a list of dependency_map, find the source nodes
	//    that a given NodeX depends on.
	//--------------------------------------------------------------------------

	template <typename NodeX, typename AllDeps>
	struct node_dependencies
	{
		// Pred that selects only dependency_map where target_node_t = NodeX
		template <typename MAP>
		struct depends_on_this {
			static constexpr bool value =
				std::is_same_v<typename MAP::target_node_t, NodeX>;
		};

		// Filter out all maps that belong to NodeX
		using filtered = filter_tuple_t<AllDeps, depends_on_this>;

		// For each map, we only care about the source_node_t
		template <typename MAP>
		struct map_to_source
		{
			using type = typename MAP::source_node_t;
		};

		template <typename> struct transform_to_sources;
		template <typename... Ms>
		struct transform_to_sources<std::tuple<Ms...>>
		{
			using type = std::tuple<typename map_to_source<Ms>::type...>;
		};

		// The final result is a tuple of node types that NodeX depends on
		using type = typename transform_to_sources<filtered>::type;
	};


	// checks if all dependencies of NodeT appear in the "Sorted" tuple
	template <typename NodeT, typename Sorted, typename AllDeps>
	struct dependencies_in_sorted
	{
		using deps_tuple = typename node_dependencies<NodeT, AllDeps>::type;

		template <typename... Ds>
		static constexpr bool checkAll(std::tuple<Ds...>)
		{
			return (... && in_tuple_v<Ds, Sorted>);
		}

		static constexpr bool value = checkAll(deps_tuple{});
	};


	//--------------------------------------------------------------------------
	// 3) Linear Topological Sort
	//--------------------------------------------------------------------------
	template <typename Unsorted, typename Sorted, typename AllDeps>
	struct topo_sort_impl
	{
	private:
		static constexpr bool empty = (std::tuple_size_v<Unsorted> == 0);

		// find the first node in Unsorted whose dependencies are satisfied
		template <typename Tup>
		struct find_satisfied;

		template <typename... Ns>
		struct find_satisfied<std::tuple<Ns...>>
		{
		private:
			template <typename N>
			static constexpr bool is_ok = dependencies_in_sorted<N, Sorted, AllDeps>::value;

			// pick_if utility
			template <bool C, typename T> struct pick_if { using type = void; };
			template <typename T> struct pick_if<true, T> { using type = T; };

			template <typename Found, typename... Ts>
			struct scan_list { using type = Found; };

			// recursively scan each node:
			// if is_ok<Head> is true, adopt Head if no prior Found
			template <typename Found, typename Head, typename...Tail>
			struct scan_list<Found, Head, Tail...>
			{
				static constexpr bool ok = is_ok<Head>;
				using maybe = typename pick_if<ok, Head>::type;
				using type = typename scan_list<
					std::conditional_t<std::is_void_v<Found>, maybe, Found>,
					Tail...
				>::type;
			};

		public:
			// the final type is the first node meeting is_ok
			using type = typename scan_list<void, Ns...>::type;
		};

		using found = typename find_satisfied<Unsorted>::type;
		static constexpr bool found_any = !std::is_void_v<found>;

		// remove that node from Unsorted, append to Sorted
		using next_unsorted = tuple_remove_t<Unsorted, found>;
		using next_sorted = tuple_append_t<Sorted, found>;

		using step_case = typename topo_sort_impl<next_unsorted, next_sorted, AllDeps>::type;
	public:
		using type = std::conditional_t<
			empty,
			Sorted,
			std::conditional_t<found_any, step_case, std::false_type>
		>;

		static_assert(!std::is_same_v<type, std::false_type>,
			"Topological sort failed: cycle or unresolved dependencies.");
	};

	// base-case: Unsorted is empty => done
	template <typename Sorted, typename AllDeps>
	struct topo_sort_impl<std::tuple<>, Sorted, AllDeps>
	{
		using type = Sorted;
	};

	// The main interface for linear topological sort
	template <typename NodeTuple, typename AllDeps>
	struct topological_sort
	{
		using type = typename topo_sort_impl<NodeTuple, std::tuple<>, AllDeps>::type;
	};


	//--------------------------------------------------------------------------
	// 4) Layered Topological Sort
	//    Each layer is a set of nodes that can run in parallel
	//--------------------------------------------------------------------------
	template <typename Unsorted, typename LayersSoFar, typename AllDeps>
	struct layered_topo_sort_impl
	{
	private:
		static constexpr bool empty = (std::tuple_size_v<Unsorted> == 0);

		// Flatten current layers to see which nodes are already "done"
		// so we can check if dependencies are satisfied.
		template <typename T> struct flatten_tuple_of_tuples;
		template <typename... Ts>
		struct flatten_tuple_of_tuples<std::tuple<Ts...>>
		{
		private:
			// If Ts... are sub-tuples, we can do a big tuple_cat
			template <typename U> struct identity { using type = U; };

			static auto catAll(Ts...)
				-> decltype(std::tuple_cat(typename identity<Ts>::type{}...));
		public:
			using type = decltype(catAll(std::declval<Ts>()...));
		};

		using FlattenedSoFar = typename flatten_tuple_of_tuples<LayersSoFar>::type;


		// select next group: all nodes whose dependencies are in FlattenedSoFar
		template <typename N>
		struct can_run_now {
			static constexpr bool value = dependencies_in_sorted<N, FlattenedSoFar, AllDeps>::value;
		};

		// filter out nodes that can run
		using next_group = filter_tuple_t<Unsorted, can_run_now>;
		static constexpr bool group_empty = (std::tuple_size_v<next_group> == 0);

		// remove these nodes from unsorted
		// we only handle each node once
		template <typename Rem, typename T>
		struct remove_all;

		template <typename Rem>
		struct remove_all<Rem, std::tuple<>>
		{
			using type = std::tuple<>;
		};

		template <typename Rem, typename Head, typename...Tail>
		struct remove_all<Rem, std::tuple<Head, Tail...>>
		{
		private:
			static constexpr bool in_rem = in_tuple_v<Head, Rem>;
			using tail_removed = typename remove_all<Rem, std::tuple<Tail...>>::type;
		public:
			using type = std::conditional_t<
				in_rem,
				tail_removed,
				decltype(std::tuple_cat(std::declval<std::tuple<Head>>(), std::declval<tail_removed>()))
			>;
		};

		using next_unsorted = typename remove_all<next_group, Unsorted>::type;
		static constexpr bool have_unsorted = (std::tuple_size_v<Unsorted> != 0);

		// append next_group as a new layer
		using appended = decltype(std::tuple_cat(std::declval<LayersSoFar>(), std::declval<std::tuple<next_group>>()));

		using step_case = typename layered_topo_sort_impl<next_unsorted, appended, AllDeps>::type;
	public:
		using type = std::conditional_t<
			empty,
			LayersSoFar,
			std::conditional_t<(group_empty&& have_unsorted), std::false_type, step_case>
		>;

		static_assert(!std::is_same_v<type, std::false_type>,
			"Layered topological sort failed: cycle or unresolved dependencies.");
	};

	// base-case: no unsorted => done
	template <typename LayersSoFar, typename AllDeps>
	struct layered_topo_sort_impl<std::tuple<>, LayersSoFar, AllDeps>
	{
		using type = LayersSoFar;
	};

	// The main interface for layered topological sort
	template <typename NodeTuple, typename AllDeps>
	struct layered_topological_sort
	{
		using type = typename layered_topo_sort_impl<NodeTuple, std::tuple<>, AllDeps>::type;
	};
} // namespace lux
