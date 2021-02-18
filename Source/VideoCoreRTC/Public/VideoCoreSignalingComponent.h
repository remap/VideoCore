//
// VideoCoreSignalingComponent.h
//
//  Generated on February 8 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SocketIOClientComponent.h"
#include "SIOJsonValue.h"
#include "VideoCoreSignalingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignalingConnected, USIOJsonObject*, RouterCaps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignallingDisconnected, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSubsystemInitialized, USIOJsonObject*, MyCaps);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VIDEOCORERTC_API UVideoCoreSignalingComponent : public UActorComponent
{
	GENERATED_BODY()
private:
	UPROPERTY()
	USocketIOClientComponent* sIOClientComponent;

public:	
	// Sets default values for this component's properties
	UVideoCoreSignalingComponent();

	UFUNCTION()
	USocketIOClientComponent* getSocketIOClientComponent() const { return sIOClientComponent; }

public:
	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignalingConnected OnRtcSignalingConnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignallingDisconnected OnRtcSiganlingDisconnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSubsystemInitialized OnRtcSubsystemInitialized;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UFUNCTION()
	void onConnectedToServer(FString SessionId, bool bIsReconnection);
	UFUNCTION()
	void onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason);

	void setupVideoCoreServerCallbacks();
};
