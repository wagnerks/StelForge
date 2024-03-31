#pragma once

namespace SFE::CoreModule {
	class Core {
	public:
		Core() = default;
		~Core();

		void update(float dt);
		void init();
	};
}

