//
// VideoCoreMediaReceiver.h
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"
#include "BaseMediaSource.h"
#include "VideoCoreSignalingComponent.h"
#include "Engine/Texture2DDynamic.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreMediaReceiver.generated.h"

/**
 * Media source used for rendering incoming WebRTC media track. Can render one video and one audio track only.
 */
UCLASS(BlueprintType, Blueprintable, HideCategories = ("Platforms"), Category = "VideoCore IO", HideCategories = ("Information"), META = (DisplayName = "VideoCoreRTC Media Receiver"))
class VIDEOCORERTC_API UVideoCoreMediaReceiver : public UBaseMediaSource
	, public mediasoupclient::Transport::Listener
	, public mediasoupclient::Consumer::Listener
	, public rtc::VideoSinkInterface<webrtc::VideoFrame>
	, public webrtc::AudioTrackSinkInterface
{
	GENERATED_UCLASS_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void Init(UVideoCoreSignalingComponent* vcSiganlingComponent);

	UFUNCTION(BlueprintCallable)
	void Consume(FString producerId);

	UFUNCTION(BlueprintCallable)
	UTexture2D* getVideoTexture() const { return videoTexture_;  }

protected:
	/**
	Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	to allow the object to begin an asynchronous cleanup process.
	*/
	void BeginDestroy() override;

private:

	UPROPERTY()
	UVideoCoreSignalingComponent* vcComponent_;

private:
	void subscribe();
	void consume(mediasoupclient::RecvTransport* t);

	// mediasoupclient::Transport::Listener interface
	std::future<void> OnConnect(
		mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
		mediasoupclient::Transport* transport, const std::string& connectionState) override;

	// mediasoupclient::Consumer::Listener interface
	void OnTransportClose(mediasoupclient::Consumer* consumer) override;

	// rtc::VideoSinkInterface<webrtc::VideoFrame> interface
	void OnFrame(const webrtc::VideoFrame& vf);

	mediasoupclient::RecvTransport* recvTransport_;
	mediasoupclient::Consumer* consumer_;

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;

	// render texture
	FCriticalSection renderSyncContext_;
	uint32_t bufferSize_;
	uint8_t* frameBgraBuffer_;
	bool hasNewFrame_;
	//UTexture2DDynamic* videoTexture_;
	UTexture2D* videoTexture_;

	void initTexture(int w, int h);
	void captureVideoFrame();

	void shutdown();
};
