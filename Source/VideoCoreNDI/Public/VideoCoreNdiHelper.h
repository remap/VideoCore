// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

#include <Components/AudioComponent.h>
#include <Objects/Media/NDIMediaSoundWave.h>
#include <Objects/Media/NDIMediaReceiver.h>

#include "VideoCoreNdiHelper.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNdiHelperOnSourceConnected, UNDIMediaReceiver*, Source);

/**
 * 
 */
UCLASS(ClassGroup = (VideoCore), Blueprintable, meta = (BlueprintSpawnableComponent))
class VIDEOCORENDI_API UVideoCoreNdiHelper : public UObject
{
	GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable)
    void SetupMediaSourceCallbacks(UNDIMediaReceiver* NDIMediaSource);

    UFUNCTION(BlueprintCallable)
    bool SetupAudioComponent(UNDIMediaReceiver* NDIMediaSource, UAudioComponent* AudioComponent);


    UPROPERTY(BlueprintAssignable)
    FNdiHelperOnSourceConnected OnSourceConnected;

private:
    UPROPERTY(transient)
    UNDIMediaSoundWave* AudioSoundWave = nullptr;
	
};
