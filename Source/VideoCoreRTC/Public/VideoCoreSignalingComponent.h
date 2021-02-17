// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SocketIOClientComponent.h"

#pragma warning(disable:4596)
#pragma warning(disable:4800)
#include "mediasoupclient.hpp"

#include "VideoCoreSignalingComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VIDEOCORERTC_API UVideoCoreSignalingComponent : public UActorComponent
	, public mediasoupclient::Transport::Listener
	, public mediasoupclient::Consumer::Listener
	, public rtc::VideoSinkInterface<webrtc::VideoFrame>
	//, public webrtc::ObserverInterface
	//, public webrtc::RtpReceiverObserverInterface
	
{
	GENERATED_BODY()
private:
	UPROPERTY()
	USocketIOClientComponent* sIOClientComponent;

public:	
	// Sets default values for this component's properties
	UVideoCoreSignalingComponent();

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

	// TODO: refactor this (into a separate actor/class?)
	void loadMediaSoupDevice(TSharedPtr<FJsonValue> rtpCapabilities);
	void subscribe();
	void consume(mediasoupclient::RecvTransport* t);

	// mediasoupclient::Transport::Listener interface
	std::future<void> OnConnect(
		mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters) override;
	void OnConnectionStateChange(
		mediasoupclient::Transport* transport, const std::string& connectionState) override;

	// mediasoupclient::Consumer::Listener interface
	void OnTransportClose(mediasoupclient::Consumer* consumer) override;

	// rtc::VideoSinkInterface<webrtc::VideoFrame> interface
	void OnFrame(const webrtc::VideoFrame& vf);

	mediasoupclient::Device device_;
	mediasoupclient::RecvTransport* recvTransport_;
	mediasoupclient::Consumer* consumer_;

	//webrtc::MediaStreamInterface* stream_;
	rtc::scoped_refptr<webrtc::MediaStreamInterface> stream_;
};
