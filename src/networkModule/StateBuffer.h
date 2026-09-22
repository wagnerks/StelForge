#pragma once

#include <algorithm>
#include <deque>

namespace SFE::Network {
	// The last states of something another machine decides, each stamped with the host's clock as it was taken
	// (NetworkSystem::hostTime), played back a little in the past so there are always two to go between: what arrived
	// late or not at all is covered, and the motion is smooth instead of arriving in network-rate jumps.
	template <typename State>
	class StateBuffer {
	public:
		// Out-of-order arrivals are placed where they belong; ones older than what was already played are dropped.
		void push(double at, const State& state) {
			if (!mStates.empty() && at <= mPlayed) {
				return;
			}
			auto it = std::upper_bound(mStates.begin(), mStates.end(), at, [](double time, const Entry& entry) { return time < entry.at; });
			mStates.insert(it, { at, state });
			while (mStates.size() > kKept) {
				mStates.pop_front();
			}
		}

		bool empty() const { return mStates.empty(); }
		const State& newest() const { return mStates.back().state; }

		// The state at `at`, between the two around it by `lerp(from, to, t)`; before the first, the first, and past the
		// newest, the newest (held, not guessed). False while nothing has arrived.
		template <typename Lerp>
		bool sample(double at, State& out, Lerp&& lerp) {
			if (mStates.empty()) {
				return false;
			}
			mPlayed = at;
			if (at <= mStates.front().at) {
				out = mStates.front().state;
				return true;
			}
			if (at >= mStates.back().at) {
				out = mStates.back().state;
				return true;
			}
			auto to = std::upper_bound(mStates.begin(), mStates.end(), at, [](double time, const Entry& entry) { return time < entry.at; });
			auto from = to - 1;
			const double span = to->at - from->at;
			const float t = span > 1e-9 ? static_cast<float>((at - from->at) / span) : 1.f;
			out = lerp(from->state, to->state, t);
			// What is behind the pair played now will not be needed again.
			mStates.erase(mStates.begin(), from);
			return true;
		}

	private:
		struct Entry {
			double at = 0.0;
			State state;
		};
		static constexpr size_t kKept = 32;
		std::deque<Entry> mStates;
		double mPlayed = -1.0;
	};
}
