// Fill out your copyright notice in the Description page of Project Settings.

#include "VideoCoreNdiHelper.h"
#include "VideoCoreNDI.h"

void UVideoCoreNdiHelper::SetupMediaSourceCallbacks(UNDIMediaReceiver* NDIMediaSource)
{
	NDIMediaSource->OnNDIReceiverConnectedEvent.AddWeakLambda(this, [&](UNDIMediaReceiver* s) {
		OnSourceConnected.Broadcast(s);
	});
}


bool UVideoCoreNdiHelper::SetupAudioComponent(UNDIMediaReceiver* NDIMediaSource, UAudioComponent* AudioComponent)
{
	// Define the basic parameters for constructing temporary audio wave object
	FString AudioSource = FString::Printf(TEXT("AudioSource_%s"), *GetFName().ToString().Right(1));
	FName AudioWaveName = FName(*AudioSource);
	EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transient | RF_MarkAsNative;

	// Construct a temporary audio sound wave to be played by this component
	this->AudioSoundWave = NewObject<UNDIMediaSoundWave>(
		GetTransientPackage(),
		UNDIMediaSoundWave::StaticClass(),
		AudioWaveName,
		Flags
		);

	// Ensure the validity of the temporary sound wave object
	if (IsValid(this->AudioSoundWave))
	{
		// Set the sound of the Audio Component and Ensure playback
		AudioComponent->SetSound(this->AudioSoundWave);

		// Ensure we register the audio wave object with the media.
		if (IsValid(NDIMediaSource))
		{
			NDIMediaSource->RegisterAudioWave(AudioSoundWave);
			return true;
		}
			
	}

	return false;
}