#pragma once

#include <future>
#include <random>
#include <string_view>
#include <string>
#include <vector>

#include "componentsModule/SoundscapeComponent.h"
#include "componentsModule/TerrainComponent.h"
#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

namespace SFE::SystemsModule {
	class AudioSystem;
	class VegetationSystem;

	// Plays the scene's SoundscapeComponent through AudioSystem's voices, from where the listener is.
	class SoundscapeSystem : public ecss::System {
	public:
		void update(float dt) override;

		// A step at `feet`, taken at `speed` metres a second: the sound of whatever the ground there is.
		void footstep(const Math::Vec3& feet, float speed);
		// grass, forest, road, bank or water.
		std::string_view surfaceAt(const Math::Vec3& feet) const;

	private:
		struct Voice {
			uint32_t id = 0;       // AudioSystem's, 0 for none
			Math::Vec3 pos{};
			float gain = 0.f;      // faded towards `target`
			float target = 0.f;
			float occlusion = 1.f; // what reaches the listener past the trees, 0..1
			float cutoff = 20000.f;
		};
		struct Bird {
			Voice voice;
			double nextSong = 0.0;   // clock seconds
			int songsLeft = 0;       // at this tree before it moves on
			bool placed = false;
		};
		struct Frog {
			Math::Vec3 pos{};
			Voice voice;
			bool silent = false;
			double quietUntil = 0.0; // clock seconds: silent at least until then
		};

		bool init();
		// The trees within `radius` of `at` over the most there are at the forest's thickest, 0..1.
		float forestAround(const Math::Vec3& at, float radius) const;
		// How many trunks and crowns stand across the line from `from` to `to`.
		int treesBetween(const Math::Vec3& from, const Math::Vec3& to) const;
		float groundAt(float x, float z) const;
		bool nearestShore(const Math::Vec3& from, float reach, Math::Vec3& shore) const;
		void updateOcclusion(Voice& voice, const Math::Vec3& listener);
		void apply(Voice& voice, float dt, float fadeIn, float fadeOut, bool spatial, float minDistance, float maxDistance);
		void updateBirds(const Math::Vec3& listener, double now, float dt);
		void updateFrogs(const Math::Vec3& listener, double now, float dt);

		bool mInited = false;
		bool mActive = false;
		ComponentsModule::SoundscapeComponent mScape;
		ComponentsModule::TerrainComponent mTerrain;
		Math::Vec3 mTerrainPos{};
		// terrainHeights, looked up once: finding them hashes the terrain's whole description.
		const std::vector<float>* mHeights = nullptr;
		float mWaterLevel = 0.f;
		bool mHasWater = false;
		AudioSystem* mAudio = nullptr;
		VegetationSystem* mVegetation = nullptr;
		std::mt19937 mRandom;
		Voice mWind;
		Voice mLake;
		std::vector<Bird> mBirds;
		std::vector<Frog> mFrogs;
		float mOcclusionTimer = 0.f;
		// Per FootstepSurface, per clip: the steps found in it. Decoding the recordings takes a moment, so it runs off
		// the main thread from the start, and steps are silent until it is done.
		using Steps = std::vector<std::vector<std::vector<std::pair<float, float>>>>;
		std::future<Steps> mStepsLoading;
		Steps mSteps;
		size_t mLastStep = ~size_t{ 0 };
	};
}
