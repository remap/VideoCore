//
// VideoCoreMediaSender.h
//
//  Generated on March 23 2020
//  Copyright 2021 Regents of the University of California
//


#pragma once

#include "CoreMinimal.h"
#include "BaseMediaSource.h"
#include "VideoCoreSignalingComponent.h"
#include "VideoCoreRtcTypes.h"
#include <Engine/TextureRenderTarget2D.h>

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreMediaSender.generated.h"

namespace videocore
{
	class RenderTargetVideoTrackSource;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCorMediaSenderStartProducing, FString, StreamId, EMediaTrackKind, Kind);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVideoCorMediaSenderProduceFailure, FString, StreamId, EMediaTrackKind, Kind, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVideoCorMediaSenderStoppedProducing, FString, StreamId, EMediaTrackKind, Kind, FString, Reason);

DECLARE_MULTICAST_DELEGATE(FVideoCoreRtcInternalStreamReady);

/**
 * Media source used for sending attached render target texture as WebRTC media track. Can send one video and/or one audio track only.
 */
UCLASS(BlueprintType, Blueprintable, HideCategories = ("Platforms"), Category = "VideoCore IO", HideCategories = ("Information"), META = (DisplayName = "VideoCoreRTC Media Sender"))
class VIDEOCORERTC_API UVideoCoreMediaSender : public UBaseMediaSource
	, public mediasoupclient::Producer::Listener
{
	GENERATED_UCLASS_BODY()

public: // UE
	~UVideoCoreMediaSender();

	UFUNCTION(BlueprintCallable)
	bool Init(UVideoCoreSignalingComponent* vcSiganlingComponent);

	UFUNCTION(BlueprintCallable)
	bool Produce(FString contentHint, EMediaTrackKind trackKind);

	UFUNCTION(BlueprintCallable)
	void Stop(EMediaTrackKind trackKind);

	UFUNCTION(BlueprintCallable)
	bool hasTrackOfType(EMediaTrackKind Kind) const;

	UFUNCTION(BlueprintCallable)
	EMediaTrackState getTrackState(EMediaTrackKind Kind) const;

	UPROPERTY(BlueprintAssignable)
	FVideoCorMediaSenderStartProducing OnStartProducing;

	UPROPERTY(BlueprintAssignable)
	FVideoCorMediaSenderProduceFailure OnProduceFailure;

	UPROPERTY(BlueprintAssignable)
	FVideoCorMediaSenderStoppedProducing OnStoppedProducing;

	// will automatically start producing streams when connected to media server
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool AutoProduce;

	// The texture to send over WebRTC 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Content", META = (DisplayName = "Render Target", AllowPrivateAccess = true))
	UTextureRenderTarget2D* RenderTarget = nullptr;

	// desired videostream frame rate 
	UPROPERTY(BlueprintReadwrite, EditDefaultsOnly, Category = "Broadcast Settings", META = (DisplayName = "Frame Rate", AllowPrivateAccess = true))
	FFrameRate FrameRate = FFrameRate(30, 1);

	// the output frame size
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Broadcast Settings", META = (DisplayName = "Frame Size", AllowPrivateAccess = true))
	FIntPoint FrameSize = FIntPoint(1920, 1080);

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Broadcast Settings", META = (DisplayName = "Stream Bitrates (bps)", AllowPrivateAccess = true))
	TArray<int> Bitrates;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString VideoStreamHint;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString AudioStreamHint;

	// ready to be sent out texture
	UPROPERTY(BlueprintReadOnly)
	UTexture2D* BackbufferTexture;

public: // native

protected: // UE
	/**
	Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	to allow the object to begin an asynchronous cleanup process.
	*/
	void BeginDestroy() override;



private: // UE
	
	UPROPERTY()
	UVideoCoreSignalingComponent* vcComponent_;

	TArray<FDelegateHandle> callbackHandles_;

	FVideoCoreRtcInternalStreamReady onMediaStreamReady_;

private: // native
	
	// mediasoupclient::Producer::Listener
	void OnTransportClose(mediasoupclient::Producer*) override;
	
	void setupRenderThreadCallback();
	bool startStream(std::string id, EMediaTrackKind trackKind, std::string hint = "");
	void stopStream(EMediaTrackKind trackKind, std::string reason);
	void shutdown();
	void checkAutoProduce();

	// TODO: check this is working properly in widgets
	void setupBackBufferTexture();
	void setupRenderTarget(FIntPoint InFrameSize, FFrameRate InFrameRate);
	void createStream();
	void createVideoTrack(std::string trackId);
	void createAudioTrack(std::string trackId);
	void createVideoSource();
	void createProducer(std::string hint = "");
	void setStreamHint(FString hint, EMediaTrackKind trackKind);

	// tries to copy render target into copiedTexture_ variable
	// called on render thread every frame 
	void tryCopyRenderTarget();

	// called on timer according to the FrameRate
	// tries to send video frame over RTC stream
	void trySendFrame();


	EMediaTrackState videoTrackState_;
	EMediaTrackState audioTrackState_;

	// rtc objects
	std::shared_ptr<videocore::RenderTargetVideoTrackSource> videoSource_;
	FTimerHandle captureHandle_;

	mediasoupclient::Producer* videoProducer_;
	mediasoupclient::Producer* audioProducer_;

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;
	rtc::scoped_refptr<webrtc::VideoTrackInterface> videoTrack_;

	FCriticalSection audioSync_;
	FCriticalSection renderSync_;

	FTexture2DRHIRef backBufferTexture_ = nullptr;
	FPooledRenderTargetDesc renderTargetDesc_;
};
