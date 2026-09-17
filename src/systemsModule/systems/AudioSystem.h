#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mathModule/Forward.h"
#include "systemsModule/SystemBase.h"

struct ma_engine;
struct ma_sound;

namespace SFE::SystemsModule {
	// Plays AudioSourceComponents and one-shot sounds through miniaudio, heard from the AudioListenerComponent or,
	// without one, the current camera. Runs on the main thread after the camera has moved.
	//
	// On the None backend, or when there is no audio device, it mixes into nothing: sounds start and report
	// playing, but never advance, so a sound that does not loop never ends there.
	class AudioSystem : public ecss::System {
	public:
		AudioSystem();
		~AudioSystem() override;

		void update(float dt) override;

		// Fire and forget, at a point in the world.
		void playOneShot(std::string_view path, const Math::Vec3& pos, float volume = 1.f);
		// Fire and forget, a piece of a recording: `length` seconds from `start`, faded at both ends.
		void playSlice(std::string_view path, float start, float length, const Math::Vec3& pos, float volume, float pitch);
		// The separate hits in a recording of one sound after another -- the steps in a recording of someone walking
		// -- as start and length in seconds, each at most `maxLength` long. Decodes the file; cache what it returns.
		std::vector<std::pair<float, float>> findHits(std::string_view path, float maxLength);
		bool isPlaying(ecss::EntityId entity) const;

		// Voices: sounds another system drives itself, each through a low-pass filter so what stands between it and
		// the listener can muffle it. `stream` for long loops. 0 when the file cannot be opened.
		struct VoiceParams {
			Math::Vec3 pos{};
			float volume = 1.f;
			float minDistance = 1.f;
			float maxDistance = 100.f;
			float cutoff = 20000.f; // Hz
			bool spatial = true;
		};
		uint32_t startVoice(std::string_view path, bool loop, bool stream);
		void setVoice(uint32_t voice, const VoiceParams& params);
		// A voice that does not loop has played to its end, or is gone.
		bool voiceEnded(uint32_t voice) const;
		void stopVoice(uint32_t voice);
		void setMasterVolume(float volume);
		float masterVolume() const { return mMasterVolume; }

	private:
		struct EngineDeleter { void operator()(ma_engine* engine) const; };
		struct SoundDeleter { void operator()(ma_sound* sound) const; };
		using Sound = std::unique_ptr<ma_sound, SoundDeleter>;

		struct Source {
			Sound sound;
			std::string path;
			bool stream = false;
			uint64_t seenFrame = 0;
		};

		Sound loadSound(std::string_view path, bool stream);
		void updateListener();

		// Declared first so it is destroyed last: every sound is uninitialised against it.
		std::unique_ptr<ma_engine, EngineDeleter> mEngine;
		std::unordered_map<ecss::EntityId, Source> mSources;
		std::vector<Sound> mOneShots;
		struct Voice;
		struct VoiceDeleter { void operator()(Voice* voice) const; };
		std::unordered_map<uint32_t, std::unique_ptr<Voice, VoiceDeleter>> mVoices;
		uint32_t mNextVoice = 1;
		uint64_t mFrame = 0;
		float mMasterVolume = 1.f;
	};
}
