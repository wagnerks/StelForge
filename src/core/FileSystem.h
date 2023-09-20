#pragma once

#include <json/value.h>

namespace Engine {
	class FileSystem {
	public:
		static bool isFileExists(std::string_view path);
		static bool readFile(std::string_view path, std::string& file);
		static bool writeFile(std::string_view path, std::string& file);

		static bool readJson(std::string_view path, Json::Value& root, bool withComments = false);
		static bool writeJson(std::string_view path, const Json::Value& root);
		static Json::Value readJson(std::string_view path, bool withComments = false);
	};
}
