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

#include "tuple_traits.hpp"
#include <array>

/**
 * This file defines fundamental node concepts:
 *   1) in_binding / out_binding: to declare input or output of a node,
 *   2) node_descriptor: aggregates multiple bindings,
 *   3) NodeBase: a CRTP base class for nodes, holding input/output descriptors.
 */
namespace lux::cxx
{
	// Represents a node input binding with location (Loc) and type (T).
	template <std::size_t Loc, typename T>
	struct in_binding
	{
		static constexpr std::size_t location = Loc;
		using value_t = T;
	};

	// Represents a node output binding with location (Loc) and type (T).
	template <std::size_t Loc, typename T>
	struct out_binding
	{
		static constexpr std::size_t location = Loc;
		using value_t = T;
	};

	// A descriptor holding a tuple of bindings (either all inputs or all outputs).
	template <typename... Bindings>
	struct node_descriptor
	{
		using bindings_tuple_t = std::tuple<Bindings...>;
	};

	// Converts a node_descriptor into a tuple of reference types, e.g. (T1&, T2&, ...).
	// This is used for passing references into the node’s execution function.
	template<typename Descriptor>
	class descriptor_to_params
	{
	private:
		static constexpr std::size_t N = std::tuple_size_v<typename Descriptor::bindings_tuple_t>;

		template<size_t... I>
		static constexpr auto to_tuple(std::index_sequence<I...>)
			-> std::tuple<typename std::tuple_element_t<I, typename Descriptor::bindings_tuple_t>::value_t&...>;

	public:
		using type = decltype(to_tuple(std::make_index_sequence<N>{}));
	};

	// Extracts all 'location' fields from a node_descriptor into a static array.
	template<typename Descriptor>
	class descriptor_to_locations
	{
	private:
		static constexpr std::size_t N = std::tuple_size_v<typename Descriptor::bindings_tuple_t>;

		template<size_t... I>
		static constexpr auto to_array(std::index_sequence<I...>)
		{
			return std::array<size_t, N>{
				std::tuple_element_t<I, typename Descriptor::bindings_tuple_t>::location...
			};
		}

	public:
		static constexpr std::array<size_t, N> loc_seq
			= to_array(std::make_index_sequence<N>{});
	};

	/**
	 * NodeBase:
	 *   A CRTP base class for nodes. Users derive from NodeBase<Derived, InDesc, OutDesc>
	 *   and implement:
	 *       bool Derived::execute(const in_param_t& in, out_param_t& out)
	 */
	template<typename Derived,
		typename InDesc,
		typename OutDesc>
	struct NodeBase
	{
		using input_descriptor = InDesc;
		using output_descriptor = OutDesc;

		// in_param_t/out_param_t: e.g. (int&, double&) for inputs, (float&, bool&) for outputs
		using in_param_t = typename descriptor_to_params<InDesc>::type;
		using out_param_t = typename descriptor_to_params<OutDesc>::type;

		// Arrays of "location" extracted from each binding
		static constexpr auto in_loc_seq = descriptor_to_locations<InDesc>::loc_seq;
		static constexpr auto out_loc_seq = descriptor_to_locations<OutDesc>::loc_seq;

		// Default execute() method calls Derived::execute(in, out).
		bool execute(const in_param_t& in, out_param_t& out)
		{
			return static_cast<Derived*>(this)->execute(in, out);
		}
	};
} // namespace lux
