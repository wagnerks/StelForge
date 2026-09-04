#pragma once

#include <filesystem>
#include <json/value.h>
#include <string>
#include <string_view>

namespace SFE {
	class FileSystem {
	public:
		static void setRoot(std::string_view path);
		static const std::filesystem::path& root();
		static std::filesystem::path resolve(std::string_view path);
		static std::filesystem::path executableDir();
		static std::filesystem::path guessContentRoot(std::string_view exeDir = {});

		static bool isFileExists(std::string_view path);
		static bool createDirectories(std::string_view path);
		static bool readFile(std::string_view path, std::string& file);
		static bool writeFile(std::string_view path, std::string& file);

		static bool readJson(std::string_view path, Json::Value& root, bool withComments = false);
		static bool writeJson(std::string_view path, const Json::Value& root);
		static Json::Value readJson(std::string_view path, bool withComments = false);

	private:
		inline static std::filesystem::path sRoot{};
	};
}
