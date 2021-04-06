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
#include "VideoCoreRtcTypes.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreMediaReceiver.generated.h"

class UVideoCoreSoundWave;

namespace videocore {
	class AudioBuffer;
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCorMediaReceiverSoundSourceReady, USoundWave*, SoundWave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreMediaReceiverClientStateChanged, EClientState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreMediaReceiverStreamingStarted, FString, ProducerId, EMediaTrackKind, Kind);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FVideoCoreMediaReceiverStreamingStopped, FString, ProducerId, EMediaTrackKind, Kind, FString, Reason);

/**
 * Media source used for rendering incoming WebRTC media track. Can render one video and one audio track only.
 */
UCLASS(BlueprintType, Blueprintable, HideCategories = ("Platforms"), Category = "VideoCore IO", HideCategories = ("Information"), META = (DisplayName = "VideoCoreRTC Media Receiver"))
class VIDEOCORERTC_API UVideoCoreMediaReceiver : public UBaseMediaSource
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
	void Consume(FString clientId);

	UFUNCTION(BlueprintCallable)
	void Stop();

	UFUNCTION(BlueprintCallable)
	UTexture2D* getVideoTexture() const { return videoTexture_;  }

	UFUNCTION(BlueprintCallable)
	EClientState getRemoteClientState() const { return remoteClientState_; }

	UFUNCTION(BlueprintCallable)
	bool hasTrackOfType(EMediaTrackKind Kind) const;

	UFUNCTION(BlueprintCallable)
	bool getIsTrackEnabled(EMediaTrackKind Kind) const;

	UFUNCTION(BlueprintCallable)
	void setTrackEnabled(EMediaTrackKind Kind, bool enabled);

	UFUNCTION(BlueprintCallable)
	EMediaTrackState getTrackState(EMediaTrackKind Kind) const;

	UFUNCTION(BlueprintCallable)
	FVideoCoreMediaStreamStatistics getStats() const;

	UPROPERTY(BlueprintAssignable)
	FVideoCorMediaReceiverSoundSourceReady OnSoundSourceReady;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreMediaReceiverClientStateChanged OnClientStateChanged;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreMediaReceiverStreamingStarted OnStreamingStarted;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreMediaReceiverStreamingStopped OnStreamingStopped;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool AutoConsume;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString clientId;

	UFUNCTION(BlueprintCallable)
	void SetVideoStreamFilter(FString videoStreamId);

	UFUNCTION(BlueprintCallable)
	FString GetVideoProducerId() const;

	// Allows to specify exact stream id to consume in case client produces multiple streams
	UFUNCTION(BlueprintCallable)
	void SetAudioStreamFilter(FString audioStreamId);

	UFUNCTION(BlueprintCallable)
	FString GetAudioProducerId() const;

public: // native
	int32 GeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples);

protected:
	/**
	Called before destroying the object.  This is called immediately upon deciding to destroy the object,
	to allow the object to begin an asynchronous cleanup process.
	*/
	void BeginDestroy() override;

private: // UE

	UPROPERTY(BlueprintGetter=GetVideoProducerId, BlueprintSetter = SetVideoStreamFilter)
	FString videoProducerId;

	UPROPERTY(BlueprintGetter=GetAudioProducerId, BlueprintSetter = SetAudioStreamFilter)
	FString audioProducerId;

	UPROPERTY()
	UVideoCoreSignalingComponent* vcComponent_;
	
	UPROPERTY()
	UVideoCoreSoundWave* soundWave_;

	TArray<FDelegateHandle> callbackHandles_;
	EClientState remoteClientState_;

	UPROPERTY()
	FString videoStreamIdFilter_;

	UPROPERTY()
	FString audioStreamIdFilter_;

private: // native

	void setupRenderThreadCallback();
	void setupSocketCallbacks();
	void createStream();
	void consume(mediasoupclient::RecvTransport* t, const std::string& streamId);

	// mediasoupclient::Consumer::Listener interface
	void OnTransportClose(mediasoupclient::Consumer* consumer) override;

	// rtc::VideoSinkInterface<webrtc::VideoFrame> interface
	void OnFrame(const webrtc::VideoFrame& vf);

	// webrtc::AudioTrackSinkInterface interface
	void OnData(const void* audio_data, int bits_per_sample, int sample_rate, size_t number_of_channels,
		size_t number_of_frames);

	// rtc objects
	mediasoupclient::Consumer* videoConsumer_;
	mediasoupclient::Consumer* audioConsumer_;

	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;

	// video rendering
	FCriticalSection frameBufferSync_;
	FCriticalSection videoTextureSync_;
	uint32_t frameWidth_, frameHeight_, bufferSize_;
	uint8_t* frameBgraBuffer_;
	std::atomic_bool hasNewFrame_;
	UTexture2D* videoTexture_;
	size_t framesReceived_; // TODO: multi-thread read access -- make atomic?

	// audio rendering
	std::shared_ptr<videocore::AudioBuffer> audioBuffer_;
	FCriticalSection audioSyncContext_;
	typedef struct _AudioDataDescription {
		uint32_t nChannels_, bps_, sampleRate_;

		bool operator ==(const struct _AudioDataDescription& other) const
		{
			return nChannels_ == other.nChannels_ &&
				bps_ == other.bps_ &&
				sampleRate_ == other.sampleRate_;
		}
		bool operator !=(const struct _AudioDataDescription& other) const
		{
			return !(*this == other);
		}
	} AudioDataDescription;
	bool isCreatingSounWave_;

	AudioDataDescription adataDesc_;
	size_t samplesReceived_; // TODO: multi-thread read access -- make atomic?

	// helper calls
	void initTexture(int w, int h);
	void initFrameBuffer(int w, int h);
	void captureVideoFrame();

	void stopStreaming(const std::string& kind, const std::string& reason = "client initiated");
	void shutdown();

	void setupConsumerTrack(mediasoupclient::Consumer* c);
	void resumeTrack(mediasoupclient::Consumer* c);
	void setupSoundWave(int nChannels, int bps, int sampleRate);

	void setState(EClientState state);
	// returns true if this client and producer IDs correspond to the clientId_ and either
	// of producer ID filters (videoStreamIdFilter_ or audioStramIdFilter_)
	bool isTargetProducer(FString clientId, FString producerId, FString kind);
	bool canConsume(FString kind);
};
