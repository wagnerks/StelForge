#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	// Applies every CharacterAppearanceComponent to its entity's model: tints the skin and the hair and shows the one
	// hairstyle worn. Main thread; only an entity whose appearance changed is touched.
	class AppearanceSystem : public ecss::System {
	public:
		void update(float dt) override;

		// The hairstyles the entity's model offers: its parts named "Hair_...", in the model's order.
		static std::vector<std::string> hairStyles(ecss::EntityId entity);
		// Applies the entity's appearance again on the next update, over whatever else changed its materials.
		void reapply(ecss::EntityId entity) { mApplied.erase(entity); }

	private:
		std::unordered_map<ecss::EntityId, uint64_t> mApplied; // a hash of what was last applied
	};
}
