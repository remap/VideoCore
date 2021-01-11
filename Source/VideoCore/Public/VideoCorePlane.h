// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DefaultPawn.h"
#include "VideoCorePlane.generated.h"

UCLASS()
class VIDEOCORE_API AVideoCorePlane : public ADefaultPawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AVideoCorePlane();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	void SerializePlaneData(TArray<uint8>& data);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
	bool InitFromSerializedData(TArray<uint8>& data);
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnDeserializationCompleted();

	virtual void OnDeserializationCompleted_Implementation();
};
