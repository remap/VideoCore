// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "VideoCoreMediaReceiver.h"
#include "VideoCoreSoundWave.generated.h"

/**
 * SoundWave used for rendering incoming RTC audio stream
 */
UCLASS(Category = "VideoCore RTC", META = (DisplayName = "VideoCore Sound Wave"))
class VIDEOCORERTC_API UVideoCoreSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

	// USoundWaveProcedural interface
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;

public:
	void Init(UVideoCoreMediaReceiver* mediaReceiver, int32 nChannels, int32 bitsPerSample, int32 sampleRate);

private: // UE
	UPROPERTY()
	UVideoCoreMediaReceiver* mediaReceiver_;

private: // native
	int32 nChannels_, bps_, sampleRate_;
};
