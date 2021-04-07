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
#include "VideoCoreRtcTypes.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreSignalingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVideoCoreRtcSignalingConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignalingFailure, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSignallingDisconnected, FString, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSubsystemInitialized, USIOJsonObject*, MyCaps);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcSubsystemFailedToInit, FString, Reason);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcNewClient, FString, ClientName, FString, ClientId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcClientLeft, FString, ClientId);

DECLARE_DYNAMIC_DELEGATE_OneParam(FVideoCoreMediaServerOnClientRoster, const TArray<FVideoCoreMediaServerClientInfo>&, roster);

DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreRtcInternalNewClient, FString, FString);
DECLARE_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcInternalClientLeft, FString);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FVideoCoreRtcInternalNewProducer, FString, FString, FString);

DECLARE_MULTICAST_DELEGATE_TwoParams(FVideoCoreMediaReceiverTransportReady, FString, FString);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Category = "VideoCore IO", META = (DisplayName = "VideoCoreRTC Signaling Component"))
class VIDEOCORERTC_API UVideoCoreSignalingComponent : public UActorComponent
	, public mediasoupclient::SendTransport::Listener
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

	UFUNCTION(BlueprintCallable)
	TArray<FVideoCoreMediaServerClientInfo> getClientRoster() const;

	UFUNCTION(BlueprintCallable)
	void fetchClientRoster(FVideoCoreMediaServerOnClientRoster onRoster);

public:
	// Callbacks
	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignalingConnected OnRtcSignalingConnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignalingFailure OnRtcSignalingFailure;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSignallingDisconnected OnRtcSiganlingDisconnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSubsystemInitialized OnRtcSubsystemInitialized;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcSubsystemFailedToInit OnRtcSubsystemFailed;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcNewClient OnNewClientConnected;

	UPROPERTY(BlueprintAssignable)
	FVideoCoreRtcClientLeft OnClientLeft;

	// Properties
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString clientId;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FString clientName;

	UPROPERTY(BlueprintReadOnly)
	ESignalingClientState state;

	UPROPERTY(BlueprintReadOnly)
	FString lastError;

	UPROPERTY(BlueprintReadOnly)
	FString url;

	UPROPERTY(BlueprintReadOnly)
	FString serverPath;

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

	mediasoupclient::SendTransport* getSendTransport() const { return sendTransport_;  }
	void invokeWhenSendTransportReady(std::function<void(const mediasoupclient::SendTransport*)> cb);

	typedef std::function<std::future<std::string>(mediasoupclient::SendTransport*, const std::string& kind,
		nlohmann::json rtpParameters, const nlohmann::json& appData)> OnTransportProduce;
	void invokeOnTransportProduce(std::string trackId, OnTransportProduce cb);

	void invokeOnRecvTransportConnect(std::function<void()> cb);

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private: // UE
	UFUNCTION()
	void onConnectedToServer(FString SessionId, bool bIsReconnection);
	UFUNCTION()
	void onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason);
	UFUNCTION()
	void onNamespaceDisconnected(FString nspc);
	UFUNCTION()
	FVideoCoreMediaServerClientInfo& getClientRecord(FString cId);

	UPROPERTY()
	TMap<FString, FVideoCoreMediaServerClientInfo> clientRoster;

	FVideoCoreMediaReceiverTransportReady onTransportReady_;

private: // native
	mediasoupclient::RecvTransport* recvTransport_;
	mediasoupclient::SendTransport* sendTransport_;
	std::map<std::string, OnTransportProduce> transportProduceCb_;
	std::vector<std::function<void()>> recvTransportConnectCb_;

	// mediasoupclient::Transport::Listener (RecvTransport::Listener) interface
	std::future<void> OnConnect(
		mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
		mediasoupclient::Transport* transport, const std::string& connectionState) override;

	// mediasoupclient::SendTransport::Listener interface
	std::future<std::string> OnProduce(mediasoupclient::SendTransport*, const std::string& kind,
		nlohmann::json rtpParameters, const nlohmann::json& appData) override;
	std::future<std::string> OnProduceData(mediasoupclient::SendTransport*,
		const nlohmann::json& sctpStreamParameters, const std::string& label, const std::string& protocol, 
		const nlohmann::json& appData) override;

	void setupVideoCoreServerCallbacks();
	void initRtcSubsystem();
	void setupConsumerTransport(mediasoupclient::Device&);
	void setupProducerTransport(mediasoupclient::Device&);
	template<typename T>
	void cleanupTransport(T*& t);
};
