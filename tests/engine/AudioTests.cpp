#include <cstdint>
#include <filesystem>
#include <fstream>

#include "EngineTestFixture.h"
#include "componentsModule/AudioSourceComponent.h"
#include "componentsModule/TransformComponent.h"
#include "ecss/Registry.h"
#include "systemsModule/systems/AudioSystem.h"

namespace SFE::EngineTests {
	using ComponentsModule::TransformComponent;

	namespace {
		// A second of silence: 16-bit mono PCM.
		std::filesystem::path writeWav() {
			constexpr uint32_t kRate = 48000;
			constexpr uint32_t kDataSize = kRate * 2;
			const auto path = std::filesystem::temp_directory_path() / "sfe_audio_test.wav";
			std::ofstream out(path, std::ios::binary);
			const auto u32 = [&](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
			const auto u16 = [&](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
			out.write("RIFF", 4); u32(36 + kDataSize); out.write("WAVE", 4);
			out.write("fmt ", 4); u32(16); u16(1); u16(1); u32(kRate); u32(kRate * 2); u16(2); u16(16);
			out.write("data", 4); u32(kDataSize);
			out.write(std::string(kDataSize, '\0').data(), kDataSize);
			return path;
		}

		SystemsModule::AudioSystem* audio() {
			return ECSHandler::systemManager().getSystem<SystemsModule::AudioSystem>();
		}
	}

	// A source plays while its flag is set, and stops when the flag is cleared or the component goes away.
	TEST(AudioTest, SourceFollowsItsComponent) {
		ASSERT_TRUE(audio());
		const auto wav = writeWav();
		auto& registry = ECSHandler::registry();
		const auto id = registry.takeEntity();
		registry.addComponent<TransformComponent>(id, id);
		auto* source = registry.addComponent<AudioSourceComponent>(id);
		source->mPath = wav.string();
		source->mPlaying = true;

		step();
		EXPECT_TRUE(audio()->isPlaying(id));

		registry.pinComponent<AudioSourceComponent>(id)->mPlaying = false;
		step();
		EXPECT_FALSE(audio()->isPlaying(id));

		registry.pinComponent<AudioSourceComponent>(id)->mPlaying = true;
		step();
		EXPECT_TRUE(audio()->isPlaying(id));

		ECSHandler::destroyEntity(id);
		step();
		EXPECT_FALSE(audio()->isPlaying(id));
	}

	// A file that cannot be opened clears the flag instead of retrying every frame.
	TEST(AudioTest, MissingFileStopsTheSource) {
		ASSERT_TRUE(audio());
		auto& registry = ECSHandler::registry();
		const auto id = registry.takeEntity();
		auto* source = registry.addComponent<AudioSourceComponent>(id);
		source->mPath = "sounds/does_not_exist.wav";
		source->mPlaying = true;

		step();
		EXPECT_FALSE(audio()->isPlaying(id));
		EXPECT_FALSE(registry.pinComponent<const AudioSourceComponent>(id)->mPlaying);
		ECSHandler::destroyEntity(id);
	}
}
