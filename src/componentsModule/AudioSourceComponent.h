#pragma once

#include <string>

#include "ecss/Types.h"

namespace SFE::ComponentsModule {
	// A sound the entity plays from where its transform is. AudioSystem owns the playing sound; this is only the
	// request, so it can be set from anywhere on the main thread.
	struct AudioSourceComponent {
		// Resolved through FileSystem. wav, mp3 and flac.
		std::string mPath;
		float mVolume = 1.f;
		float mPitch = 1.f;
		// Full volume up to mMinDistance, fading out towards mMaxDistance.
		float mMinDistance = 1.f;
		float mMaxDistance = 100.f;
		bool mLoop = false;
		// Off: plays the same wherever the listener is. Music and UI.
		bool mSpatial = true;
		// Decodes while playing instead of up front: for long music, not for short effects.
		bool mStream = false;
		// Set to play from the start, clear to stop. The system clears it when a sound that does not loop ends,
		// or when the file cannot be opened.
		bool mPlaying = false;
	};
}

// The path is a string. There are few sound sources, so their array can do without memmove.
template<> struct ecss::AllowNonTrivial<SFE::ComponentsModule::AudioSourceComponent> : std::true_type {};

using SFE::ComponentsModule::AudioSourceComponent;
