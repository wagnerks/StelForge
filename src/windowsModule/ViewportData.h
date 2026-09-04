#pragma once
namespace SFE::CoreModule {
	struct ViewportData {
		void setSize(int w, int h) {
			renderW = w;
			renderH = h;

#ifdef __APPLE__
			width = renderW / renderScaleW;
			height = renderH / renderScaleH;
#else
			width = renderW;
			height = renderH;
#endif
		}

		int width = 1920;
		int height = 1080;
		int renderW = width;
		int renderH = height;
		int backbufferW = width;
		int backbufferH = height;

		float renderScaleW = 1.f;
		float renderScaleH = 1.f;

		float far = 5000.f;
		float near = 0.15f;

		int posX = 0;
		int posY = 0;
	};
}

