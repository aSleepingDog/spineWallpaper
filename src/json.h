#pragma once
#include <math.h>
#include <stack>
#include <array>
#include <vector>
#include <string>
#include <variant>
#include <iostream>
#include <unordered_map>

namespace json
{
	enum class Type : uint8_t
	{
		Undefined = 0,
		Null = 1,
		Bool = 2,
		Number = 3,
		String = 4,
		Array = 5,
		Object = 6
	};

	class Value;
	class Object;
	class Array;

	void skip_whitespace(std::string_view::iterator& it, const std::string_view::iterator& end);

	class Value
	{
	private:
		std::variant<
			std::nullptr_t,
			bool,
			double,
			std::string,
			std::vector<Value>,
			std::unordered_map<std::string, Value>
		> value_;
    public:
        Value();
        Value(std::nullptr_t);
        Value(bool value);
        Value(double value);

		Value(std::string_view value);
		Value(const std::string& value);
		Value(const char* value);

		Value(std::string&& other) noexcept;

		Value(const std::vector<Value>& other);
		Value(std::vector<Value>&& other) noexcept;

		Value(const std::unordered_map<std::string, Value>& other);
		Value(std::unordered_map<std::string, Value>&& other) noexcept;

		Value(const Value& other);
		Value& operator=(const Value& other);
		Value(Value&& other) noexcept;
		Value& operator=(Value&& other) noexcept;

		static Value parse(std::string_view::iterator& pos, const std::string_view::iterator& end);
		static Value parse(std::string_view str);
		static Value parse(const std::string& str);
		static Value parse(const char* str);

		Type get_type() const;

		std::nullptr_t to_null() const;
		bool to_bool() const;
		double to_number() const;
		const std::string& to_string() const;
		const std::vector<Value>& to_array() const;
		const std::unordered_map<std::string, Value>& to_object() const;

		bool& to_bool();
		double& to_number();
		std::string& to_string();
		std::vector<Value>& to_array();
		std::unordered_map<std::string, Value>& to_object();

		std::string to_json_string(uint64_t depth = 0, uint64_t count = 4, char fill = ' ') const;

		bool operator==(const Value& other) const;
		bool operator!=(const Value& other) const;
	};

	class Object
	{
	private:
		using it = std::unordered_map<std::string, Value>::iterator;
		using cit = std::unordered_map<std::string, Value>::const_iterator;
		using value_type = std::unordered_map<std::string, Value>::value_type;
		using size_type = std::unordered_map<std::string, Value>::size_type;

		std::unordered_map<std::string, Value> object_;
	public:
		Object();
		Object(const std::unordered_map<std::string, Value>& other);
		Object(std::unordered_map<std::string, Value>&& other) noexcept;

		Object(const Object& other);
		Object operator=(const Object& other);
		Object(Object&& other) noexcept;
		Object operator=(Object&& other) noexcept;

		static std::unordered_map<std::string, Value> parse_std(std::string_view::iterator& pos, const std::string_view::iterator& end);
		static std::unordered_map<std::string, Value> parse_std(std::string_view str);
		static std::unordered_map<std::string, Value> parse_std(const std::string& str);
		static std::unordered_map<std::string, Value> parse_std(const char* str);

		static Object parse(std::string_view str);
		static Object parse(const std::string& str);
		static Object parse(const char* str);

		std::unordered_map<std::string, Value>& to_unordered_map();
		const std::unordered_map<std::string, Value>& to_unordered_map() const;
		std::string to_json_string(uint64_t depth = 0, uint64_t count = 4, char fill = ' ');
		static std::string object_to_json_string(const std::unordered_map<std::string, Value>& object, uint64_t depth = 0, uint64_t count = 4, char fill = ' ');

		it begin();
		cit cbegin() const;
		it end();
		cit cend() const;

		bool empty() const;
		uint64_t size() const;
		uint64_t max_size() const;

		void clear();
		std::pair<it, bool> insert(const value_type& pair);
		std::pair<it, bool> insert(value_type&& pair) noexcept;
		it insert(cit pos, const value_type& pair);
		it insert(cit pos, value_type&& pair) noexcept;
		void insert(std::initializer_list<value_type> ilist);

		std::pair<it, bool> insert_or_assign(const std::string& key, const Value& value);
		std::pair<it, bool> insert_or_assign(std::string&& key, const Value& value);
		it insert_or_assign(cit hint, const std::string& k, Value&& obj);
		it insert_or_assign(cit hint, std::string&& k, Value&& obj);

		it erase(it pos);
		it erase(cit pos);
		it erase(cit start, cit end);
		size_type erase(const std::string& key);

		void swap(Object& other) noexcept;

		Value& operator[](const std::string& key);
		Value& operator[](std::string&& key);

		Value& at(const std::string& key);
		const Value& at(const std::string& key) const;

		size_type count(const std::string& key) const;

		it find(const std::string& key);
		cit find(const std::string& key) const;

		bool contains(const std::string& key) const;

		Type get_type(const std::string& str) const;
	};

	class Array
	{
	private:
		using it = std::vector<Value>::iterator;
		using cit = std::vector<Value>::const_iterator;
		using rit = std::vector<Value>::reverse_iterator;
		using crit = std::vector<Value>::const_reverse_iterator;

		std::vector<Value> array_;
	public:
		Array();
		Array(const std::vector<Value>& array);
		Array(std::vector<Value>&& array);

		Array(const Array& other);
		Array operator=(const Array& other);
		Array(Array&& other) noexcept;
		Array operator=(Array&& other) noexcept;

		static std::vector<Value> parse_std(std::string_view::iterator& pos, const std::string_view::iterator& end);
		static std::vector<Value> parse_std(std::string_view str);
		static std::vector<Value> parse_std(const std::string& str);
		static std::vector<Value> parse_std(const char* str);

		static Array parse(std::string_view str);
		static Array parse(const std::string& str);
		static Array parse(const char* str);

		std::vector<Value>& to_vector();
		const std::vector<Value>& to_vector() const;

		std::string to_json_string(uint64_t depth = 0, uint64_t count = 4, char fill = ' ');
		static std::string array_to_json_string(const std::vector<Value>& array, uint64_t depth = 0, uint64_t count = 4, char fill = ' ');

		void assign(uint64_t count, const Value& elem);
		std::vector<uint8_t>::allocator_type get_allocator() const;

		Value& operator[](uint64_t index);
		const Value& operator[](uint64_t index) const;
		Value& at(uint64_t index);
		const Value& at(uint64_t index) const;
		Value& front();
		const Value& front() const;
		Value& back();
		const Value& back() const;

		it begin();
		cit cbegin() const;
		it end();
		cit cend() const;
		rit rbegin();
		crit crbegin() const;
		crit rend();
		crit crend() const;

		uint64_t size() const;
		bool empty() const;
		uint64_t max_size() const;
		void reserve(uint64_t count);
		uint64_t capacity() const;
		void shrink_to_fit();

		void clear();
		it insert(it pos, const Value& elem);
		it insert(it pos, Value&& elem);
		it insert(it pos, uint64_t count, const Value& elem);
		it insert(it pos, uint64_t count, Value&& elem);
		it insert(it pos, std::initializer_list<Value> ilist);

		it emplace(it pos, const Value& elem);
		it emplace(it pos, Value&& elem);

		it erase(it pos);
		it erase(cit pos);
		it erase(it first, it last);
		it erase(cit first, cit last);

		void push_back(const Value& elem);
		void push_back(Value&& elem);

		void pop_back();

		void resize(uint64_t count);
		void resize(uint64_t count, const Value& elem);

		void swap(Array& other);

		bool operator==(const Array& other) const;
		bool operator!=(const Array& other) const;
	};

	template<Type T,typename... Args>
	struct Param {};

	template<>
	struct Param<Type::Bool, std::false_type>
	{
		static constexpr Type t = Type::Bool;
		static bool check(bool b)
		{
			return b == false;
		}
	};
	template<>
	struct Param<Type::Bool, std::true_type>
	{
		static constexpr Type t = Type::Bool;
		static bool check(bool b)
		{
			return b == true;
		}
	};

	template<typename T>
	struct is_param_s : std::false_type {};	
	template<Type T, typename... Args>
	struct is_param_s<Param<T, Args...>> : std::true_type {};
	template<typename T>
	concept is_param = is_param_s<T>::value;

	template<auto Callable>
	struct TpFunc 
	{
		template<typename... Args>
		static decltype(auto) call(Args&&... args)
		{
			return std::invoke(Callable, std::forward<Args>(args)...);
		}
	};

	template<auto Callable>
	struct Param<Type::Number, TpFunc<Callable>>
	{
		static constexpr Type t = Type::Number;
		static bool check(double v)
		{
			return TpFunc<Callable>::call(v);
		}
	};

	template<>
	struct Param<Type::Number>
	{
		static constexpr Type t = Type::Number;
		static bool check(double v)
		{
			return true;
		}
	};

	template<auto Callable>
	struct Param<Type::String, TpFunc<Callable>>
	{
		static constexpr Type t = Type::String;
		static bool check(const std::string& v)
		{
			return TpFunc<Callable>::call(v);
		}
	};

	template<>
	struct Param<Type::String>
	{
		static constexpr Type t = Type::String;
		static bool check(const std::string& v)
		{
			return true;
		}
	};

	template<is_param... ITRMS>
	struct ArrayParams
	{
		static bool check(const std::vector<Value>& v)
		{
			constexpr size_t SIZE = sizeof...(ITRMS);
			if (v.size() != SIZE)
			{
				return false;
			}
			bool result = [&]<size_t... I>(std::index_sequence<I...>)
			{
				return (ITRMS::check(v[I]) && ...);
			}(std::index_sequence_for<ITRMS...>{});
			return result;
		}
	};

	template<is_param... ITRMS>
	struct Param<Type::Array, ArrayParams<ITRMS...>>
	{
		static constexpr Type t = Type::Array;
		static bool check(const std::vector<Value>& v)
		{
			return ArrayParams<ITRMS...>::check(v);
		}
	};

	template<size_t N>
	struct TpString
	{
		size_t size;
		char str[N];
		constexpr TpString(const char(&s)[N])
		{
			size = N - 1;
			std::copy_n(s, N, str);
		}
	};

	template<TpString N,is_param P>
	struct ObjectParam
	{
		static constexpr TpString<N.size + 1> key = N;
		static bool check_v(const Value& v)
		{
			return check<P>(v);
		}
	};

	template<typename Item>
	static bool check_item(const std::unordered_map<std::string, Value>& v)
	{
		std::string key;
		for (uint64_t i = 0; i < Item::key.size; ++i)
		{
			key += Item::key.str[i];
		}
		if (!v.contains(key)) return false;
		return Item::check_v(v.at(key));
	}

	template<typename... ITEMS>
	struct ObjectParams
	{
		static bool check(const std::unordered_map<std::string, Value>& v)
		{
			return (check_item<ITEMS>(v) && ...);
		}
	};

	template<typename... ITRMS>
	struct Param<Type::Object, ObjectParams<ITRMS...>>
	{
		static constexpr Type t = Type::Object;
		static bool check(const std::unordered_map<std::string, Value>& v)
		{
			return ObjectParams<ITRMS...>::check(v);
		}
	};

	template<>
	struct Param<Type::Object>
	{
		static constexpr Type t = Type::Object;
		static bool check(const std::unordered_map<std::string, Value>& v)
		{
			return true;
		}
	};

	template<is_param T>
	bool check(const Value& v)
	{
		if (v.get_type() != T::t)
		{
			return false;
		}
		if constexpr (T::t == Type::Null)
		{
			return true;
		}
		else if constexpr (T::t == Type::Bool)
		{
			return T::check(v.to_bool());
		}
		else if constexpr (T::t == Type::Number)
		{
			return T::check(v.to_number());
		}
		else if constexpr (T::t == Type::String)
		{
			return T::check(v.to_string());
		}
		else if constexpr (T::t == Type::Array)
		{
			return T::check(v.to_array());
		}
		else if constexpr (T::t == Type::Object)
		{
			return T::check(v.to_object());
		}
	}


}