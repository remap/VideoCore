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

USTRUCT(Blueprintable)
struct FPlaneRecord {

    GENERATED_BODY()

    UPROPERTY(BlueprintReadonly)
    FString planeName_;
    
    UPROPERTY()
    FString className_;

    UPROPERTY()
    TArray<uint8> planeData_;

    friend FArchive& operator<<(FArchive& ar, FPlaneRecord& rec)
    {
        ar << rec.className_;
        ar << rec.planeName_;
        ar << rec.planeData_;

        return ar;
    }
};

UENUM(BlueprintType)
enum class EVideoCorePlaneOrientation : uint8 {
    HorizontalDefault UMETA(DisplayName="Horizontal Default"),
    VerticalLeft UMETA(DisplayName="Vertical Left"),
    VerticalRight UMETA(DisplayName="Vertical Right"),
    UpsideDown UMETA(DisplayName="Upside Down")
};

class AVideoCorePlane;

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

    UFUNCTION(BlueprintCallable)
    static void ClipboardCopy(const FString& str);

    UFUNCTION(BlueprintCallable)
    static TArray<FPlaneRecord> SerializePlanes(const TArray<AVideoCorePlane*>& planes);

    UFUNCTION(BlueprintCallable)
    static void DeserializePlane(AVideoCorePlane* plane, const FPlaneRecord& data);

    UFUNCTION(BlueprintCallable)
    static TMap<FString, FString> GetCommandLineArgs();
	
};
