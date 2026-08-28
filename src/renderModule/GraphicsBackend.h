#pragma once

namespace SFE::Render {

	enum class GraphicsBackend {
		OpenGL,
		Vulkan,
	};

	void configureGraphicsBackend(int argc, char** argv);
	GraphicsBackend activeGraphicsBackend();
	const char* graphicsBackendName(GraphicsBackend backend);

}
