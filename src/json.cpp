#include "json.h"

namespace NSROOT = json;

std::string to_utf8(uint64_t code)
{
	std::string result = "";
	if (code <= 0x7F)
	{
		result += (char)code;
	}
	else if (code <= 0x7FF)
	{
		result += (char)(0b11000000 | ((code >> 06) & 0x1F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	else if (code <= 0xFFFF)
	{
		result += (char)(0b11100000 | ((code >> 12) & 0x0F));
		result += (char)(0b10000000 | ((code >> 06) & 0x3F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	else if (code <= 0x1FFFFF)
	{
		result += (char)(0b11110000 | ((code >> 18) & 0x07));
		result += (char)(0b10000000 | ((code >> 12) & 0x3F));
		result += (char)(0b10000000 | ((code >> 06) & 0x3F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	else if (code <= 0x3FFFFFF)
	{
		result += (char)(0b11111000 | ((code >> 24) & 0x03));
		result += (char)(0b10000000 | ((code >> 18) & 0x3F));
		result += (char)(0b10000000 | ((code >> 12) & 0x3F));
		result += (char)(0b10000000 | ((code >> 06) & 0x3F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	else if (code <= 0x7FFFFFFF)
	{
		result += (char)(0b11111100 | ((code >> 30) & 0x01));
		result += (char)(0b10000000 | ((code >> 24) & 0x3F));
		result += (char)(0b10000000 | ((code >> 18) & 0x3F));
		result += (char)(0b10000000 | ((code >> 12) & 0x3F));
		result += (char)(0b10000000 | ((code >> 06) & 0x3F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	else if (code <= 0xFFFFFFFF)
	{
		result += (char)(0b11111100 | ((code >> 36) & 0x01));
		result += (char)(0b10000000 | ((code >> 30) & 0x3F));
		result += (char)(0b10000000 | ((code >> 24) & 0x3F));
		result += (char)(0b10000000 | ((code >> 18) & 0x3F));
		result += (char)(0b10000000 | ((code >> 12) & 0x3F));
		result += (char)(0b10000000 | ((code >> 06) & 0x3F));
		result += (char)(0b10000000 | ((code >> 00) & 0x3F));
	}
	return result;
}

void NSROOT::skip_whitespace(std::string_view::const_iterator& it, const std::string_view::const_iterator& end)
{
	uint8_t stage = 0;
	while (it != end)
	{
		if (stage == 0)
		{
			if (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r')
			{
				++it;
			}
			else if (*it == '/')
			{
				++it;
				if (it != end && *it == '/')
				{
					stage = 1;
					++it;
				}
				else if (it != end && *it == '*')
				{
					stage = 2;
					++it;
				}
				else throw std::runtime_error("json error");
			}
			else
			{
				break;
			}
		}
		else if (stage == 1)
		{
			if (*it == '\n' || *it == '\r')
			{
				stage = 0;
			}
			++it;
		}
		else if (stage == 2)
		{
			if (*it == '*')
			{
				++it;
				if (it != end && *it == '/')
				{
					++it;
					stage = 0;
				}
				else if (it != end)
				{
					++it;
				}
				else throw std::runtime_error("json error");
			}
			else if (it != end)
			{
				++it;
			}
			else throw std::runtime_error("json error");
		}
	}
}

NSROOT::Value::Value()
{
	this->value_ = nullptr;
}
NSROOT::Value::Value(std::nullptr_t)
{
	this->value_ = nullptr;
}
NSROOT::Value::Value(bool value)
{
	this->value_ = value;
}
NSROOT::Value::Value(double value)
{
	this->value_ = value;
}
NSROOT::Value::Value(std::string_view value)
{
	this->value_ = std::string(value);
}
NSROOT::Value::Value(const std::string& value)
{
	this->value_ = value;
}
NSROOT::Value::Value(const char* value)
{
	this->value_ = std::string(value);
}
NSROOT::Value::Value(std::string&& other) noexcept
{
	this->value_ = std::move(other);
}
NSROOT::Value::Value(const std::vector<Value>& other)
{
	this->value_ = std::vector<Value>(other);
}
NSROOT::Value::Value(std::vector<Value>&& other) noexcept
{
	this->value_ = std::move(other);
}
NSROOT::Value::Value(const std::unordered_map<std::string, Value>&other)
{
	this->value_ = std::unordered_map<std::string, Value>(other);
}
NSROOT::Value::Value(std::unordered_map<std::string, Value>&& other) noexcept
{
	this->value_ = std::move(other);
}


NSROOT::Value::Value(const Value& other)
{
	this->value_ = other.value_;
}
NSROOT::Value& NSROOT::Value::operator=(const Value& other)
{
	this->value_ = other.value_;
	return *this;
}
NSROOT::Value::Value(Value&& other) noexcept
{
	this->value_ = std::move(other.value_);
}
NSROOT::Value& NSROOT::Value::operator=(Value&& other) noexcept
{
	if (this == &other) return *this;
	this->value_ = std::move(other.value_);
	return *this;
}

NSROOT::Value NSROOT::Value::parse(std::string_view::const_iterator& pos, const std::string_view::const_iterator& end)
{
	skip_whitespace(pos, end);
	if (pos == end)
	{
		throw std::runtime_error("json error");
	}
	else if (*pos == 'n')//null
	{
		++pos;
		if (pos == end || *pos != 'u') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'l') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'l') throw std::runtime_error("json error");
		++pos;
		return Value(nullptr);
	}
	else if (*pos == 't')//true
	{
		++pos;
		if (pos == end || *pos != 'r') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'u') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'e') throw std::runtime_error("json error");
		++pos;
		return Value(true);
	}
	else if (*pos == 'f')//false
	{
		++pos;
		if (pos == end || *pos != 'a') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'l') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 's') throw std::runtime_error("json error");
		++pos;
		if (pos == end || *pos != 'e') throw std::runtime_error("json error");
		++pos;
		return Value(false);
	}
	else if ((*pos >= '0' && *pos <= '9') || *pos == '-')//number
	{
		auto s = pos;
		if (*pos == '-')
		{
			++pos;
		}
		if (pos == end || *pos < '0' || *pos > '9')
		{
			throw std::runtime_error("json error");
		}
		if (*pos == '0')
		{
			++pos;
		}
		else
		{
			while (pos != end && *pos >= '0' && *pos <= '9')
			{
				++pos;
			}
		}
		if (pos == end || *pos != '.')
		{
			std::string str(s, pos);
			return Value(std::atof(str.c_str()));
		}
		++pos;

		while (pos != end && *pos >= '0' && *pos <= '9')
		{
			++pos;
		}
		if (pos == end || *pos != 'e' || *pos != 'E')
		{
			std::string str(s, pos);
			return Value(std::atof(str.c_str()));
		}
		++pos;

		if (pos == end || *pos != '+' || *pos != '-')
		{
			throw std::runtime_error("json error");
		}
		if (*pos == '-' || *pos == '+')
		{
			++pos;
		}
		if (pos == end || *pos < '0' || *pos > '9')
		{
			throw std::runtime_error("json error");
		}
		while (pos != end && *pos >= '0' && *pos <= '9')
		{
			++pos;
		}
		std::string str(s, pos);
		return Value(std::atof(str.c_str()));
	}
	else if (*pos == '"')//string
	{
		++pos;
		uint8_t stage = 0;
		std::string result;
		while (pos != end && stage != 0 || *pos != '"')
		{
			if (stage == 0 && *pos != '\\')
			{
				result += *pos;
				++pos;
			}
			else if (stage == 0 && *pos == '\\')
			{
				stage = 1;
				++pos;
			}
			else if (stage == 1 && *pos != 'u')
			{
				if (*pos == '"')
				{
					result += '"';
				}
				else if (*pos == '\\')
				{
					result += '\\';
				}
				else if (*pos == '/')
				{
					result += '/';
				}
				else if (*pos == 'b')
				{
					result += '\b';
				}
				else if (*pos == 'f')
				{
					result += '\f';
				}
				else if (*pos == 'n')
				{
					result += '\n';
				}
				else if (*pos == 'r')
				{
					result += '\r';
				}
				else if (*pos == 't')
				{
					result += '\t';
				}
				else
				{
					throw std::runtime_error("json error");
				}
				stage = 0;
				++pos;
			}
			else if (stage == 1 && *pos == 'u')
			{
				++pos;
				uint64_t code = 0;
				uint8_t time = 0;
				while (pos != end && ((*pos >= '0' && *pos <= '9') || (*pos >= 'a' && *pos <= 'f') || (*pos >= 'A' && *pos <= 'F')) && time < 16)
				{
					code <<= 4;
					if (*pos >= '0' && *pos <= '9')
					{
						code += (*pos - '0');
					}
					else if (*pos >= 'a' && *pos <= 'f')
					{
						code += (*pos - 'a' + 10);
					}
					else if (*pos >= 'A' && *pos <= 'F')
					{
						code += (*pos - 'A' + 10);
					}
					++pos;
					time++;
				}
				result += to_utf8(code);
				stage = 0;
			}
		}
		++pos;
		return Value(result);
	}
	else if (*pos == '[')//array
	{
		return Value(Array::parse_std(pos, end));
	}
	else if (*pos == '{')//object
	{
		return Value(Object::parse_std(pos, end));
	}
	else
	{
		throw std::runtime_error("json error");
	}
}
NSROOT::Value NSROOT::Value::parse(std::string_view str)
{
	auto it = str.begin();
	auto end = str.end();
	return parse(it, end);
}
NSROOT::Value NSROOT::Value::parse(const std::string& str)
{
	return parse(std::string_view(str));
}
NSROOT::Value NSROOT::Value::parse(const char* str)
{
	return parse(std::string_view(str));
}
NSROOT::Type NSROOT::Value::get_type() const
{
	return (NSROOT::Type)(this->value_.index() + 1);
}
std::nullptr_t NSROOT::Value::to_null() const
{
	return std::get<0>(this->value_);
}
bool NSROOT::Value::to_bool() const
{
	return std::get<1>(this->value_);
}
double NSROOT::Value::to_number() const
{
	return std::get<2>(this->value_);
}
const std::string& NSROOT::Value::to_string() const
{
	return std::get<3>(this->value_);
}
const std::vector<NSROOT::Value>& NSROOT::Value::to_array() const
{
	return std::get<4>(this->value_);
}
const std::unordered_map<std::string, NSROOT::Value>& NSROOT::Value::to_object() const
{
	return std::get<5>(this->value_);
}
bool& NSROOT::Value::to_bool()
{
	return std::get<1>(this->value_);
}
double& NSROOT::Value::to_number()
{
	return std::get<2>(this->value_);
}
std::string& NSROOT::Value::to_string()
{
	return std::get<3>(this->value_);
}
std::vector<NSROOT::Value>& NSROOT::Value::to_array()
{
	return std::get<4>(this->value_);
}
std::unordered_map<std::string, NSROOT::Value>& NSROOT::Value::to_object()
{
	return std::get<5>(this->value_);
}
std::string NSROOT::Value::to_json_string(uint64_t depth, uint64_t count, char fill) const
{
	if (this->get_type() == Type::Null)
	{
		return "null";
	}
	else if (this->get_type() == Type::Bool)
	{
		return this->to_bool() ? "true" : "false";
	}
	else if (this->get_type() == Type::Number)
	{
		double value = this->to_number();
		if (value == floor(value))
		{
			return std::to_string((int64_t)(value));
		}
		return std::to_string(value);
	}
	else if (this->get_type() == Type::String)
	{
		return "\"" + this->to_string() + "\"";
	}
	else if (this->get_type() == Type::Array)
	{
		return Array::array_to_json_string(this->to_array(), depth, count, fill);
	}
	else if (this->get_type() == Type::Object)
	{
		return Object::object_to_json_string(this->to_object(), depth, count, fill);
	}
	else
	{
		return "";
	}
}

bool NSROOT::Value::operator==(const Value& other) const
{
	return this->value_ == other.value_;
}
bool NSROOT::Value::operator!=(const Value& other) const
{
	return this->value_ != other.value_;
}
NSROOT::Object::Object(const std::unordered_map<std::string, Value>& other)
{
	this->object_ = other;
}
NSROOT::Object::Object(std::unordered_map<std::string, Value> && other) noexcept
{
	this->object_ = std::move(other);
}
NSROOT::Object::Object(const Object & other)
{
	this->object_ = other.object_;
}
NSROOT::Object NSROOT::Object::operator=(const Object & other)
{
	this->object_ = other.object_;
	return *this;
}
NSROOT::Object::Object(Object&& other) noexcept
{
	this->object_ = std::move(other.object_);
}
NSROOT::Object NSROOT::Object::operator=(Object && other) noexcept
{
	if (this == &other) return *this;
	this->object_ = std::move(other.object_);
	return *this;
}
std::unordered_map<std::string, NSROOT::Value> NSROOT::Object::parse_std(std::string_view::const_iterator& pos, const std::string_view::const_iterator& end)//todo
{
	std::unordered_map<std::string, Value> object;
	skip_whitespace(pos, end);
	if (pos == end || *pos != '{')
	{
		throw std::runtime_error("json error");
	}
	++pos;
	while (pos != end)
	{
		skip_whitespace(pos, end);
		if (pos == end) throw std::runtime_error("json error");
		else if (*pos == '}') break;
		Value key = Value::parse(pos, end);
		if (key.get_type() != Type::String) throw std::runtime_error("json error");
		skip_whitespace(pos, end);
		if (pos == end || *pos != ':') throw std::runtime_error("json error");
		else ++pos;
		Value value = Value::parse(pos, end);
		object[key.to_string()] = value;
		skip_whitespace(pos, end);
		if (pos == end || (*pos != ',' && *pos != '}')) throw std::runtime_error("json error");
		else if (*pos == ',') ++pos;
		else if (*pos == '}') break;
	}
	++pos;
	return object;
}
std::unordered_map<std::string, NSROOT::Value> NSROOT::Object::parse_std(std::string_view str)
{
	auto it = str.begin();
	auto end = str.end();
	return parse_std(it, end);
}
std::unordered_map<std::string, NSROOT::Value> NSROOT::Object::parse_std(const std::string& str)
{
	return parse_std(std::string_view(str));
}
std::unordered_map<std::string, NSROOT::Value> NSROOT::Object::parse_std(const char* str)
{
	return parse_std(std::string_view(str));
}
NSROOT::Object NSROOT::Object::parse(std::string_view str)
{
	auto it = str.begin();
	auto end = str.end();
	return parse_std(it, end);
}
NSROOT::Object NSROOT::Object::parse(const std::string& str)
{
	return parse_std(std::string_view(str));
}
NSROOT::Object NSROOT::Object::parse(const char* str)
{
	return parse_std(std::string_view(str));
}
std::unordered_map<std::string, NSROOT::Value>& NSROOT::Object::to_unordered_map()
{
	return this->object_;
}
const std::unordered_map<std::string, NSROOT::Value>& NSROOT::Object::to_unordered_map() const
{
	return this->object_;
}
std::string NSROOT::Object::to_json_string(uint64_t depth, uint64_t count, char fill)
{
	return Object::object_to_json_string(this->object_, depth, count, fill);
}
std::string NSROOT::Object::object_to_json_string(const std::unordered_map<std::string, Value>& object, uint64_t depth, uint64_t count, char fill)
{
	std::string result = "{\n";
	std::string indent((depth + 1) * count, fill);
	for (auto& [key, value] : object)
	{
		result += indent;
		result += '"' + key + '"';
		result += ": ";
		result += value.to_json_string(depth + 1, count, fill);
		result += ",\n";
	}
	if (result.size() > 2)
	{
		result.pop_back();
		result.pop_back();
		for (uint64_t i = 0; i < count; ++i) indent.pop_back();
		result += '\n';
		result += indent;
	}
	else
	{
		result.back() = ' ';
	}
	result += '}';
	return result;
}
NSROOT::Object::it NSROOT::Object::begin()
{
	return this->object_.begin();
}
NSROOT::Object::cit NSROOT::Object::cbegin() const
{
	return this->object_.cbegin();
}
NSROOT::Object::it NSROOT::Object::end()
{
	return this->object_.end();
}
NSROOT::Object::cit NSROOT::Object::cend() const
{
	return this->object_.cend();
}
bool NSROOT::Object::empty() const
{
	return this->object_.empty();
}
uint64_t NSROOT::Object::size() const
{
	return this->object_.size();
}
uint64_t NSROOT::Object::max_size() const
{
	return this->object_.max_size();
}
void NSROOT::Object::clear()
{
	this->object_.clear();
}
std::pair<NSROOT::Object::it, bool> NSROOT::Object::insert(const value_type & pair)
{
	return this->object_.insert(pair);
}
std::pair<NSROOT::Object::it, bool> NSROOT::Object::insert(value_type&& pair) noexcept
{
	return this->object_.insert(std::move(pair));
}
NSROOT::Object::it NSROOT::Object::insert(cit pos, const value_type& pair)
{
	return this->object_.insert(pos, pair);
}
NSROOT::Object::it NSROOT::Object::insert(cit pos, value_type&& pair) noexcept
{
	return this->object_.insert(pos, std::move(pair));
}
void NSROOT::Object::insert(std::initializer_list<value_type> ilist)
{
	this->object_.insert(ilist);
}
std::pair<NSROOT::Object::it, bool> NSROOT::Object::insert_or_assign(const std::string & key, const Value & value)
{
	return this->object_.insert_or_assign(key, value);
}
std::pair<NSROOT::Object::it, bool> NSROOT::Object::insert_or_assign(std::string&& key, const Value& value)
{
	return this->object_.insert_or_assign(std::move(key), value);
}
NSROOT::Object::it NSROOT::Object::insert_or_assign(cit hint, const std::string& k, Value&& obj)
{
	return this->object_.insert_or_assign(hint, k, std::move(obj));
}
NSROOT::Object::it NSROOT::Object::insert_or_assign(cit hint, std::string&& k, Value&& obj)
{
	return this->object_.insert_or_assign(hint, std::move(k), std::move(obj));
}
NSROOT::Object::it NSROOT::Object::erase(it pos)
{
	return this->object_.erase(pos);
}
NSROOT::Object::it NSROOT::Object::erase(cit pos)
{
	return this->object_.erase(pos);
}
NSROOT::Object::it NSROOT::Object::erase(cit start, cit end)
{
	return this->object_.erase(start, end);
}
NSROOT::Object::size_type NSROOT::Object::erase(const std::string& key)
{
	return this->object_.erase(key);
}
void NSROOT::Object::swap(Object& other) noexcept
{
	this->object_.swap(other.object_);
}
NSROOT::Value& NSROOT::Object::operator[](const std::string& key)
{
	return this->object_[key];
}
NSROOT::Value& NSROOT::Object::operator[](std::string&& key)
{
	return this->object_[std::move(key)];
}
NSROOT::Value& NSROOT::Object::at(const std::string& key)
{
	return this->object_.at(key);
}
const NSROOT::Value& NSROOT::Object::at(const std::string& key) const
{
	return this->object_.at(key);
}
NSROOT::Object::size_type NSROOT::Object::count(const std::string& key) const
{
	return this->object_.count(key);
}
NSROOT::Object::it NSROOT::Object::find(const std::string& key)
{
	return this->object_.find(key);
}
NSROOT::Object::cit NSROOT::Object::find(const std::string& key) const
{
	return this->object_.find(key);
}
bool NSROOT::Object::contains(const std::string& key) const
{
	return this->object_.contains(key);
}

NSROOT::Type NSROOT::Object::get_type(const std::string& str) const
{
	auto it = this->object_.find(str);
	if (it == this->object_.end()) return Type::Undefined;
	return it->second.get_type();
}

NSROOT::Array::Array()
{}
NSROOT::Array::Array(const std::vector<Value>&array)
{
	this->array_ = array;
}
NSROOT::Array::Array(std::vector<Value> && array)
{
	this->array_ = std::move(array);
}
NSROOT::Array::Array(const Array & other)
{
	this->array_ = other.array_;
}
NSROOT::Array NSROOT::Array::operator=(const Array & other)
{
	this->array_ = other.array_;
	return *this;
}
NSROOT::Array::Array(Array&& other) noexcept
{
	this->array_ = std::move(other.array_);
}
NSROOT::Array NSROOT::Array::operator=(Array && other) noexcept
{
	if (this == &other) return *this;
	this->array_ = std::move(other.array_);
	return *this;
}
std::vector<NSROOT::Value> NSROOT::Array::parse_std(std::string_view::iterator& pos, const std::string_view::iterator& end)
{
	std::vector<Value> array;
	skip_whitespace(pos, end);
	if (pos == end || *pos != '[')
	{
		throw std::runtime_error("json error");
	}
	++pos;
	while (pos != end)
	{
		skip_whitespace(pos, end);
		if (pos == end) throw std::runtime_error("json error");
		else if (*pos == ']') break;
		Value value = Value::parse(pos, end);
		array.push_back(value);
		skip_whitespace(pos, end);
		if (*pos != ',' && *pos != ']')
		{
			throw std::runtime_error("json error");
		}
		else if (*pos == ',') ++pos;
		else if (*pos == ']') break;
	}
	++pos;
	return array;
}
std::vector<NSROOT::Value> NSROOT::Array::parse_std(std::string_view str)
{
	auto it = str.begin();
	auto end = str.end();
	return parse_std(it, end);
}
std::vector<NSROOT::Value> NSROOT::Array::parse_std(const std::string& str)
{
	return parse_std(std::string_view(str));
}
std::vector<NSROOT::Value> NSROOT::Array::parse_std(const char* str)
{
	return parse_std(std::string_view(str));
}
NSROOT::Array NSROOT::Array::parse(std::string_view str)
{
	auto it = str.begin();
	auto end = str.end();
	return parse_std(it, end);
}
NSROOT::Array NSROOT::Array::parse(const std::string& str)
{
	return parse_std(std::string_view(str));
}
NSROOT::Array NSROOT::Array::parse(const char* str)
{
	return parse_std(std::string_view(str));
}
std::vector<NSROOT::Value>& NSROOT::Array::to_vector()
{
	return this->array_;
}
const std::vector<NSROOT::Value>& NSROOT::Array::to_vector() const
{
	return this->array_;
}
std::string NSROOT::Array::to_json_string(uint64_t depth, uint64_t count, char fill)
{
	return array_to_json_string(this->array_, depth, count, fill);
}
std::string NSROOT::Array::array_to_json_string(const std::vector<Value>& array, uint64_t depth, uint64_t count, char fill)
{
	std::string result = "[\n";
	std::string indent((depth + 1) * count, fill);
	for (auto& value : array)
	{
		result += indent;
		result += value.to_json_string(depth + 1, count, fill);
		result += ",\n";
	}
	if (result.size() > 2)
	{
		result.pop_back();
		result.pop_back();
		for (uint64_t i = 0; i < count; ++i) indent.pop_back();
		result += '\n';
		result += indent;
	}
	else
	{
		result.back() = ' ';
	}
	result += ']';
	return result;
}
void NSROOT::Array::assign(uint64_t count, const Value& elem)
{
	return this->array_.assign(count, elem);
}
std::vector<uint8_t>::allocator_type NSROOT::Array::get_allocator() const
{
	return this->array_.get_allocator();
}
NSROOT::Value& NSROOT::Array::operator[](uint64_t index)
{
	return this->array_[index];
}
const NSROOT::Value& NSROOT::Array::operator[](uint64_t index) const
{
	return this->array_[index];
}
NSROOT::Value& NSROOT::Array::at(uint64_t index)
{
	return this->array_.at(index);
}
const NSROOT::Value& NSROOT::Array::at(uint64_t index) const
{
	return this->array_.at(index);
}
NSROOT::Value& NSROOT::Array::front()
{
	return this->array_.front();
}
const NSROOT::Value& NSROOT::Array::front() const
{
	return this->array_.front();
}
NSROOT::Value& NSROOT::Array::back()
{
	return this->array_.back();
}
const NSROOT::Value& NSROOT::Array::back() const
{
	return this->array_.back();
}
NSROOT::Array::it NSROOT::Array::begin()
{
	return this->array_.begin();
}
NSROOT::Array::cit NSROOT::Array::cbegin() const
{
	return this->array_.cbegin();
}
NSROOT::Array::it NSROOT::Array::end()
{
	return this->array_.end();
}
NSROOT::Array::cit NSROOT::Array::cend() const
{
	return this->array_.cend();
}
NSROOT::Array::rit NSROOT::Array::rbegin()
{
	return this->array_.rbegin();
}
NSROOT::Array::crit NSROOT::Array::crbegin() const
{
	return this->array_.crbegin();
}
NSROOT::Array::crit NSROOT::Array::rend()
{
	return this->array_.rend();
}
NSROOT::Array::crit NSROOT::Array::crend() const
{
	return this->array_.crend();
}
uint64_t NSROOT::Array::size() const
{
	return this->array_.size();
}
bool NSROOT::Array::empty() const
{
	return this->array_.empty();
}
uint64_t NSROOT::Array::max_size() const
{
	return this->array_.max_size();
}
void NSROOT::Array::reserve(uint64_t count)
{
	return this->array_.reserve(count);
}
uint64_t NSROOT::Array::capacity() const
{
	return this->array_.capacity();
}
void NSROOT::Array::shrink_to_fit()
{
	return this->array_.shrink_to_fit();
}
void NSROOT::Array::clear()
{
	return this->array_.clear();
}
NSROOT::Array::it NSROOT::Array::insert(it pos, const Value & elem)
{
	return this->array_.insert(pos, elem);
}
NSROOT::Array::it NSROOT::Array::insert(it pos, Value&& elem)
{
	return this->array_.insert(pos, std::move(elem));
}
NSROOT::Array::it NSROOT::Array::insert(it pos, uint64_t count, const Value& elem)
{
	return this->array_.insert(pos, count, elem);
}
NSROOT::Array::it NSROOT::Array::insert(it pos, uint64_t count, Value&& elem)
{
	return this->array_.insert(pos, count, std::move(elem));
}
NSROOT::Array::it NSROOT::Array::insert(it pos, std::initializer_list<Value> ilist)
{
	return this->array_.insert(pos, ilist);
}

NSROOT::Array::it NSROOT::Array::erase(it pos)
{
	return this->array_.erase(pos);
}
NSROOT::Array::it NSROOT::Array::erase(cit pos)
{
	return this->array_.erase(pos);
}
NSROOT::Array::it NSROOT::Array::erase(it first, it last)
{
	return this->array_.erase(first, last);
}
NSROOT::Array::it NSROOT::Array::erase(cit first, cit last)
{
	return this->array_.erase(first, last);
}
void NSROOT::Array::push_back(const Value& elem)
{
	this->array_.push_back(elem);
}
void NSROOT::Array::push_back(Value && elem)
{
	this->array_.push_back(std::move(elem));
}
void NSROOT::Array::pop_back()
{
	this->array_.pop_back();
}
void NSROOT::Array::resize(uint64_t count)
{
	this->array_.resize(count);
}
void NSROOT::Array::resize(uint64_t count, const Value & elem)
{
	this->array_.resize(count, elem);
}
void NSROOT::Array::swap(Array & other)
{
	this->array_.swap(other.array_);
}
bool NSROOT::Array::operator==(const Array& other) const
{
	return this->array_ == other.array_;
}
bool NSROOT::Array::operator!=(const Array& other) const
{
	return this->array_ != other.array_;
}



#undef ERROR_WRONG_FORMAT

#undef NSROOT
