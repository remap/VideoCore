//
// VideoCoreRtcTypes.h
//
//  Generated on March 23 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include <CoreMinimal.h>
#include "VideoCoreRtcTypes.generated.h"

UENUM(BlueprintType)
enum class EClientState : uint8 {
	Offline UMETA(DisplayName = "Offline"),
	NotProducing UMETA(DisplayName = "Not Producing"),
	Producing UMETA(DisplayName = "Producing")
};

UENUM(BlueprintType)
enum class EMediaTrackKind : uint8 {
	Unknown UMETA(DisplayName = "Unknown"),
	Audio UMETA(DisplayName = "Audio"),
	Video UMETA(DisplayName = "Video"),
	Data UMETA(DisplayName = "Data")
};

UENUM(BlueprintType)
enum class EMediaTrackState : uint8 {
	Unknown UMETA(DisplayName = "Unknown"),
	Ended UMETA(DisplayName = "Ended"),
	Initializing UMETA(DisplayName = "Initializing"),
	Live UMETA(DisplayName = "Live")
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