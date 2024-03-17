#pragma once
#include <map>
#include <string>
#include <vector>


namespace HexGPU
{

class Settings final
{
public:
	explicit Settings(std::string_view file_name);
	~Settings();

	Settings(const Settings&)= delete;
	Settings& operator=(const Settings&)= delete;

public:
	using IntType= int64_t;
	using RealType= float;

	std::string_view GetOrSetString(std::string_view key, std::string_view default_value= "");
	IntType GetOrSetInt(std::string_view key, IntType default_value= 0);
	RealType GetOrSetReal(std::string_view key, RealType default_value= 0.0);

	std::string_view GetString(std::string_view key, std::string_view default_value= "");
	IntType GetInt(std::string_view key, IntType default_value = 0);
	RealType GetReal(std::string_view key, RealType default_value = 0.0);

	void SetString(std::string_view key, std::string_view value);
	void SetInt(std::string_view key, IntType value);
	void SetReal(std::string_view key, RealType value);

	bool HasValue(std::string_view key);

	std::vector<std::string> GetSettingsKeysStartsWith(std::string_view key_start) const;

private:
	const std::string file_name_;
	std::string temp_key_;
	// Use std::map in order to save values in alphabetical order.
	std::map<std::string, std::string> values_map_;
};

} // namespace HexGPU
