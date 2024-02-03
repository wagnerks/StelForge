#pragma once

#include <vector>

#include "core/Singleton.h"

namespace SFE::Debug {
	class ShadersDebug : public Singleton<ShadersDebug> {
		friend Singleton;
	public:
		void shadersDebugDraw(bool& opened);
	private:
		~ShadersDebug() override = default;
		std::vector<size_t> mOpenedShaderDebugWindows;
	};
}
