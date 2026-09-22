#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SFE::Network {
	// How messages travel between machines, whatever carries them: UDP by address now (EnetTransport), Steam's
	// sockets later. A peer is a small number the transport hands out; a client has one, the host it joined.
	class Transport {
	public:
		using Peer = uint32_t;

		enum class Channel : uint8_t {
			Reliable,   // arrives, and in order: joins, leaves, actions, what changed once
			Unreliable, // the newest wins, older ones may be lost: states sent many times a second
			Count
		};

		struct Event {
			enum class Type : uint8_t { Connected, Disconnected, Received };
			Type type = Type::Received;
			Peer peer = 0;
			Channel channel = Channel::Reliable; // what a received message came on
			std::vector<uint8_t> data;
		};

		virtual ~Transport() = default;

		// Listens for up to `maxPeers` joining on `port`.
		virtual bool host(uint16_t port, uint32_t maxPeers) = 0;
		// Starts joining the host at `address`; Connected arrives through poll once it has.
		virtual bool connect(const std::string& address, uint16_t port) = 0;
		virtual void send(Peer peer, Channel channel, const void* data, size_t size) = 0;
		// Everything that happened since the last poll; sends what was queued since.
		virtual void poll(std::vector<Event>& events) = 0;
		virtual void disconnect(Peer peer) = 0;
		// Seconds a message takes there and back, as the transport measures it.
		virtual float roundTrip(Peer peer) const = 0;
	};
}
