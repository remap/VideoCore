//
// video-core-rtc.hpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"
#include "api/create_peerconnection_factory.h"

namespace videocore 
{
	extern int kDefaultTextureWidth;
	extern int kDefaultTextureHeight;

	void initialize();

	nlohmann::json fromFString(FString jsonString);
	nlohmann::json fromFJsonValue(const TSharedPtr<FJsonValue>& jsonValue);
	nlohmann::json fromFJsonObject(const TSharedPtr<FJsonObject>& jsonObject);

	TSharedPtr<FJsonValue> fromJsonObject(const nlohmann::json& jobj);
	
	bool getIsUsingDefaultAdm();
	mediasoupclient::Device& getDevice();
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> getWebRtcFactory();

	void loadMediaSoupDevice(TSharedPtr<FJsonValue> rtpCapabilities);

	// mediasoupclient::Device object is loaded by signalling component (VideoCoreSignalingComponent)
	// once successfully connected to media server and fetched RTP capabilities
	// since different media streamin actors can access device at will, this function
	// ensures that the callback is executed only when device has been successfully loaded
	void ensureDeviceLoaded(std::function<void(mediasoupclient::Device&)> cb,
		std::function<void(std::string reason)> errCb = {});

	std::string generateUUID();
	void dispatchOnUtilityThread(std::function<void()> task);

	class DataConsumerListener : public mediasoupclient::DataConsumer::Listener {
	public:
		typedef std::function<void(mediasoupclient::DataConsumer*)> OnConnectionEvent;
		typedef std::function<void(mediasoupclient::DataConsumer*, const webrtc::DataBuffer&)> OnDataEvent;

		OnConnectionEvent onConnecting_, onOpen_, onClosing_, onClose_, onTransportClose_;
		OnDataEvent onMessageReceived_;

	private:
		// mediasoupclient::DataConsumer::Listener interface
		void OnConnecting(mediasoupclient::DataConsumer* consumer) override
		{
			if (onConnecting_) onConnecting_(consumer);
		}

		void OnOpen(mediasoupclient::DataConsumer* consumer) override
		{
			if (onOpen_) onOpen_(consumer);
		}

		void OnClosing(mediasoupclient::DataConsumer* consumer) override
		{
			if (onClosing_) onClosing_(consumer);
		}

		void OnClose(mediasoupclient::DataConsumer* consumer) override
		{
			if (onClose_) onClose_(consumer);
		}

		void OnMessage(mediasoupclient::DataConsumer* consumer, const webrtc::DataBuffer& buffer) override
		{
			if (onMessageReceived_) onMessageReceived_(consumer, buffer);
		}

		void OnTransportClose(mediasoupclient::DataConsumer* consumer) override
		{
			if (onTransportClose_) onTransportClose_(consumer);
		}
	};
}