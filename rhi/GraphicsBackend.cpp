#include "rhi/GraphicsBackend.h"

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace SFE::Render {
	namespace {
		GraphicsBackend gBackend = GraphicsBackend::OpenGL;

		bool equalsIgnoreCase(std::string_view a, std::string_view b) {
			if (a.size() != b.size()) {
				return false;
			}
			for (size_t i = 0; i < a.size(); ++i) {
				const char ca = a[i];
				const char cb = b[i];
				if (ca >= 'A' && ca <= 'Z') {
					if (ca + ('a' - 'A') != cb) {
						return false;
					}
				}
				else if (cb >= 'A' && cb <= 'Z') {
					if (cb + ('a' - 'A') != ca) {
						return false;
					}
				}
				else if (ca != cb) {
					return false;
				}
			}
			return true;
		}

		GraphicsBackend parseName(std::string_view name) {
			if (equalsIgnoreCase(name, "vulkan") || equalsIgnoreCase(name, "vk")) {
				return GraphicsBackend::Vulkan;
			}
			return GraphicsBackend::OpenGL;
		}
	}

	void configureGraphicsBackend(int argc, char** argv) {
		if (const char* env = std::getenv("STELFORGE_BACKEND")) {
			gBackend = parseName(env);
		}

		for (int i = 1; i < argc; ++i) {
			if (!argv[i]) {
				continue;
			}
			const std::string_view arg{ argv[i] };
			if (arg == "--backend" && i + 1 < argc && argv[i + 1]) {
				gBackend = parseName(argv[i + 1]);
				++i;
			}
			else if (arg.starts_with("--backend=")) {
				gBackend = parseName(arg.substr(std::string_view("--backend=").size()));
			}
		}
	}

	GraphicsBackend activeGraphicsBackend() {
		return gBackend;
	}

	const char* graphicsBackendName(const GraphicsBackend backend) {
		switch (backend) {
		case GraphicsBackend::Vulkan: return "Vulkan";
		default: return "OpenGL";
		}
	}
}
