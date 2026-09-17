#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "renderModule/renderPasses/RenderPass.h"

namespace ecss {
	class SystemManager;
}

namespace SFE {
	/**
	 * @brief What a game adds to the engine.
	 *
	 * The engine knows nothing of any game: no game system, render pass or component is named
	 * anywhere in it. A game registers itself here before Engine::init, and the engine asks this
	 * at the points where it used to hold lists of its own. Both the runtime and the editor call
	 * the same registration function of the game they are built with, which is what lets the
	 * editor show and edit a game's components without the engine knowing what they are.
	 *
	 * Registration is explicit rather than done by a static initialiser: a static library drops
	 * translation units nothing references, and self-registering objects disappear with them.
	 */
	class GameModule {
	public:
		/// @brief Systems the game creates once the engine's own exist. Called with the manager.
		using SystemsHook = std::function<void(ecss::SystemManager&)>;
		static void addSystems(SystemsHook hook);
		/// @brief Runs every hook; the engine calls this while it starts its systems.
		static void createSystems(ecss::SystemManager& manager);

		/**
		 * @brief A render pass of the game's, drawn right after the engine pass called @p after.
		 *
		 * Draw order is what a pass depends on, so a game says which pass it follows rather than
		 * an index that shifts when the engine gains one. An empty @p after puts it last.
		 */
		static void addPass(std::string_view after, Render::RenderPassEntry entry);

		/// @brief The engine's passes in draw order, with the game's inserted. The engine walks this.
		static const std::vector<Render::RenderPassEntry>& passes();

		/// @brief A window of the game's in the debug UI, drawn beside the engine's own.
		struct Panel {
			std::string title;
			std::function<void()> draw;
		};
		static void addPanel(std::string title, std::function<void()> draw);
		static const std::vector<Panel>& panels();
	};
}
