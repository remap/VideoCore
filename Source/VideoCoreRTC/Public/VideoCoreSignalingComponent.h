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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcNewClient, FString, ClientName, FString, ClientId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcClientLeft, FString, ClientId);

DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcInternalNewClient, FString, FString);
DECLARE_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcInternalClientLeft, FString);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcInternalNewProducer, FString, FString);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VIDEOCORERTC_API UVideoCoreSignalingComponent : public UActorComponent
{
	GENERATED_BODY()
private:
	UPROPERTY()
	USocketIOClientComponent* sIOClientComponent_;

public:	
	// Sets default values for this component's properties
	UVideoCoreSignalingComponent();

	UFUNCTION()
	USocketIOClientComponent* getSocketIOClientComponent() const { return sIOClientComponent_; }

	UFUNCTION(BlueprintCallable)
	void connect(FString url, FString path = TEXT("server"));

public:
	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignalingConnected OnRtcSignalingConnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignallingDisconnected OnRtcSiganlingDisconnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSubsystemInitialized OnRtcSubsystemInitialized;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcNewClient OnNewClientConnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcClientLeft OnClientLeft;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString clientId;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString clientName;

	FVideoCoreRtcInternalNewClient onNewClient_;
	FVideoCoreRtcInternalClientLeft onClientLeft_;
	FVideoCoreRtcInternalNewProducer onNewProducer_;

protected:
	// Called before BeginPlay
	virtual void InitializeComponent() override;
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
