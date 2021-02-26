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
#include "Components/AudioComponent.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreMediaReceiver.generated.h"

class UVideoCoreSoundWave;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCorMediaReceiverSoundSourceReady, USoundWave*, SoundWave);

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
	
public: // UE
	~UVideoCoreMediaReceiver();

	UFUNCTION(BlueprintCallable)
	void Init(UVideoCoreSignalingComponent* vcSiganlingComponent);

	UFUNCTION(BlueprintCallable)
	void Consume(FString producerId);

	UFUNCTION(BlueprintCallable)
	UTexture2D* getVideoTexture() const { return videoTexture_;  }

	UPROPERTY(BlueprintAssignable)
	FVideoCorMediaReceiverSoundSourceReady OnSoundSourceReady;

public: // native

	// TODO: refactor into VideoCore function library
	static std::string generateUUID();

	int32 GeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples);

protected:
	/**
	Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	to allow the object to begin an asynchronous cleanup process.
	*/
	void BeginDestroy() override;

private: // UE

	UPROPERTY()
	UVideoCoreSignalingComponent* vcComponent_;
	
	UPROPERTY()
	UVideoCoreSoundWave* soundWave_;

private: // native

	void subscribe();
	void consume(mediasoupclient::RecvTransport* t, bool consumeVideo = true);

	// mediasoupclient::Transport::Listener interface
	std::future<void> OnConnect(
		mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
		mediasoupclient::Transport* transport, const std::string& connectionState) override;

	// mediasoupclient::Consumer::Listener interface
	void OnTransportClose(mediasoupclient::Consumer* consumer) override;

	// rtc::VideoSinkInterface<webrtc::VideoFrame> interface
	void OnFrame(const webrtc::VideoFrame& vf);

	// webrtc::AudioTrackSinkInterface interface
	void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels,
		size_t number_of_frames);

	// rtc objects
	mediasoupclient::RecvTransport* recvTransport_;
	mediasoupclient::Consumer* videoConsumer_;
	mediasoupclient::Consumer* audioConsumer_;

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;

	// video rendering
	FCriticalSection renderSyncContext_;
	uint32_t frameWidth_, frameHeight_, bufferSize_;
	uint8_t* frameBgraBuffer_;
	bool needFrameBuffer_, hasNewFrame_;
	UTexture2D* videoTexture_;

	// audio rendering
	FCriticalSection audioSyncContext_;
	std::vector<uint8_t> audioBuffer_;
	uint32_t nSamples_, nChannels_, bps_, nFrames_, sampleRate_;

	// helper calls
	void initTexture(int w, int h);
	void initFrameBuffer(int w, int h);
	void captureVideoFrame();

	void shutdown();

	void setupConsumerTrack(mediasoupclient::Consumer* c);
	void setupSoundWave(int nChannels, int bps, int sampleRate);
};
