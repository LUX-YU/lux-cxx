#pragma once
#include <vector>
#include <unordered_map>
#include <variant>
#include <string>
#include <cstdint>
#include <cstddef>
#include <string_view>
#include <charconv>
#include <stdexcept>
#include "lux/cxx/container/HeterogenousLookup.hpp"
#include "lux/cxx/compile_time/expected.hpp"

namespace lux::cxx
{
	using VSupportedArgTypes = std::variant <
		int8_t,
		int16_t,
		int32_t,
		int64_t,
		uint8_t,
		uint16_t,
		uint32_t,
		uint64_t,
		float,
		double,
		std::string_view,

		std::vector<int8_t>,
		std::vector<int16_t>,
		std::vector<int32_t>,
		std::vector<int64_t>,
		std::vector<uint8_t>,
		std::vector<uint16_t>,
		std::vector<uint32_t>,
		std::vector<uint64_t>,
		std::vector<float>,
		std::vector<double>,
		std::vector<std::string_view>
	> ;

	template<typename T>
	concept IsVector = requires(T t) {
		typename T::value_type;
		requires std::is_same_v<T, std::vector<typename T::value_type>>;
	};

	template<typename T>
	concept IsString = std::is_same_v<T, std::string>;

	template<typename T>
	concept IsArithmetic = std::is_arithmetic_v<T>;

	template<typename T>
	concept IsStringView = std::is_same_v<T, std::string_view>;

	class Argument
	{
		friend class ArgumentParser;
		template<typename T> friend class TArgument;
	public:
		Argument() = default;

		Argument(std::string short_name, std::string long_name)
			: _short_name(std::move(short_name)), _long_name(std::move(long_name)) { }

	private:

		template<typename T>
		static void convertVector(std::vector<std::string_view> inputs, VSupportedArgTypes& value) {
			std::vector<T> vec;
			for (const auto& input : inputs) {
				T elem;
				auto result = std::from_chars(input.data(), input.data() + input.size(), elem);
				if (result.ec == std::errc()) {
					vec.push_back(elem);
				}
				else {
					throw std::runtime_error("Conversion failed for vector element");
				}
			}
			value = std::move(vec);
		}

		template<typename T>
		static void convertArithmetic(std::vector<std::string_view> inputs, VSupportedArgTypes& value)
		{
			T num;
			auto result = std::from_chars(inputs.front().data(), inputs.front().data() + inputs.front().size(), num);
			if (result.ec == std::errc()) {
				value = num;
			}
			else {
				throw std::runtime_error("Conversion failed for numeric type");
			}
		}

		template<typename T>
		Argument& setConverter() {
			if constexpr (IsVector<T>) {
				_convert_and_assign = Argument::convertVector<typename T::value_type>;
				_multiple_values    = true;
			}
			else if constexpr (IsArithmetic<T>) {
				_convert_and_assign = Argument::convertArithmetic<T>;
			}
			else if constexpr (IsStringView<T>) {
				_convert_and_assign = [](const std::vector<std::string_view>& s, VSupportedArgTypes& v) {
					v = s.front();
				};
			}
			return *this;
		}

		using AssignFunc = std::function<void(const std::vector<std::string_view>&, VSupportedArgTypes&)>;

		VSupportedArgTypes		_value;
		AssignFunc				_convert_and_assign;
		std::string				_description{ "" };
		std::string				_short_name;
		std::string				_long_name;
		bool					_multiple_values{ false };
		bool					_required{ false };
	};

	template<typename T>
	class TArgument
	{
	public:
		TArgument(Argument& arg) : _arg(arg) {}

		TArgument& setRequired(bool required)
		{
			_arg._required = required;
			return *this;
		}

		TArgument& setDescription(std::string description)
		{
			_arg._description = description;
			return *this;
		}

		TArgument& setDefaultValue(const T& default_value)
		{
			_arg._value = default_value;
			return *this;
		}

	private:
		Argument& _arg;
	};

	enum class EArgumentParseError
	{
		SUCCESS,
		ARGUMENT_NOT_FOUND,
		ARGUMENT_TYPE_MISMATCH,
	};

	class ArgumentParser
	{
	public:
		ArgumentParser(bool allow_unrecognized = false)
			: _allow_unrecognized(allow_unrecognized) { }

		template<typename T>
		TArgument<T> addArgument(std::string short_name, std::string long_name)
		{
			Argument arg{ std::move(short_name), std::move(long_name) };
			arg.setConverter<T>();
			if (!arg._short_name.empty()) {
				_short_name_map[arg._short_name] = arg._long_name;
			}
			return TArgument<T>{_arguments[arg._long_name] = std::move(arg)};
		}

		template<typename T>
		TArgument<T> addArgument(std::string long_name) {
			return addArgument<T>("", std::move(long_name));
		}

		template<typename T>
		::lux::cxx::expected<T, EArgumentParseError> getArgument(const std::string& name)
		{
			if (_arguments.find(name) != _arguments.end())
			{
				return handle_argument<T>(_arguments[name]);
			}
			else if (_short_name_map.find(name) != _short_name_map.end())
				return handle_argument<T>(_arguments[_short_name_map[name]]);
			else
				return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_NOT_FOUND);
		}

		::lux::cxx::expected<void, EArgumentParseError> parse(int argc, char* argv[])
		{
			std::unordered_map<std::string_view, std::vector<std::string_view>> parsed_args;

			for (int i = 1; i < argc; ++i)
			{
				std::string_view key(argv[i]);

				if (key[0] == '-') // check if it's a flag
				{
					// Remove leading dashes for uniformity
					if (key[1] == '-')
					{
						// long name
						key.remove_prefix(2);
					}
					else
					{
						// short name
						key.remove_prefix(1);
						// get long name from short name
						auto iter = _short_name_map.find(key);
						if (iter == _short_name_map.end() && !_allow_unrecognized)
						{
							return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_NOT_FOUND);	
						}
						else
						{
							key = iter->second;
						}
					}

					if(auto iter = _arguments.find(key); iter == _arguments.end() && !_allow_unrecognized)
					{
						return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_NOT_FOUND);
					}

					// Support for flags without values or with multiple values
					if (i + 1 < argc && argv[i + 1][0] != '-')
					{
						++i;
						while (i < argc && argv[i][0] != '-') // support for multiple values
						{
							parsed_args[key].push_back(argv[i]);
							if (auto iter = _arguments.find(key); iter != _arguments.end())
							{
								if (!iter->second._multiple_values) break;
							}
							++i;
						}
						--i; // adjust index because the for loop will increment it
					}
					else
					{
						// This means it's a flag-type argument that doesn't require values, 
						// we add an empty vector
						parsed_args[key];
					}
				}
			}

			// Assign values to arguments or report errors
			for (auto& [key, arg] : _arguments) {
				if (parsed_args.find(key) != parsed_args.end()) {
					try {
						arg._convert_and_assign(parsed_args[key], arg._value);
					}
					catch (const std::exception& e) {
						return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_TYPE_MISMATCH);
					}
				}
				else if (arg._required) {
					return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_NOT_FOUND);
				}
			}

			return ::lux::cxx::expected<void, EArgumentParseError>{};
		}

	private:
		template<typename T>
		::lux::cxx::expected<T, EArgumentParseError> handle_argument(Argument& arg)
		{
			if constexpr (std::is_same_v<T, std::string>) {
				if (auto pval = std::get_if<std::string_view>(&arg._value)) {
					return std::string(*pval);
				}
				else {
					return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_TYPE_MISMATCH);
				}
			}
			else {
				if (auto pval = std::get_if<T>(&arg._value)) {
					return *pval;
				}
				else {
					return ::lux::cxx::unexpected<EArgumentParseError>(EArgumentParseError::ARGUMENT_TYPE_MISMATCH);
				}
			}
		}

		bool	                       _allow_unrecognized{ false };
		heterogenouts_map<Argument>    _arguments;
		heterogenouts_map<std::string> _short_name_map;
	};
}
