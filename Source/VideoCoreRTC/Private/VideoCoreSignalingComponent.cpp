//
// VideoCoreSignalingComponent.cpp
//
//  Generated on February 8 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreSignalingComponent.h"
#include "native/video-core-rtc.hpp"

using namespace std;
using namespace videocore;

// Sets default values for this component's properties
UVideoCoreSignalingComponent::UVideoCoreSignalingComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	bWantsInitializeComponent = true;
}

void UVideoCoreSignalingComponent::InitializeComponent()
{
	sIOClientComponent_ = Cast<USocketIOClientComponent>(this->GetOwner()->GetComponentByClass(USocketIOClientComponent::StaticClass()));
	if (!sIOClientComponent_)
	{
		UE_LOG(LogTemp, Warning, TEXT("No sister socket IO component found"));
		return;
	}
	else
	{
		videocore::initialize();
	}
}

// Called when the game starts
void UVideoCoreSignalingComponent::BeginPlay()
{
	Super::BeginPlay();
}

// TODO: add destroy call
//if (recvTransport_)
//{
//	recvTransport_->Close();
//	delete recvTransport_;
//}

void UVideoCoreSignalingComponent::connect(FString url, FString path)
{
	if (sIOClientComponent_)
	{
		UE_LOG(LogTemp, Log, TEXT("Connecting %s (%s)..."), *url, *path);

		sIOClientComponent_->Disconnect();
		sIOClientComponent_->SetupCallbacks();
		setupVideoCoreServerCallbacks();

		USIOJsonObject* query = USIOJsonObject::ConstructJsonObject(this);;
		if (!clientName.IsEmpty())
			query->SetStringField(TEXT("name"), clientName);
		if (!clientId.IsEmpty())
			query->SetStringField(TEXT("id"), clientId);

		sIOClientComponent_->Connect(url, path, query);
	}
}

// Called every frame
void UVideoCoreSignalingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UVideoCoreSignalingComponent::invokeWhenRecvTransportReady(function<void(const mediasoupclient::RecvTransport*)> cb)
{
	if (recvTransport_)
		cb(recvTransport_);
	else
		onTransportReady_.AddLambda([cb, this](FString tId) {
			cb(this->getRecvTransport());
		});
}

void UVideoCoreSignalingComponent::setupVideoCoreServerCallbacks()
{
	sIOClientComponent_->OnConnected.AddDynamic(this, &UVideoCoreSignalingComponent::onConnectedToServer);
	sIOClientComponent_->OnDisconnected.AddDynamic(this, &UVideoCoreSignalingComponent::onDisconnected);
	
	sIOClientComponent_->OnNativeEvent(TEXT("newClient"), [&](const FString&, const TSharedPtr<FJsonValue>& data) {
		auto m = data->AsObject();
		FString clientName = m->GetStringField(TEXT("name"));
		FString clientId = m->GetStringField(TEXT("id"));

		UE_LOG(LogTemp, Log, TEXT("new client %s %s"), *clientName, *clientId);

		OnNewClientConnected.Broadcast(clientName, clientId);
		onNewClient_ .Broadcast(clientName, clientId);
		// TODO: add to local client roster
	});

	sIOClientComponent_->OnNativeEvent(TEXT("clientDisconnected"), [&](const FString& msg, const TSharedPtr<FJsonValue>& data) {
		UE_LOG(LogTemp, Log, TEXT("client disconnected %s %s"), *msg, *USIOJConvert::ToJsonString(data));

		OnClientLeft.Broadcast(data->AsString());
		onClientLeft_.Broadcast(data->AsString());
	});

	sIOClientComponent_->OnNativeEvent(TEXT("newProducer"),
		[&](const FString& msg, const TSharedPtr<FJsonValue>& data)
	{
		auto m = data->AsObject();
		FString clientId = m->GetStringField(TEXT("clientId"));
		FString producerId = m->GetStringField(TEXT("producerId"));

		UE_LOG(LogTemp, Log, TEXT("client {%s} created new producer {%s}"), *clientId, *producerId);
		onNewProducer_.Broadcast(clientId, producerId);
	});

	sIOClientComponent_->OnNativeEvent(TEXT("admit"), [&](const FString&, const TSharedPtr<FJsonValue>& response) {
		auto m = response->AsObject();
		UE_LOG(LogTemp, Log, TEXT("Admitted as %s (id %s)"), *m->GetStringField(TEXT("name")), *m->GetStringField(TEXT("id")));

		this->clientName = m->GetStringField(TEXT("name"));
		this->clientId = m->GetStringField(TEXT("id"));
	});
}

void UVideoCoreSignalingComponent::setupConsumerTransport()
{
	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetBoolField(TEXT("forceTcp"), false);

	sIOClientComponent_->EmitNative(TEXT("createConsumerTransport"), obj, [this](auto response) {
		auto m = response[0]->AsObject();
		FString errorMsg;

		if (m->TryGetStringField(TEXT("error"), errorMsg))
		{
			UE_LOG(LogTemp, Warning, TEXT("Server failed to create consumer transport: %s"), *errorMsg);

		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Server created consumer transport. Reply %s"),
				*USIOJConvert::ToJsonString(m));

			nlohmann::json consumerData = fromFJsonObject(m);
			recvTransport_ = getDevice().CreateRecvTransport(this,
				consumerData["id"].get<std::string>(),
				consumerData["iceParameters"],
				consumerData["iceCandidates"],
				consumerData["dtlsParameters"]);

			onTransportReady_.Broadcast(FString(consumerData["id"].get<std::string>().c_str()));
		}
	});
}

void UVideoCoreSignalingComponent::setupProducerTransport()
{

}

std::future<void>
UVideoCoreSignalingComponent::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
	UE_LOG(LogTemp, Log, TEXT("Transport OnConnect"));

	std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();

	nlohmann::json m = {
		{"transportId", },
		{"dtlsParameters", dtlsParameters} };

	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetStringField(TEXT("transportId"), transport->GetId().c_str());
	obj->SetField(TEXT("dtlsParameters"), fromJsonObject(dtlsParameters));

	sIOClientComponent_->EmitNative(TEXT("connectConsumerTransport"), obj, [promise](auto response) {
		if (response.Num())
		{
			auto m = response[0]->AsObject();
			UE_LOG(LogTemp, Log, TEXT("connectConsumerTransport reply %s"), *USIOJConvert::ToJsonString(m));
		}
	});

	promise->set_value();
	return promise->get_future();
}

void
UVideoCoreSignalingComponent::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
	FString str(connectionState.c_str());
	UE_LOG(LogTemp, Log, TEXT("Transport connection state change %s"), *str);

	if (connectionState == "connected")
		// TODO: replace with mcast delegate
		sIOClientComponent_->EmitNative(TEXT("resume"), nullptr, [](auto response) {
	});
}

void UVideoCoreSignalingComponent::onConnectedToServer(FString SessionId, bool bIsReconnection)
{
	UE_LOG(LogTemp, Log, TEXT("onConnectedToServer called"));
	if (!bIsReconnection)
	{
		UE_LOG(LogTemp, Log, TEXT("VideoCore media server connected"));

		sIOClientComponent_->EmitNative(TEXT("getRouterRtpCapabilities"), nullptr, [this](auto response) {
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

			// TODO: add err callback if device can't be loaded
			videocore::ensureDeviceLoaded([this](mediasoupclient::Device&) {
				setupConsumerTransport();
				//setupProducerTransport();
			});
		});
	}
}

void UVideoCoreSignalingComponent::onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("VideoCore mediaserver disconnected. Reason: %s"),
		(Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));

	OnRtcSiganlingDisconnected.Broadcast((Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));
}