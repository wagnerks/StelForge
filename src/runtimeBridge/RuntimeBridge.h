#pragma once



#include "editorProtocol/EditorProtocol.h"

#include "editorProtocol/TcpSocket.h"



#include "containersModule/Singleton.h"



#include <atomic>

#include <chrono>

#include <condition_variable>

#include <cstdint>

#include <deque>

#include <mutex>

#include <string>

#include <thread>

#include <vector>



namespace SFE::Render {
	class RenderHardwareInterface;
}

namespace SFE::RuntimeBridge {



	struct IncomingMessage {

		EditorProtocol::MessageType type = EditorProtocol::MessageType::Error;

		std::string json;

		std::vector<uint8_t> binary;

	};



	class RuntimeBridge : public Singleton<RuntimeBridge> {

	public:

		void start(uint16_t port = EditorProtocol::kDefaultPort);

		void stop();

		void tick();



		bool isRunning() const { return mRunning.load(); }

		bool hasClient() const { return mHasClient.load(); }

		void streamPresentedFrame();

	private:

		void networkThreadMain();

		void sendThreadMain();

		void processMessage(const IncomingMessage& msg);

		bool sendPacket(EditorProtocol::MessageType type, const std::string& json, const void* binary = nullptr, uint32_t binarySize = 0);

		bool enqueuePacket(std::vector<uint8_t>&& packet, bool dropIfBusy);

		void queueFramePacket(std::vector<uint8_t>&& packet);

		void sendStatus();

		void sendWorldSnapshot(const std::string& requestJson = {});

		void processInput(const std::string& json);

		void syncDisplayMode();

		void applyEmbedHost();



		std::thread mNetworkThread;

		std::thread mSendThread;

		std::atomic<bool> mRunning{ false };

		std::atomic<bool> mSendRunning{ false };

		std::atomic<bool> mHasClient{ false };

		std::atomic<bool> mSubscribeFrames{ false };



		std::mutex mIncomingMutex;

		std::vector<IncomingMessage> mIncoming;



		std::mutex mOutgoingMutex;

		std::condition_variable mOutgoingCv;

		std::deque<std::vector<uint8_t>> mOutgoing;



		EditorProtocol::TcpSocket mClientSocket;

		EditorProtocol::TcpSocket mListenSocket;

		uint16_t mPort = EditorProtocol::kDefaultPort;



		std::vector<uint8_t> mRecvBuffer;

		std::vector<uint8_t> mFrameRgba;
		std::vector<uint8_t> mFrameReadback;
		bool captureStreamFrame(Render::RenderHardwareInterface& rhi, std::vector<uint8_t>& rgba, int& width, int& height);

		bool mAppliedRemoteDisplay = false;

		bool mEmbeddedHostMode = false;
		bool mEmbedConfigDirty = false;
		uint64_t mEmbedHostHwnd = 0;
		int mEmbedX = 0;
		int mEmbedY = 0;
		int mEmbedW = 0;
		int mEmbedH = 0;

		int mAppliedOverlayX = -1;
		int mAppliedOverlayY = -1;
		int mAppliedOverlayW = -1;
		int mAppliedOverlayH = -1;
		std::chrono::steady_clock::time_point mLastOverlayRefresh{};
		std::chrono::steady_clock::time_point mLastFrameCapture{};
		std::chrono::steady_clock::time_point mLastSnapshotRequest{};

	};

} // namespace SFE::RuntimeBridge
