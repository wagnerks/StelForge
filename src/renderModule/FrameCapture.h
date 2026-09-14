#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rhi/RenderHardwareInterface.h"

namespace SFE::Render {
	// Deterministic frame grab for backend comparison: reads the exact image present() would
	// blit to the screen and writes it as a PNG. Works with a hidden or occluded window, so it
	// never depends on the window being visible or focused.
	// `path` is resolved against the content root unless it is already absolute.
	bool captureFrameToPng(const std::string& path);

	// Checksum of read-back pixels, so a caller can skip re-writing identical content and a
	// comparison script can spot a changed pass without loading every image.
	uint32_t crcOfPixels(const std::vector<uint8_t>& rgba);

	// RGBA8, top-down. Exposed for callers that want the pixels rather than a file.
	bool writePng(const std::string& path, const std::vector<uint8_t>& rgba, int width, int height);
}
