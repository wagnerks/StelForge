#include "TcpSocket.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using native_socket_t = SOCKET;
static constexpr native_socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using native_socket_t = int;
static constexpr native_socket_t kInvalidSocket = -1;
#endif

namespace SFE::EditorProtocol {

	static void tuneSocketBuffers(native_socket_t sock)
	{
		int bufSize = 16 * 1024 * 1024;
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
		setsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
#ifdef _WIN32
		DWORD sendTimeoutMs = 5000;
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeoutMs), sizeof(sendTimeoutMs));
#endif
	}

	static bool sNetworkReady = false;

	TcpSocket::~TcpSocket()
	{
		close();
	}

	bool TcpSocket::initNetwork()
	{
#ifdef _WIN32
		if (sNetworkReady) {
			return true;
		}
		WSADATA wsa{};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			return false;
		}
		sNetworkReady = true;
#endif
		return true;
	}

	void TcpSocket::shutdownNetwork()
	{
#ifdef _WIN32
		if (sNetworkReady) {
			WSACleanup();
			sNetworkReady = false;
		}
#endif
	}

	void TcpSocket::close()
	{
		if (mSocket != static_cast<uintptr_t>(kInvalidSocket)) {
#ifdef _WIN32
			closesocket(static_cast<native_socket_t>(mSocket));
#else
			::close(static_cast<native_socket_t>(mSocket));
#endif
			mSocket = static_cast<uintptr_t>(kInvalidSocket);
		}
		if (mListenSocket != static_cast<uintptr_t>(kInvalidSocket)) {
#ifdef _WIN32
			closesocket(static_cast<native_socket_t>(mListenSocket));
#else
			::close(static_cast<native_socket_t>(mListenSocket));
#endif
			mListenSocket = static_cast<uintptr_t>(kInvalidSocket);
		}
		mConnected = false;
	}

	bool TcpSocket::listen(uint16_t port)
	{
		close();
		if (!initNetwork()) {
			return false;
		}

		native_socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == kInvalidSocket) {
			return false;
		}

		int yes = 1;
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons(port);
		if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}
		if (::listen(sock, 1) != 0) {
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}

		mListenSocket = static_cast<uintptr_t>(sock);
		return true;
	}

	bool TcpSocket::acceptClient(TcpSocket& client)
	{
		if (mListenSocket == static_cast<uintptr_t>(kInvalidSocket)) {
			return false;
		}
		sockaddr_in addr{};
		socklen_t len = sizeof(addr);
		native_socket_t clientSock = ::accept(static_cast<native_socket_t>(mListenSocket), reinterpret_cast<sockaddr*>(&addr), &len);
		if (clientSock == kInvalidSocket) {
			return false;
		}
		client.close();
		client.mSocket = static_cast<uintptr_t>(clientSock);
		client.mConnected = true;
		client.mLoopback = (ntohl(addr.sin_addr.s_addr) >> 24) == 127;
		tuneSocketBuffers(clientSock);
#ifdef _WIN32
		{
			BOOL nodelay = TRUE;
			setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
		}
#endif
		return true;
	}

	bool TcpSocket::connectTo(const char* host, uint16_t port)
	{
		close();
		if (!initNetwork()) {
			return false;
		}

		native_socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == kInvalidSocket) {
			return false;
		}

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}

		if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
			closesocket(sock);
#else
			::close(sock);
#endif
			return false;
		}

		mSocket = static_cast<uintptr_t>(sock);
		mConnected = true;
		tuneSocketBuffers(sock);
#ifdef _WIN32
		{
			BOOL nodelay = TRUE;
			setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
		}
#endif
		return true;
	}

	bool TcpSocket::sendAll(const uint8_t* data, size_t size)
	{
		if (!mConnected || mSocket == static_cast<uintptr_t>(kInvalidSocket)) {
			return false;
		}
		std::lock_guard lock(mSendMutex);
		size_t sent = 0;
		while (sent < size) {
			const int chunk = ::send(static_cast<native_socket_t>(mSocket), reinterpret_cast<const char*>(data + sent), static_cast<int>(size - sent), 0);
			if (chunk <= 0) {
				mConnected = false;
				return false;
			}
			sent += static_cast<size_t>(chunk);
		}
		return true;
	}

	void TcpSocket::setReceiveBufferSize(int bytes)
	{
		if (mSocket == static_cast<uintptr_t>(kInvalidSocket)) {
			return;
		}
		setsockopt(static_cast<native_socket_t>(mSocket), SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bytes), sizeof(bytes));
	}

	bool TcpSocket::recvSome(std::vector<uint8_t>& buffer, size_t maxBytes, size_t& received)
	{
		received = 0;
		if (!mConnected || mSocket == static_cast<uintptr_t>(kInvalidSocket)) {
			return false;
		}
		buffer.resize(maxBytes);
		const int chunk = ::recv(static_cast<native_socket_t>(mSocket), reinterpret_cast<char*>(buffer.data()), static_cast<int>(maxBytes), 0);
		if (chunk == 0) {
			mConnected = false;
			return false;
		}
		if (chunk < 0) {
#ifdef _WIN32
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				buffer.clear();
				return true;
			}
#endif
			mConnected = false;
			return false;
		}
		received = static_cast<size_t>(chunk);
		buffer.resize(received);
		return true;
	}

} // namespace SFE::EditorProtocol
