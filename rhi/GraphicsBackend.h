#pragma once

namespace SFE::Render {

	enum class GraphicsBackend {
		OpenGL,
		Vulkan,
		// No window and no GPU: every call succeeds and draws nothing. For running the game without a picture.
		None,
	};

	// From STELFORGE_BACKEND, then --backend. False, with the backend unchanged, when a name is not one of
	// opengl/gl, vulkan/vk, none/null.
	bool configureGraphicsBackend(int argc, char** argv);
	GraphicsBackend activeGraphicsBackend();
	const char* graphicsBackendName(GraphicsBackend backend);

}
