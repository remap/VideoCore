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

// TODO: move enums into a separate file
UENUM(BlueprintType)
enum class EClientState : uint8 {
	Offline UMETA(DisplayName="Offline"),
	NotProducing UMETA(DisplayName="Not Producing"),
	Producing UMETA(DisplayName="Producing")
};

UENUM(BlueprintType)
enum class EMediaTrackKind : uint8 {
	Audio UMETA(DisplayName="Audio"),
	Video UMETA(DisplayName="Video")
};

UENUM(BlueprintType)
enum class EMediaTrackState : uint8 {
	Unknown UMETA(DisplayName = "Unknown"),
	Ended UMETA(DisplayName="Ended"),
	Live UMETA(DisplayName="Live")
};

USTRUCT(BlueprintType, Blueprintable, Category = "VideoCore RTC", META = (DisplayName = "VideoCore RTC Stats"))
struct VIDEOCORERTC_API FVideoCoreMediaStreamStatistics
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	int AudioLevel;

	UPROPERTY(BlueprintReadOnly)
	bool TypingNoiseDetected;

	UPROPERTY(BlueprintReadOnly)
	bool VoiceDetected;

	UPROPERTY(BlueprintReadOnly)
	int SamplesReceived;

	UPROPERTY(BlueprintReadOnly)
	FString VideoContentHint;

	UPROPERTY(BlueprintReadOnly)
	int FramesReceived;
};


class UVideoCoreSoundWave;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCorMediaReceiverSoundSourceReady, USoundWave*, SoundWave);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreMediaReceiverClientStateChanged, EClientState, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreMediaReceiverStreamingStarted, FString, ProducerId, FString, Kind);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreMediaReceiverStreamingStopped, FString, ProducerId, FString, Reason);

DECLARE_MULTICAST_DELEGATE(FVideoCoreMediaReceiverTransportReady);

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
	void Consume(FString clientId);

	UFUNCTION(BlueprintCallable)
	void Stop();

	UFUNCTION(BlueprintCallable)
	UTexture2D* getVideoTexture() const { return videoTexture_;  }

	UFUNCTION(BlueprintCallable)
	EClientState getClientState() const { return clientState_; }

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

	// TODO: make setter and re-start streaming if clientId changes
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString clientId;

	UPROPERTY(BlueprintReadOnly)
	FString videoProducerId;

	UPROPERTY(BlueprintReadOnly)
	FString audioProducerId;

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

	TArray<FDelegateHandle> callbackHandles_;
	FVideoCoreMediaReceiverTransportReady OnTransportReady;
	EClientState clientState_;

private: // native

	void setupSocketCallbacks();
	void setupConsumer();
	void consume(mediasoupclient::RecvTransport* t, const std::string& streamId);

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
	size_t framesReceived_; // TODO: multi-thread read access -- make atomic?

	// audio rendering
	FCriticalSection audioSyncContext_;
	std::vector<uint8_t> audioBuffer_;
	uint32_t nSamples_, nChannels_, bps_, nFrames_, sampleRate_;
	size_t samplesReceived_; // TODO: multi-thread read access -- make atomic?

	// helper calls
	void initTexture(int w, int h);
	void initFrameBuffer(int w, int h);
	void captureVideoFrame();

	void stopStreaming(const std::string& kind);
	void shutdown();

	void setupConsumerTrack(mediasoupclient::Consumer* c);
	void setupSoundWave(int nChannels, int bps, int sampleRate);

	void setState(EClientState state);
};
