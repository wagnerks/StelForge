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
		/// @brief Where the engine's own assets are -- shaders, fonts, the stand-in textures.
		/// A game's content root is searched first and this one is the fallback, so a game may
		/// override any of them by putting a file of the same name in its own content.
		static void setEngineRoot(std::string_view path);
		static const std::filesystem::path& engineRoot();
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
		inline static std::filesystem::path sEngineRoot{};
	};
}
