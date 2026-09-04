#include "ShaderBase.h"

#include "core/FileSystem.h"

#include <filesystem>
#include <regex>
#include <string_view>
#include <unordered_set>

using namespace SFE::ShaderModule;

namespace {
	std::string loadResolved(const std::filesystem::path& path, std::unordered_set<std::string>& seen) {
		const auto key = path.lexically_normal().generic_string();
		if (!seen.insert(key).second) {
			return {};
		}

		std::string code;
		SFE::FileSystem::readFile(path.generic_string(), code);
		if (code.empty()) {
			return {};
		}

		static const std::regex includeRe(R"-(^\s*#include\s+"([^"]+)")-");
		std::string out;
		out.reserve(code.size() + 256);
		const auto dir = path.parent_path();
		std::string_view rest = code;
		while (!rest.empty()) {
			const auto nl = rest.find('\n');
			const auto line = std::string(rest.substr(0, nl == std::string_view::npos ? rest.size() : nl + 1));
			rest = nl == std::string_view::npos ? std::string_view{} : rest.substr(nl + 1);

			std::smatch m;
			if (std::regex_search(line, m, includeRe)) {
				const std::filesystem::path inc(m[1].str());
				auto resolved = inc.is_absolute() ? inc : dir / inc;
				if (!std::filesystem::exists(resolved)) {
					resolved = SFE::FileSystem::resolve((std::filesystem::path("shaders") / inc).generic_string());
				}
				out += loadResolved(resolved, seen);
			}
			else {
				out += line;
			}
		}
		return out;
	}
}

std::string ShaderBase::loadShaderCode(std::string_view path) {
	std::unordered_set<std::string> seen;
	return loadResolved(SFE::FileSystem::resolve(path), seen);
}
