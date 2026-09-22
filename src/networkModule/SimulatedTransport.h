#pragma once

#include <deque>
#include <memory>
#include <random>

#include "NetworkSettings.h"
#include "Transport.h"

namespace SFE::Network {
	// A transport as a worse network would carry it, for trying the game over one without leaving the desk: every
	// message this machine sends or receives waits half the added round trip, give or take the jitter, and some are
	// lost. A lost unreliable message is gone; a lost reliable one arrives a round trip later, as a resend would. What
	// goes reliably keeps its order; what goes unreliably may overtake. Joins and leaves travel as reliable messages.
	//
	// Wraps the transport that really carries them; with nothing set it passes everything straight through.
	class SimulatedTransport final : public Transport {
	public:
		explicit SimulatedTransport(std::unique_ptr<Transport> inner) : mInner(std::move(inner)) {}

		void setConditions(const Conditions& conditions) { mConditions = conditions; }
		const Conditions& conditions() const { return mConditions; }

		bool host(uint16_t port, uint32_t maxPeers) override { return mInner->host(port, maxPeers); }
		bool connect(const std::string& address, uint16_t port) override { return mInner->connect(address, port); }
		void send(Peer peer, Channel channel, const void* data, size_t size) override;
		void poll(std::vector<Event>& events) override;
		void disconnect(Peer peer) override { mInner->disconnect(peer); }
		float roundTrip(Peer peer) const override { return mInner->roundTrip(peer) + mConditions.lagMs / 1000.f; }

	private:
		struct Waiting {
			double at = 0.0; // Engine::clockSeconds() it goes on
			Channel channel = Channel::Reliable;
			Event event;     // for a send: the peer and the bytes
		};

		// When a message sent or received now is let through, or false for one that is lost.
		bool delay(Channel channel, double& at, double& lastReliable);
		static void insert(std::deque<Waiting>& queue, Waiting&& waiting);

		std::unique_ptr<Transport> mInner;
		Conditions mConditions;
		std::mt19937 mRandom{ 0x5eed };
		std::deque<Waiting> mOutgoing; // by time
		std::deque<Waiting> mIncoming; // by time
		double mLastReliableOut = 0.0;
		double mLastReliableIn = 0.0;
		std::vector<Event> mArrived;
	};
}
