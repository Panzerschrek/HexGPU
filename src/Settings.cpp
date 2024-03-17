#include "Settings.hpp"
#include "Log.hpp"
#include <cstring>
#include <optional>


namespace HexGPU
{

namespace
{

// TODO - use to_chars/from_chars

std::optional<Settings::IntType> StrToInt(const char* str)
{
	Settings::IntType sign= 1;
	if(str[0] == '-')
	{
		sign= -1;
		++str;
	}

	Settings::IntType v= 0;
	while(*str != 0)
	{
		if(str[0] < '0' || str[0] > '9')
			return std::nullopt;
		v*= 10;
		v+= str[0] - '0';
		++str;
	}
	return v * sign;
}

std::optional<Settings::RealType> StrToReal(const char* str)
{
	Settings::RealType sign= 1.0;
	if( str[0] == '-' )
	{
		sign= -1.0;
		++str;
	}

	Settings::RealType v= 0.0;
	while(*str != 0)
	{
		if(str[0] == ',' || str[0] == '.')
		{
			++str;
			break;
		}
		if(str[0] < '0' || str[0] > '9')
			return std::nullopt;
		v*= 10.0;
		v+= float(str[0] - '0');
		++str;
	}
	Settings::RealType m= 0.1f;
	while( *str != 0 )
	{
		if( str[0] < '0' || str[0] > '9' )
			return std::nullopt;

		v+= float(str[0] - '0') * m;
		m*= 0.1f;
		++str;
	}

	return v * sign;
}

std::string NumberToString(const Settings::IntType i)
{
	return std::to_string(i);
}

std::string NumberToString(const Settings::RealType f)
{
	// HACK - replace ',' to '.' for bad locale
	std::string result= std::to_string(f);
	size_t pos = result.find(",");
	if (pos != std::string::npos) result[pos] = '.';

	return result;
}

std::string MakeQuotedString(const std::string& str)
{
	std::string result;
	result.reserve(str.size() + 3u);
	result+= "\"";

	for(const char c : str)
	{
		if(c == '"' || c == '\\')
			result+= '\\';
		result+= c;
	}

	result += "\"";
	return result;
}

} // namespace

Settings::Settings(const std::string_view file_name)
	: file_name_(file_name)
{
	std::ifstream file(file_name_);

	if(!file.is_open())
	{
		Log::Warning("Can't open file \"", file_name_, "\"");
		return;
	}

	std::string line;

	while(std::getline(file, line))
	{
		std::string str[2]; // key-value pair

		const char* s= line.data();
		const char* const s_end= line.data() + line.size();
		for(size_t i= 0u; i < 2u; ++i)
		{
			while(s < s_end && std::isspace(*s))
				++s;
			if(s == s_end)
				break;

			if(*s == '"') // string in quotes
			{
				++s;
				while(s < s_end && *s != '"')
				{
					if(*s == '\\' && s + 1 < s_end && (s[1] == '"' || s[1] == '\\') )
						++s; // Escaped symbol
					str[i].push_back(*s);
					++s;
				}
				if(*s == '"')
					++s;
				else
					break;
			}
			else
			{
				while(s < s_end && !std::isspace(*s))
				{
					str[i].push_back(*s);
					++s;
				}
			}
		}

		if(!str[0].empty())
			values_map_.emplace(std::move(str[0]), std::move(str[1]));
	}
}

Settings::~Settings()
{
	std::ofstream file(file_name_);

	if(!file.is_open())
	{
		Log::Warning("Can't open file \"", file_name_, "\"");
		return;
	}

	for(const auto& map_value : values_map_)
	{
		const std::string key= MakeQuotedString(map_value.first);
		const std::string value= MakeQuotedString(map_value.second);

		file << key << " " << value << "\n";
	}

	file.flush();
	if(file.fail())
		Log::Warning("Failed to flush file \"", file_name_, "\"");
}

std::string_view Settings::GetOrSetString(const std::string_view key, const std::string_view default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		auto it_bool_pair= values_map_.emplace(temp_key_, default_value);
		return it_bool_pair.first->second;
	}
	else
	{
		return it->second;
	}
}

Settings::IntType Settings::GetOrSetInt(const std::string_view key, const IntType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		values_map_.emplace(temp_key_, NumberToString(default_value));
		return default_value;
	}
	else
	{
		const auto res= StrToInt(it->second.c_str());
		if(res == std::nullopt)
		{
			it->second= NumberToString(default_value);
			return default_value;
		}
		return *res;
	}
}

Settings::RealType Settings::GetOrSetReal(const std::string_view key, const RealType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
	{
		values_map_.emplace(temp_key_, NumberToString(default_value));
		return default_value;
	}
	else
	{
		const auto res= StrToReal(it->second.c_str());
		if(res == std::nullopt)
		{
			it->second= NumberToString(default_value);
			return default_value;
		}
		return *res;
	}
}

std::string_view Settings::GetString(const std::string_view key, const std::string_view default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
		return default_value;
	return it->second;
}

Settings::IntType Settings::GetInt(std::string_view key, const IntType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
		return default_value;
	if(const std::optional<IntType> value_parsed= StrToInt(it->second.data()))
		return *value_parsed;
	return default_value;
}

Settings::RealType Settings::GetReal(std::string_view key, const RealType default_value)
{
	temp_key_= key;
	const auto it= values_map_.find(temp_key_);
	if(it == values_map_.end())
		return default_value;
	if(const std::optional<RealType> value_parsed= StrToReal(it->second.data()))
		return *value_parsed;
	return default_value;
}

void Settings::SetString(const std::string_view key, const std::string_view value)
{
	temp_key_= key;
	values_map_[temp_key_]= value;
}

void Settings::SetInt(const std::string_view key, const IntType value)
{
	temp_key_= key;
	values_map_[temp_key_]= NumberToString(value);
}

void Settings::SetReal(const std::string_view key, const RealType value)
{
	temp_key_= key;
	values_map_[temp_key_]= NumberToString(value);
}

bool Settings::HasValue(const std::string_view key)
{
	temp_key_= key;
	return values_map_.count(temp_key_) != 0;
}

std::vector<std::string> Settings::GetSettingsKeysStartsWith(const std::string_view key_start) const
{
	std::vector<std::string> result;
	for(const auto& map_value : values_map_)
	{
		if(map_value.first.size() >= key_start.size() &&
			std::string_view(map_value.first.data(), key_start.size()) == key_start)
			result.push_back(map_value.first);
	}

	return result;
}

} // namespace HexGPU
