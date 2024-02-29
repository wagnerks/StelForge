#pragma once
namespace SFE::CoreModule {
	struct ScreenDrawData {
		int width = 1920;
		int height = 1080;
		int renderW = width;
		int renderH = height;

		float renderScaleW = 1.f;
		float renderScaleH = 1.f;

		float far = 5000.f;
		float near = 1.f;
	};
}

