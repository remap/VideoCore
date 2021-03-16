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

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreSignalingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignalingConnected, USIOJsonObject*, RouterCaps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignallingDisconnected, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSubsystemInitialized, USIOJsonObject*, MyCaps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcNewClient, FString, ClientName, FString, ClientId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcClientLeft, FString, ClientId);

DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcInternalNewClient, FString, FString);
DECLARE_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcInternalClientLeft, FString);
DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcInternalNewProducer, FString, FString);

DECLARE_MULTICAST_DELEGATE_OneParam(FVideoCoreMediaReceiverTransportReady, FString);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Category = "VideoCore IO", META = (DisplayName = "VideoCoreRTC Signaling Component"))
class VIDEOCORERTC_API UVideoCoreSignalingComponent : public UActorComponent
	, public mediasoupclient::Transport::Listener
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
	void BeginDestroy() override;

public:	// native
	mediasoupclient::RecvTransport* getRecvTransport() const { return recvTransport_; }
	void invokeWhenRecvTransportReady(std::function<void(const mediasoupclient::RecvTransport*)> cb);

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private: // UE
	UFUNCTION()
	void onConnectedToServer(FString SessionId, bool bIsReconnection);
	UFUNCTION()
	void onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason);

	FVideoCoreMediaReceiverTransportReady onTransportReady_;

private: // native
	mediasoupclient::RecvTransport* recvTransport_;

	// mediasoupclient::Transport::Listener interface
	std::future<void> OnConnect(
		mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
		mediasoupclient::Transport* transport, const std::string& connectionState) override;

	void setupVideoCoreServerCallbacks();
	void setupConsumerTransport();
	void setupProducerTransport();
};
