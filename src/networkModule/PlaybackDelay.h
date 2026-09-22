#pragma once

#include <algorithm>
#include <array>

namespace SFE::Network {
	// How far behind the host's clock to play back what another machine sends (StateBuffer::sample), so that there is
	// nearly always a newer state than the moment shown, whatever the network does.
	//
	// Each state that arrives says how late it is: the host's clock now, less its stamp. The delay aims at nearly the
	// latest of the recent ones (kPercentile of the last kSamples) and one gap between sends more, as the newest state
	// has to last until the next arrives. It grows quickly, when the network gets worse, and shrinks slowly, so the
	// playback does not visibly slow down and hurry again with every late packet.
	class PlaybackDelay {
	public:
		// `sendInterval`: seconds between the states the other machine sends.
		explicit PlaybackDelay(double sendInterval) : mSendInterval(sendInterval), mDelay(kFloor + sendInterval) {}

		// A state stamped `stamped` arrived when the host's clock read `now`.
		void arrived(double stamped, double now) {
			mSamples[mNext] = now - stamped;
			mNext = (mNext + 1) % kSamples;
			mCount = (std::min)(mCount + 1, kSamples);
		}

		// Once a frame: eased towards where it should be.
		void update(double dt) {
			if (mCount == 0) {
				return;
			}
			std::array<double, kSamples> sorted{};
			std::copy_n(mSamples.begin(), mCount, sorted.begin());
			const size_t at = static_cast<size_t>((mCount - 1) * kPercentile);
			std::nth_element(sorted.begin(), sorted.begin() + at, sorted.begin() + mCount);
			const double target = std::clamp(sorted[at] + mSendInterval + kMargin, kFloor + mSendInterval, kCeiling);
			const double rate = target > mDelay ? kGrowRate : kShrinkRate;
			const double step = rate * dt;
			mDelay = target > mDelay ? (std::min)(target, mDelay + step) : (std::max)(target, mDelay - step);
		}

		// Seconds behind the host's clock to play back at.
		double delay() const { return mDelay; }

	private:
		static constexpr size_t kSamples = 90;         // three seconds at 30 a second
		static constexpr double kPercentile = 0.95;
		static constexpr double kMargin = 0.01;        // seconds
		static constexpr double kFloor = 0.02;         // seconds over one send gap, however good the network
		static constexpr double kCeiling = 1.0;        // seconds: past this, shown late is worse than shown jerky
		static constexpr double kGrowRate = 0.5;       // seconds of delay gained per second
		static constexpr double kShrinkRate = 0.05;    // and lost: playback runs 5% fast while catching up

		double mSendInterval = 0.0;
		double mDelay = 0.0;
		std::array<double, kSamples> mSamples{};
		size_t mNext = 0;
		size_t mCount = 0;
	};
}
