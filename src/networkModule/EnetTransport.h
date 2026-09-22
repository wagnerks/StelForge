#pragma once

#include "Transport.h"

struct _ENetHost;

namespace SFE::Network {
	// Transport over UDP by address, with ENet's reliable and sequenced channels.
	class EnetTransport final : public Transport {
	public:
		EnetTransport();
		~EnetTransport() override;

		bool host(uint16_t port, uint32_t maxPeers) override;
		bool connect(const std::string& address, uint16_t port) override;
		void send(Peer peer, Channel channel, const void* data, size_t size) override;
		void poll(std::vector<Event>& events) override;
		void disconnect(Peer peer) override;
		float roundTrip(Peer peer) const override;

	private:
		_ENetHost* mHost = nullptr;
		bool mInitialised = false;
	};
}
