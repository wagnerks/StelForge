#include "SimulatedTransport.h"

#include <algorithm>

#include "core/Engine.h"

namespace SFE::Network {
	bool SimulatedTransport::delay(Channel channel, double& at, double& lastReliable) {
		const double now = Engine::clockSeconds();
		std::uniform_real_distribution<float> unit(0.f, 1.f);
		const float jitter = (unit(mRandom) * 2.f - 1.f) * mConditions.jitterMs;
		at = now + (std::max)(0.f, mConditions.lagMs * 0.5f + jitter) / 1000.0;
		if (unit(mRandom) * 100.f < mConditions.lossPercent) {
			if (channel != Channel::Reliable) {
				return false;
			}
			// Resent once it is missed: a round trip later.
			at += (std::max)(mConditions.lagMs, 20.f) / 1000.0;
		}
		if (channel == Channel::Reliable) {
			at = (std::max)(at, lastReliable);
			lastReliable = at;
		}
		return true;
	}

	void SimulatedTransport::insert(std::deque<Waiting>& queue, Waiting&& waiting) {
		const auto it = std::upper_bound(queue.begin(), queue.end(), waiting.at, [](double at, const Waiting& other) { return at < other.at; });
		queue.insert(it, std::move(waiting));
	}

	void SimulatedTransport::send(Peer peer, Channel channel, const void* data, size_t size) {
		if (!mConditions.any()) {
			mInner->send(peer, channel, data, size);
			return;
		}
		Waiting waiting;
		waiting.channel = channel;
		waiting.event.peer = peer;
		const auto* bytes = static_cast<const uint8_t*>(data);
		waiting.event.data.assign(bytes, bytes + size);
		if (delay(channel, waiting.at, mLastReliableOut)) {
			insert(mOutgoing, std::move(waiting));
		}
	}

	void SimulatedTransport::poll(std::vector<Event>& events) {
		const double now = Engine::clockSeconds();
		// What has waited long enough goes; then the transport sends it, along with what else is queued.
		while (!mOutgoing.empty() && mOutgoing.front().at <= now) {
			const auto& waiting = mOutgoing.front();
			mInner->send(waiting.event.peer, waiting.channel, waiting.event.data.data(), waiting.event.data.size());
			mOutgoing.pop_front();
		}
		mArrived.clear();
		mInner->poll(mArrived);
		// What arrived waits as it would have on the way; a join or a leave as a reliable message.
		for (auto& event : mArrived) {
			if (!mConditions.any() && mIncoming.empty()) {
				events.push_back(std::move(event));
				continue;
			}
			Waiting waiting;
			waiting.channel = event.type == Event::Type::Received ? event.channel : Channel::Reliable;
			waiting.event = std::move(event);
			if (delay(waiting.channel, waiting.at, mLastReliableIn)) {
				insert(mIncoming, std::move(waiting));
			}
		}
		while (!mIncoming.empty() && mIncoming.front().at <= now) {
			events.push_back(std::move(mIncoming.front().event));
			mIncoming.pop_front();
		}
	}
}
