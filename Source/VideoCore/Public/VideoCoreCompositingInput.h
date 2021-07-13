// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CompositingElements/CompositingElementInputs.h"
#include "VideoCoreCompositingInput.generated.h"

/**
 * 
 */
UCLASS()
class VIDEOCORE_API UVideoCoreCompositingInput : public UMediaTextureCompositingInput
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetMediaTextureSource(UTexture* texture);
	
};
