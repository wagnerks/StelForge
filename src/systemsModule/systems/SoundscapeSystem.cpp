#include "SoundscapeSystem.h"

#include <algorithm>
#include <cmath>

#include "AudioSystem.h"
#include "CameraSystem.h"
#include "VegetationSystem.h"
#include "assetsModule/modelModule/Terrain.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/WaterComponent.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "renderModule/Atmosphere.h"

namespace SFE::SystemsModule {
	namespace {
		// Trees within this many metres of the listener make up the forest it hears the wind in, and how many of them
		// there are in the thickest of it.
		constexpr float kForestRadius = 18.f;
		constexpr float kFullForest = 45.f;
		// A trunk and its crown muffle a sound whose line to the listener passes this close to the trunk.
		constexpr float kTreeWidth = 1.2f;
		// What each tree across the line leaves of the sound, and of its highs.
		constexpr float kGainPerTree = 0.9f;
		constexpr float kCutoffPerTree = 0.8f;
		constexpr float kMinCutoff = 700.f;
		constexpr float kOcclusionInterval = 0.2f; // seconds between updates of what is in the way

		constexpr float kLakeReach = 90.f;   // metres to the shore still heard
		constexpr size_t kBirds = 4;
		constexpr float kBirdNear = 12.f;
		constexpr float kBirdFar = 55.f;
		constexpr float kFrogSpacing = 28.f;
		constexpr size_t kMaxFrogs = 16;
		constexpr float kFrogHearing = 90.f;
		constexpr float kFrogStartle = 14.f; // they fall silent when someone comes this close
		constexpr float kFrogCalm = 24.f;    // and sing again once they are this far off, after a while

		float approach(float value, float target, float rate, float dt) {
			const float step = rate * dt;
			return value < target ? (std::min)(value + step, target) : (std::max)(value - step, target);
		}
	}

	bool SoundscapeSystem::init() {
		auto& registry = ECSHandler::registry();
		bool found = false;
		for (const auto& [id, scape, terrain, transform] : registry.view<const SoundscapeComponent, const ComponentsModule::TerrainComponent, const TransformComponent>()) {
			if (scape && terrain && transform) {
				mScape = *scape;
				mTerrain = *terrain;
				mTerrainPos = transform->getPos(true);
				mHeights = &AssetsModule::terrainHeights(mTerrain);
				found = true;
				break;
			}
		}
		if (!found) {
			return true;
		}
		for (const auto& [id, water, transform] : registry.view<const ComponentsModule::WaterComponent, const TransformComponent>()) {
			if (water && transform) {
				mWaterLevel = transform->getPos(true).y;
				mHasWater = true;
				break;
			}
		}
		mAudio = ECSHandler::getSystem<AudioSystem>();
		mVegetation = ECSHandler::getSystem<VegetationSystem>();
		if (!mAudio) {
			return true;
		}
		mRandom.seed(mScape.seed);
		if (!mScape.wind.empty()) {
			mWind.id = mAudio->startVoice(mScape.wind, true, true);
		}
		if (!mScape.lake.empty() && mHasWater) {
			mLake.id = mAudio->startVoice(mScape.lake, true, true);
		}
		mBirds.resize(mScape.birds.empty() ? 0 : kBirds);
		for (size_t i = 0; i < mBirds.size(); ++i) {
			mBirds[i].nextSong = Engine::clockSeconds() + 1.0 + 3.0 * static_cast<double>(i);
		}

		// Frogs in the shallows round the lake: spots a little under the water line, no two close together.
		if (mHasWater && !mScape.frogs.empty()) {
			const float half = mTerrain.size * 0.5f;
			std::vector<Math::Vec3> candidates;
			for (float z = -half; z < half; z += 5.f) {
				for (float x = -half; x < half; x += 5.f) {
					const float depth = mWaterLevel - groundAt(mTerrainPos.x + x, mTerrainPos.z + z);
					if (depth > 0.05f && depth < 0.45f) {
						candidates.push_back({ mTerrainPos.x + x, mWaterLevel, mTerrainPos.z + z });
					}
				}
			}
			std::shuffle(candidates.begin(), candidates.end(), mRandom);
			for (const auto& spot : candidates) {
				if (mFrogs.size() >= kMaxFrogs) {
					break;
				}
				const bool crowded = std::any_of(mFrogs.begin(), mFrogs.end(), [&](const Frog& frog) {
					return Math::length(frog.pos - spot) < kFrogSpacing;
				});
				if (!crowded) {
					mFrogs.push_back({ spot });
				}
			}
		}
		if (!mScape.footsteps.empty()) {
			mStepsLoading = std::async(std::launch::async, [audio = mAudio, surfaces = mScape.footsteps] {
				Steps steps(surfaces.size());
				for (size_t i = 0; i < surfaces.size(); ++i) {
					for (const auto& clip : surfaces[i].clips) {
						steps[i].push_back(audio->findHits(clip, surfaces[i].maxLength));
					}
				}
				return steps;
			});
		}
		mActive = true;
		return true;
	}

	float SoundscapeSystem::groundAt(const float x, const float z) const {
		return mTerrainPos.y + AssetsModule::terrainSurfaceHeight(mTerrain, *mHeights, x - mTerrainPos.x, z - mTerrainPos.z);
	}

	float SoundscapeSystem::forestAround(const Math::Vec3& at, const float radius) const {
		if (!mVegetation) {
			return 0.f;
		}
		const auto& placements = mVegetation->placements();
		const auto& layers = mVegetation->vegetation().layers;
		if (placements.instances.empty()) {
			return 0.f;
		}
		const float cell = placements.cellSize;
		const int side = placements.cellsPerSide;
		const int x0 = (std::max)(0, static_cast<int>(std::floor((at.x - radius - placements.origin.x) / cell)));
		const int x1 = (std::min)(side - 1, static_cast<int>(std::floor((at.x + radius - placements.origin.x) / cell)));
		const int z0 = (std::max)(0, static_cast<int>(std::floor((at.z - radius - placements.origin.y) / cell)));
		const int z1 = (std::min)(side - 1, static_cast<int>(std::floor((at.z + radius - placements.origin.y) / cell)));
		int trees = 0;
		for (int cz = z0; cz <= z1; ++cz) {
			for (int cx = x0; cx <= x1; ++cx) {
				const size_t index = static_cast<size_t>(cz) * side + cx;
				for (uint32_t i = placements.cellStart[index]; i < placements.cellStart[index + 1]; ++i) {
					const auto& tree = placements.instances[i];
					if (layers[tree.layer].canopy <= 0.f) {
						continue;
					}
					const float dx = tree.pos.x - at.x;
					const float dz = tree.pos.z - at.z;
					trees += dx * dx + dz * dz < radius * radius ? 1 : 0;
				}
			}
		}
		return (std::min)(static_cast<float>(trees) / kFullForest, 1.f);
	}

	int SoundscapeSystem::treesBetween(const Math::Vec3& from, const Math::Vec3& to) const {
		if (!mVegetation) {
			return 0;
		}
		const auto& placements = mVegetation->placements();
		const auto& layers = mVegetation->vegetation().layers;
		if (placements.instances.empty()) {
			return 0;
		}
		const float cell = placements.cellSize;
		const int side = placements.cellsPerSide;
		const float ax = from.x;
		const float az = from.z;
		const float bx = to.x - from.x;
		const float bz = to.z - from.z;
		const float length2 = (std::max)(bx * bx + bz * bz, 1e-4f);
		const int x0 = (std::max)(0, static_cast<int>(std::floor(((std::min)(from.x, to.x) - kTreeWidth - placements.origin.x) / cell)));
		const int x1 = (std::min)(side - 1, static_cast<int>(std::floor(((std::max)(from.x, to.x) + kTreeWidth - placements.origin.x) / cell)));
		const int z0 = (std::max)(0, static_cast<int>(std::floor(((std::min)(from.z, to.z) - kTreeWidth - placements.origin.y) / cell)));
		const int z1 = (std::min)(side - 1, static_cast<int>(std::floor(((std::max)(from.z, to.z) + kTreeWidth - placements.origin.y) / cell)));
		int trees = 0;
		for (int cz = z0; cz <= z1; ++cz) {
			for (int cx = x0; cx <= x1; ++cx) {
				const size_t index = static_cast<size_t>(cz) * side + cx;
				for (uint32_t i = placements.cellStart[index]; i < placements.cellStart[index + 1]; ++i) {
					const auto& tree = placements.instances[i];
					if (layers[tree.layer].canopy <= 0.f) {
						continue;
					}
					// Along the line, not at either end: a bird's own tree does not muffle it.
					const float t = ((tree.pos.x - ax) * bx + (tree.pos.z - az) * bz) / length2;
					if (t <= 0.05f || t >= 0.95f) {
						continue;
					}
					const float px = ax + bx * t - tree.pos.x;
					const float pz = az + bz * t - tree.pos.z;
					trees += px * px + pz * pz < kTreeWidth * kTreeWidth ? 1 : 0;
				}
			}
		}
		return trees;
	}

	bool SoundscapeSystem::nearestShore(const Math::Vec3& from, const float reach, Math::Vec3& shore) const {
		// Straight out in a ring of directions, a couple of metres a step: the first water each way, the nearest of them.
		constexpr int kDirections = 24;
		constexpr float kStep = 2.f;
		float best = reach;
		bool found = false;
		if (groundAt(from.x, from.z) < mWaterLevel) {
			shore = { from.x, mWaterLevel, from.z };
			return true;
		}
		for (int d = 0; d < kDirections; ++d) {
			const float angle = static_cast<float>(d) / kDirections * 6.2831853f;
			const float dx = std::cos(angle);
			const float dz = std::sin(angle);
			for (float r = kStep; r < best; r += kStep) {
				if (groundAt(from.x + dx * r, from.z + dz * r) < mWaterLevel) {
					best = r;
					shore = { from.x + dx * r, mWaterLevel, from.z + dz * r };
					found = true;
					break;
				}
			}
		}
		return found;
	}

	void SoundscapeSystem::updateOcclusion(Voice& voice, const Math::Vec3& listener) {
		const int trees = treesBetween(listener, voice.pos);
		voice.occlusion = std::pow(kGainPerTree, static_cast<float>(trees));
		voice.cutoff = (std::max)(20000.f * std::pow(kCutoffPerTree, static_cast<float>(trees)), kMinCutoff);
	}

	void SoundscapeSystem::apply(Voice& voice, const float dt, const float fadeIn, const float fadeOut, const bool spatial, const float minDistance,
		const float maxDistance) {
		if (!voice.id) {
			return;
		}
		voice.gain = approach(voice.gain, voice.target, voice.gain < voice.target ? fadeIn : fadeOut, dt);
		AudioSystem::VoiceParams params;
		params.pos = voice.pos;
		params.volume = voice.gain * voice.occlusion;
		params.cutoff = voice.cutoff;
		params.spatial = spatial;
		params.minDistance = minDistance;
		params.maxDistance = maxDistance;
		mAudio->setVoice(voice.id, params);
	}

	void SoundscapeSystem::updateBirds(const Math::Vec3& listener, const double now, const float dt) {
		if (!mVegetation) {
			return;
		}
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		// By reference: a conditional that could also yield a temporary copied all the placements every frame.
		const auto& placements = mVegetation->placements();
		const auto& layers = mVegetation->vegetation().layers;
		for (auto& bird : mBirds) {
			if (bird.voice.id && mAudio->voiceEnded(bird.voice.id)) {
				mAudio->stopVoice(bird.voice.id);
				bird.voice.id = 0;
			}
			if (bird.voice.id) {
				apply(bird.voice, dt, 100.f, 100.f, true, 4.f, 80.f);
				continue;
			}
			if (now < bird.nextSong) {
				continue;
			}
			// Off to another tree once it has sung its fill here, or when the listener has gone far from it.
			if (!bird.placed || bird.songsLeft <= 0 || Math::length(bird.voice.pos - listener) > kBirdFar * 1.3f) {
				bird.placed = false;
				if (!placements.instances.empty()) {
					for (int attempt = 0; attempt < 12 && !bird.placed; ++attempt) {
						const float angle = unit(mRandom) * 6.2831853f;
						const float distance = kBirdNear + (kBirdFar - kBirdNear) * unit(mRandom);
						const Math::Vec3 around{ listener.x + std::cos(angle) * distance, listener.y, listener.z + std::sin(angle) * distance };
						const int cx = static_cast<int>(std::floor((around.x - placements.origin.x) / placements.cellSize));
						const int cz = static_cast<int>(std::floor((around.z - placements.origin.y) / placements.cellSize));
						if (cx < 0 || cz < 0 || cx >= placements.cellsPerSide || cz >= placements.cellsPerSide) {
							continue;
						}
						const size_t index = static_cast<size_t>(cz) * placements.cellsPerSide + cx;
						const uint32_t first = placements.cellStart[index];
						const uint32_t count = placements.cellStart[index + 1] - first;
						for (uint32_t tries = 0; tries < (std::min)(count, 16u); ++tries) {
							const auto& tree = placements.instances[first + static_cast<uint32_t>(unit(mRandom) * static_cast<float>(count)) % count];
							if (layers[tree.layer].canopy <= 0.f) {
								continue;
							}
							// Up in the crown: the kit's grown trees stand six to nine hundred of their units tall.
							bird.voice.pos = tree.pos + Math::Vec3{ 0.f, tree.scale * (400.f + 300.f * unit(mRandom)), 0.f };
							bird.placed = true;
							break;
						}
					}
				}
				if (!bird.placed) {
					bird.nextSong = now + 3.0;
					continue;
				}
				bird.songsLeft = 3 + static_cast<int>(unit(mRandom) * 6.f);
			}
			const auto& song = mScape.birds[static_cast<size_t>(unit(mRandom) * static_cast<float>(mScape.birds.size())) % mScape.birds.size()];
			bird.voice.id = mAudio->startVoice(song, false, false);
			bird.voice.target = mScape.birdVolume * (0.6f + 0.4f * unit(mRandom));
			bird.voice.gain = bird.voice.target;
			updateOcclusion(bird.voice, listener);
			apply(bird.voice, dt, 100.f, 100.f, true, 4.f, 80.f);
			--bird.songsLeft;
			bird.nextSong = now + 2.0 + 9.0 * unit(mRandom);
		}
	}

	void SoundscapeSystem::updateFrogs(const Math::Vec3& listener, const double now, const float dt) {
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		for (size_t i = 0; i < mFrogs.size(); ++i) {
			auto& frog = mFrogs[i];
			const float distance = Math::length(frog.pos - listener);
			if (distance > kFrogHearing) {
				if (frog.voice.id) {
					mAudio->stopVoice(frog.voice.id);
					frog.voice = {};
				}
				continue;
			}
			if (!frog.voice.id) {
				frog.voice.id = mAudio->startVoice(mScape.frogs[i % mScape.frogs.size()], true, true);
				frog.voice.pos = frog.pos;
				frog.voice.gain = 0.f;
				updateOcclusion(frog.voice, listener);
			}
			// Hushed by someone coming near, one after another rather than all at once; they start up again a while
			// after the listener has moved off.
			if (distance < kFrogStartle) {
				frog.silent = true;
				frog.quietUntil = now + 6.0 + 12.0 * unit(mRandom);
			}
			else if (frog.silent && distance > kFrogCalm && now > frog.quietUntil) {
				frog.silent = false;
			}
			frog.voice.target = frog.silent ? 0.f : mScape.frogVolume;
			apply(frog.voice, dt, 0.08f, 1.2f + 0.3f * static_cast<float>(i % 3), true, 3.f, kFrogHearing);
		}
	}

	std::string_view SoundscapeSystem::surfaceAt(const Math::Vec3& feet) const {
		const float ground = groundAt(feet.x, feet.z);
		if (mHasWater && mWaterLevel - ground > 0.08f) {
			return "water";
		}
		if (mHasWater && ground - mWaterLevel < 0.5f) {
			return "bank";
		}
		if (std::fabs(AssetsModule::terrainRoadDistance(mTerrain, feet.x - mTerrainPos.x, feet.z - mTerrainPos.z)) < 1.6f) {
			return "road";
		}
		// Two or more trees within a few steps: the needles and twigs under them.
		return forestAround(feet, 6.f) * kFullForest >= 2.f ? "forest" : "grass";
	}

	void SoundscapeSystem::footstep(const Math::Vec3& feet, const float speed) {
		if (!mActive || !mAudio || mScape.footsteps.empty()) {
			return;
		}
		const auto surface = surfaceAt(feet);
		size_t index = 0;
		while (index < mScape.footsteps.size() && mScape.footsteps[index].surface != surface) {
			++index;
		}
		if (index == mScape.footsteps.size()) {
			index = 0;
		}
		const auto& steps = mScape.footsteps[index];
		if (steps.clips.empty()) {
			return;
		}
		if (mStepsLoading.valid() && mStepsLoading.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			mSteps = mStepsLoading.get();
		}
		if (index >= mSteps.size()) {
			return;
		}
		const auto& found = mSteps[index];
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		// Any step of any of its recordings, but not the one just played.
		size_t total = 0;
		for (const auto& hits : found) {
			total += hits.size();
		}
		if (total == 0) {
			return;
		}
		size_t pick = static_cast<size_t>(unit(mRandom) * static_cast<float>(total)) % total;
		if (pick == mLastStep && total > 1) {
			pick = (pick + 1) % total;
		}
		mLastStep = pick;
		for (size_t clip = 0; clip < found.size(); ++clip) {
			if (pick < found[clip].size()) {
				const auto [start, length] = found[clip][pick];
				const float volume = steps.volume * std::clamp(0.55f + 0.12f * speed, 0.6f, 1.2f) * (0.85f + 0.15f * unit(mRandom));
				mAudio->playSlice(steps.clips[clip], start, length, feet, volume, 0.94f + 0.12f * unit(mRandom));
				return;
			}
			pick -= found[clip].size();
		}
	}

	void SoundscapeSystem::update(const float dt) {
		if (!mInited) {
			mInited = init();
		}
		if (!mActive || !mAudio) {
			return;
		}
		auto* cameras = ECSHandler::getSystem<CameraSystem>();
		if (!cameras) {
			return;
		}
		Math::Vec3 listener;
		{
			const auto transform = ECSHandler::registry().pinComponent<const TransformComponent>(cameras->getCurrentCamera());
			if (!transform) {
				return;
			}
			listener = transform->getPos(true);
		}
		const double now = Engine::clockSeconds();
		mOcclusionTimer -= dt;
		const bool refresh = mOcclusionTimer <= 0.f;
		if (refresh) {
			mOcclusionTimer = kOcclusionInterval;
		}

		// The wind in the trees, all round: as loud as the forest is thick and the wind blows.
		if (mWind.id && refresh) {
			const auto wind = Render::Atmosphere::sceneWind();
			const float strength = wind ? wind->strength : 0.5f;
			mWind.target = mScape.windVolume * (0.2f + 0.8f * forestAround(listener, kForestRadius)) * (0.4f + 0.6f * strength);
		}
		apply(mWind, dt, 0.3f, 0.3f, false, 1.f, 1.f);

		// The lake at the nearest water, moving along the shore with the listener.
		if (mLake.id && refresh) {
			Math::Vec3 shore;
			if (nearestShore(listener, kLakeReach, shore)) {
				mLake.pos = mLake.target > 0.f ? mLake.pos + (shore - mLake.pos) * 0.5f : shore;
				mLake.target = mScape.lakeVolume;
				updateOcclusion(mLake, listener);
			}
			else {
				mLake.target = 0.f;
			}
		}
		apply(mLake, dt, 0.5f, 0.5f, true, 3.f, kLakeReach);

		if (!mScape.birds.empty()) {
			updateBirds(listener, now, dt);
			if (refresh) {
				for (auto& bird : mBirds) {
					if (bird.voice.id) {
						updateOcclusion(bird.voice, listener);
					}
				}
			}
		}
		if (!mScape.frogs.empty()) {
			updateFrogs(listener, now, dt);
			if (refresh) {
				for (auto& frog : mFrogs) {
					if (frog.voice.id) {
						updateOcclusion(frog.voice, listener);
					}
				}
			}
		}
	}
}
