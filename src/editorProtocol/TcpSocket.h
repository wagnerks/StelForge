#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace SFE::EditorProtocol {

	class TcpSocket {
	public:
		TcpSocket() = default;
		~TcpSocket();

		TcpSocket(const TcpSocket&) = delete;
		TcpSocket& operator=(const TcpSocket&) = delete;

		static bool initNetwork();
		static void shutdownNetwork();

		bool listen(uint16_t port);
		bool acceptClient(TcpSocket& client);
		bool connectTo(const char* host, uint16_t port);
		void close();

		bool isConnected() const { return mConnected; }
		// An accepted client on this same machine.
		bool isLoopback() const { return mLoopback; }
		bool sendAll(const uint8_t* data, size_t size);
		bool recvSome(std::vector<uint8_t>& buffer, size_t maxBytes, size_t& received);
		void setReceiveBufferSize(int bytes);

		uintptr_t nativeHandle() const { return mSocket; }

	private:
		uintptr_t mSocket = static_cast<uintptr_t>(-1);
		uintptr_t mListenSocket = static_cast<uintptr_t>(-1);
		bool mConnected = false;
		bool mLoopback = false;
		std::mutex mSendMutex;
	};

} // namespace SFE::EditorProtocol
