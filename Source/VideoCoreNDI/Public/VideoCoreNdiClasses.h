// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include <Structures/NDIConnectionInformation.h>
#include <Objects/Media/NDIMediaTexture2D.h>

#include "VideoCoreNdiClasses.generated.h"

UCLASS(ClassGroup = (VideoCore), Blueprintable, meta = (BlueprintSpawnableComponent))
class VIDEOCORENDI_API UNdiSourceListItem : public UObject {
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadWrite)
    FNDIConnectionInformation ndiSourceInfo_;
};

UCLASS(ClassGroup = (VideoCore), Blueprintable, HideDropdown, HideCategories = (ImportSettings, Compression, Texture, Adjustments, Compositing, LevelOfDetail, Object), meta = (BlueprintSpawnableComponent, DisplayName = "VideoCore NDI Media Texture 2D"))
class VIDEOCORENDI_API UVideoCoreNdiMediaTexture : public UNDIMediaTexture2D {
    GENERATED_BODY()
};
