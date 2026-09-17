#include "GameModule.h"

#include <algorithm>

#include "systemsModule/SystemsPriority.h"

namespace SFE {
	namespace {
		std::vector<GameModule::SystemsHook>& systemHooks() {
			static std::vector<GameModule::SystemsHook> hooks;
			return hooks;
		}

		std::vector<GameModule::Panel>& panelList() {
			static std::vector<GameModule::Panel> panels;
			return panels;
		}

		std::vector<Render::RenderPassEntry>& passList() {
			static std::vector<Render::RenderPassEntry> list(std::begin(SystemsModule::RENDER_PASSES), std::end(SystemsModule::RENDER_PASSES));
			return list;
		}
	}

	void GameModule::addSystems(SystemsHook hook) {
		if (hook) {
			systemHooks().push_back(std::move(hook));
		}
	}

	void GameModule::createSystems(ecss::SystemManager& manager) {
		for (const auto& hook : systemHooks()) {
			hook(manager);
		}
	}

	void GameModule::addPass(const std::string_view after, const Render::RenderPassEntry entry) {
		auto& list = passList();
		const auto at = after.empty() ? list.end()
			: std::find_if(list.begin(), list.end(), [&](const Render::RenderPassEntry& e) { return after == e.name; });
		// A pass whose anchor is not there goes last rather than nowhere: the game is then drawn
		// over the finished frame instead of silently doing nothing.
		list.insert(at == list.end() ? list.end() : at + 1, entry);
	}

	const std::vector<Render::RenderPassEntry>& GameModule::passes() {
		return passList();
	}

	void GameModule::addPanel(std::string title, std::function<void()> draw) {
		if (draw) {
			panelList().push_back({ std::move(title), std::move(draw) });
		}
	}

	const std::vector<GameModule::Panel>& GameModule::panels() {
		return panelList();
	}
}
