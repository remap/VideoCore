// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SaveDataManager.generated.h"

UCLASS()
class VIDEOCORE_API ASaveDataManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASaveDataManager();

protected:
// Called when the game starts or when spawned
    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere,BlueprintReadWrite)
    TArray<FTransform> recordedTransforms;
    
    UPROPERTY(EditAnywhere,BlueprintReadWrite)
    TArray<float> recordedTimes;
    

    UFUNCTION(BlueprintCallable, Category = SaveLoad)
    bool SaveData();
    

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
