// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VideoCoreFunctionLibrary.generated.h"

USTRUCT(Blueprintable)
struct FPlaneData {
    GENERATED_BODY()

        UPROPERTY(BlueprintReadWrite)
        FTransform transform_;

    UPROPERTY(BlueprintReadWrite)
        FVector defaultScale_;

    UPROPERTY(BlueprintReadWrite)
        FRotator autorotateLocalRotation_;

    UPROPERTY(BlueprintReadWrite)
        bool isAutoRotate_;

    UPROPERTY(BlueprintReadWrite)
        FTwoVectors textureCrop_;
};

/**
 * 
 */
UCLASS()
class VIDEOCORE_API UVideoCoreFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static TArray<uint8> StringToByteArray(const FString& str);

	UFUNCTION(BlueprintCallable)
	static FString ByteArrayToString(const TArray<uint8>& barr);

    UFUNCTION(BlueprintCallable)
    static TArray<uint8> ToBytesArray(const FPlaneData& d);

    UFUNCTION(BlueprintCallable)
    static FPlaneData FromBytesArray(const TArray<uint8>& d, int& offset);
	
};
