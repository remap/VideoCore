// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCoreSoundWave.h"

void
UVideoCoreSoundWave::Init(UVideoCoreMediaReceiver* mediaReceiver, int32 nChannels, int32 bitsPerSample, int32 sampleRate)
{
	mediaReceiver_ = mediaReceiver;
	nChannels_ = nChannels;
	bps_ = bitsPerSample;
	sampleRate_ = sampleRate;

	NumChannels = nChannels;
	SampleRate = sampleRate;
	SampleByteSize = (int32)ceil((float)bps_ / 8.);
	bLooping = true;
	Duration = INDEFINITELY_LOOPING_DURATION;

	bProcedural = true;

	SoundGroup = SOUNDGROUP_Default;

	bOverrideConcurrency = true;
	ConcurrencyOverrides.bLimitToOwner = true;
	ConcurrencyOverrides.MaxCount = 2;

	// TODO: find more info on this. set empricially
	//NumSamplesToGeneratePerCallback = 2048 * 8;

	OnSoundWaveProceduralUnderflow.BindLambda([](auto w, int32 n) {
		UE_LOG(LogTemp, Log, TEXT("AUDIO BUFFER UNDERFLOW %d"), n);
	});
}

int32
UVideoCoreSoundWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	/*UE_LOG(LogTemp, Log, TEXT("OnGeneratePCMAudio called. NumSamples requested %d"), NumSamples);*/

	if (IsValid(mediaReceiver_))
	{
		return mediaReceiver_->GeneratePCMAudio(OutAudio, NumSamples);
	}
	return 0;
}

Audio::EAudioMixerStreamDataFormat::Type 
UVideoCoreSoundWave::GetGeneratedPCMDataFormat() const
{
	return Audio::EAudioMixerStreamDataFormat::Int16;
}