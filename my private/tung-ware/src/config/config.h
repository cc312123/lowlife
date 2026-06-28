#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "../settings.h"

namespace config
{
	struct config_file_t
	{
		std::string name;
		std::string path;
	};

	std::string get_config_folder();
	bool create_config_folder();
	std::vector<config_file_t> get_config_files();
	bool save_config(const std::string& name);
	bool load_config(const std::string& name);
	bool delete_config(const std::string& name);
}
