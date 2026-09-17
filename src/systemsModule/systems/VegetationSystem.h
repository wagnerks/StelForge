#pragma once

#include <mutex>
#include <vector>

#include "componentsModule/VegetationComponent.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	// Keeps the scene's vegetation (VegetationPlacementsComponent) as model entities only where it is near enough
	// to matter: each layer with a draw distance is spawned inside it around the camera and destroyed once the
	// camera has moved away. A layer drawn as impostors beyond that distance is marked, placement by placement,
	// hidden from the impostors while its entity is there, so exactly one of the two draws it.
	class VegetationSystem : public ecss::System {
	public:
		VegetationSystem() = default;
		// Main thread, after the camera has moved and before the frame is rendered.
		void update(float dt) override;

		// One bit per placement, set while its entity draws it instead of its impostor. Copied into `bits` when it
		// changed since `version`; true then.
		bool copyEntityBits(std::vector<uint32_t>& bits, uint64_t& version);

		// What grows and where, once the first update has read it; empty before.
		const ComponentsModule::VegetationComponent& vegetation() const { return mVegetation; }
		const ComponentsModule::VegetationPlacementsComponent& placements() const { return mPlacements; }

	private:
		struct Pending {
			uint32_t instance = 0;
			ecss::EntityId entity = ecss::INVALID_ID;
			uint64_t frame = 0; // acted on from this frame on
		};

		bool init();
		void spawn(uint32_t instance);
		void setBit(uint32_t instance, bool set);

		bool mInited = false;
		ComponentsModule::VegetationComponent mVegetation;
		ComponentsModule::VegetationPlacementsComponent mPlacements;
		// Per layer: its surfaces' textures, and the surfaces themselves in mVegetation.
		struct Surface {
			Render::TextureHandle on;
			Render::TextureHandle albedo;
			Render::TextureHandle normals;
			const ComponentsModule::VegetationSurface* layer = nullptr;
		};
		std::vector<std::vector<Surface>> mSurfaces;
		float mReach = 0.f;    // the farthest any streamed layer is spawned, with the detail setting in it
		float mFarthest = 0.f; // the farthest a layer is authored to reach
		// The graphics settings' lod detail: how far the models are kept instead of their impostors.
		float mDetail = 1.f;
		// Metres a layer's models are kept out to.
		float reachOf(const ComponentsModule::VegetationLayer& layer) const { return layer.drawDistance * mDetail; }

		std::vector<ecss::EntityId> mEntities; // per placement
		std::vector<ecss::EntityId> mRoots;    // per placement: its root model's entity, if it has one
		std::vector<uint32_t> mSpawned;        // placements of streamed layers that have an entity
		std::vector<Pending> mShowing;         // spawned, their bit set once the entity is surely drawn
		std::vector<Pending> mDestroying;      // bit cleared, the entity destroyed once the impostor surely draws
		uint64_t mFrame = 0;

		std::mutex mBitsMutex;
		std::vector<uint32_t> mBits;
		uint64_t mBitsVersion = 1;
	};
}
