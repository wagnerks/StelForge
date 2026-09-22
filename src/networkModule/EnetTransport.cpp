#include "EnetTransport.h"

#include <enet/enet.h>

#include "logsModule/logger.h"

namespace SFE::Network {
	namespace {
		constexpr size_t kChannels = static_cast<size_t>(Transport::Channel::Count);
	}

	EnetTransport::EnetTransport() {
		mInitialised = enet_initialize() == 0;
		if (!mInitialised) {
			LogsModule::Logger::LOG_ERROR("network: ENet did not start");
		}
	}

	EnetTransport::~EnetTransport() {
		if (mHost) {
			for (size_t i = 0; i < mHost->peerCount; ++i) {
				if (mHost->peers[i].state == ENET_PEER_STATE_CONNECTED) {
					enet_peer_disconnect_now(&mHost->peers[i], 0);
				}
			}
			enet_host_flush(mHost);
			enet_host_destroy(mHost);
		}
		if (mInitialised) {
			enet_deinitialize();
		}
	}

	bool EnetTransport::host(uint16_t port, uint32_t maxPeers) {
		if (!mInitialised || mHost) {
			return false;
		}
		ENetAddress address{};
		address.host = ENET_HOST_ANY;
		address.port = port;
		mHost = enet_host_create(&address, maxPeers, kChannels, 0, 0);
		if (!mHost) {
			LogsModule::Logger::LOG_ERROR("network: cannot listen on port %u", static_cast<unsigned>(port));
		}
		return mHost != nullptr;
	}

	bool EnetTransport::connect(const std::string& address, uint16_t port) {
		if (!mInitialised || mHost) {
			return false;
		}
		mHost = enet_host_create(nullptr, 1, kChannels, 0, 0);
		if (!mHost) {
			return false;
		}
		ENetAddress to{};
		if (enet_address_set_host(&to, address.c_str()) != 0) {
			LogsModule::Logger::LOG_ERROR("network: cannot resolve %s", address.c_str());
			return false;
		}
		to.port = port;
		return enet_host_connect(mHost, &to, kChannels, 0) != nullptr;
	}

	void EnetTransport::send(Peer peer, Channel channel, const void* data, size_t size) {
		if (!mHost || peer >= mHost->peerCount || mHost->peers[peer].state != ENET_PEER_STATE_CONNECTED) {
			return;
		}
		const enet_uint32 flags = channel == Channel::Reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
		ENetPacket* packet = enet_packet_create(data, size, flags);
		if (enet_peer_send(&mHost->peers[peer], static_cast<enet_uint8>(channel), packet) != 0) {
			enet_packet_destroy(packet);
		}
	}

	void EnetTransport::poll(std::vector<Event>& events) {
		if (!mHost) {
			return;
		}
		ENetEvent event;
		while (enet_host_service(mHost, &event, 0) > 0) {
			Event out;
			out.peer = static_cast<Peer>(event.peer - mHost->peers);
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				out.type = Event::Type::Connected;
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
				out.type = Event::Type::Disconnected;
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				out.type = Event::Type::Received;
				out.channel = static_cast<Channel>(event.channelID);
				out.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
				enet_packet_destroy(event.packet);
				break;
			default:
				continue;
			}
			events.push_back(std::move(out));
		}
		enet_host_flush(mHost);
	}

	void EnetTransport::disconnect(Peer peer) {
		if (mHost && peer < mHost->peerCount) {
			enet_peer_disconnect(&mHost->peers[peer], 0);
		}
	}

	float EnetTransport::roundTrip(Peer peer) const {
		return mHost && peer < mHost->peerCount ? static_cast<float>(mHost->peers[peer].roundTripTime) / 1000.f : 0.f;
	}
}
