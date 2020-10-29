// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerControllerHelper.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnNewPlaneSpawned, FString, planeName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewPlaneSpawned_Mcast, AActor*, plane);

UCLASS(Blueprintable, BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VIDEOCORE_API UPlayerControllerHelper : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UPlayerControllerHelper();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable)
    void SpawnPlaneActor(UClass *planeClass, FTransform transform,
                         FOnNewPlaneSpawned onNewPlaneSpawned);
    
    UFUNCTION(Server, reliable)
    void RPC_SpawnPlaneActor(UClass *planeClass, FTransform transform);
    
    UPROPERTY(BlueprintCallable, BlueprintAssignable)
    FOnNewPlaneSpawned_Mcast Server_OnNewPlaneSpawned;
    
    UFUNCTION(BlueprintNativeEvent)
    void OnServerSpawnedPlane(AActor *plane);
    
    UFUNCTION(BlueprintCallable)
    void CallUserOnNewPlaneSpawned(FString planeName);
    
private:
    FOnNewPlaneSpawned User_onNewPlaneSpawned_;
};
