//
// VideoCoreSignalingComponent.cpp
//
//  Generated on February 8 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreSignalingComponent.h"
#include "native/video-core-rtc.hpp"

// Sets default values for this component's properties
UVideoCoreSignalingComponent::UVideoCoreSignalingComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void UVideoCoreSignalingComponent::BeginPlay()
{
	Super::BeginPlay();

	sIOClientComponent = Cast<USocketIOClientComponent>(this->GetOwner()->GetComponentByClass(USocketIOClientComponent::StaticClass()));
	if (!sIOClientComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("No sister socket IO component found"));
		return;
	}
	else
	{
		videocore::initialize();

		sIOClientComponent->Disconnect();
		sIOClientComponent->InitializeComponent();
		setupVideoCoreServerCallbacks();
		sIOClientComponent->Connect(TEXT("https://192.168.0.9:3000"), TEXT("server"));
	}
}


// Called every frame
void UVideoCoreSignalingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UVideoCoreSignalingComponent::setupVideoCoreServerCallbacks()
{
	sIOClientComponent->OnConnected.AddDynamic(this, &UVideoCoreSignalingComponent::onConnectedToServer);
	sIOClientComponent->OnDisconnected.AddDynamic(this, &UVideoCoreSignalingComponent::onDisconnected);
}

void UVideoCoreSignalingComponent::onConnectedToServer(FString SessionId, bool bIsReconnection)
{
	UE_LOG(LogTemp, Log, TEXT("onConnectedToServer called"));
	if (!bIsReconnection)
	{
		UE_LOG(LogTemp, Log, TEXT("VideoCore media server connected"));

		sIOClientComponent->EmitNative(TEXT("getRouterRtpCapabilities"), nullptr, [this](auto response) {
			auto m = response[0]->AsObject();

			{
				UE_LOG(LogTemp, Log, TEXT("GOT ROUTER CAPABILITIES %s"), *USIOJConvert::ToJsonString(m));
				USIOJsonObject* uobj = USIOJsonObject::ConstructJsonObject(this);
				uobj->SetRootObject(m);
				OnRtcSignalingConnected.Broadcast(uobj);
			}

			videocore::loadMediaSoupDevice(response[0]);

			{
				USIOJsonObject* uobj = USIOJsonObject::ConstructJsonObject(this);
				USIOJsonValue* caps = USIOJsonValue::ConstructJsonValue(this,
					videocore::fromJsonObject(videocore::getDevice().GetRtpCapabilities()));
				uobj->SetField(TEXT("rtpCapabilities"), caps);
				OnRtcSubsystemInitialized.Broadcast(uobj);
			}
		});
	}
}

void UVideoCoreSignalingComponent::onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("VideoCore mediaserver disconnected. Reason: %s"),
		(Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));

	OnRtcSiganlingDisconnected.Broadcast((Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));
}