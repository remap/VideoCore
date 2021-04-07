//
// VideoCoreSignalingComponent.cpp
//
//  Generated on February 8 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreSignalingComponent.h"
#include "CULambdaRunnable.h"
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
	state = ESignalingClientState::NotConnected;
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


void UVideoCoreSignalingComponent::BeginDestroy()
{
	if (recvTransport_)
	{
		recvTransport_->Close();
		delete recvTransport_;
	}

	// Call the base implementation of 'BeginDestroy'
	Super::BeginDestroy();
}

TArray<FVideoCoreMediaServerClientInfo> UVideoCoreSignalingComponent::getClientRoster() const
{
	TArray<FVideoCoreMediaServerClientInfo> roster;

	for (auto t : clientRoster)
		roster.Add(t.Value);

	return roster;
}

void UVideoCoreSignalingComponent::fetchClientRoster(FVideoCoreMediaServerOnClientRoster onRoster)
{
	sIOClientComponent_->EmitNative(TEXT("getClientList"), nullptr,
		[this, onRoster](auto response) {

		for (auto v : response[0]->AsArray())
		{
			auto m = v->AsObject();
			int nProducers = (int)m->GetNumberField("nProducers");
			int nConsumers = (int)m->GetNumberField("nConsumers");
			FVideoCoreMediaServerClientInfo cInfo(m->GetStringField("id"), 
				m->GetStringField("name"),
				(nProducers ? EClientState::Producing : EClientState::NotProducing),
				nProducers, nConsumers);

			this->clientRoster.Add(cInfo.clientId, cInfo);
		}

		onRoster.ExecuteIfBound(this->getClientRoster());
	});
}

void UVideoCoreSignalingComponent::connect(FString url, FString path)
{
	if (sIOClientComponent_)
	{
		UE_LOG(LogTemp, Log, TEXT("Connecting %s (%s)..."), *url, *path);

		clientRoster.Empty();
		sIOClientComponent_->Disconnect();
		sIOClientComponent_->SetupCallbacks();

		static once_flag f;
		call_once(f, [this] {
			setupVideoCoreServerCallbacks();
		});

		USIOJsonObject* query = USIOJsonObject::ConstructJsonObject(this);;
		if (!clientName.IsEmpty())
			query->SetStringField(TEXT("name"), clientName);
		if (!clientId.IsEmpty())
			query->SetStringField(TEXT("id"), clientId);

		sIOClientComponent_->Connect(url, path, query);
		state = ESignalingClientState::Connecting;
		
		this->url = url;
		this->serverPath = path;
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
		onTransportReady_.AddLambda([cb, this](FString tId, FString type) {
			if (type.Equals("recv"))
			{
				cb(this->getRecvTransport());
			}
		});
}

void UVideoCoreSignalingComponent::invokeWhenSendTransportReady(function<void(const mediasoupclient::SendTransport*)> cb)
{
	if (sendTransport_)
		cb(sendTransport_);
	else
		onTransportReady_.AddLambda([cb, this](FString tId, FString type) {
			if (type.Equals("send"))
			{
				cb(this->getSendTransport());
			}
		});
}

void UVideoCoreSignalingComponent::invokeOnTransportProduce(std::string trackId, OnTransportProduce cb)
{
	if (transportProduceCb_.find(trackId) != transportProduceCb_.end())
		throw runtime_error("Callback for track already exists: " + trackId);

	transportProduceCb_[trackId] = cb;
}

void UVideoCoreSignalingComponent::invokeOnRecvTransportConnect(function<void()> cb)
{
	recvTransportConnectCb_.push_back(cb);
}

void UVideoCoreSignalingComponent::setupVideoCoreServerCallbacks()
{
	sIOClientComponent_->OnConnected.AddDynamic(this, &UVideoCoreSignalingComponent::onConnectedToServer);
	sIOClientComponent_->OnDisconnected.AddDynamic(this, &UVideoCoreSignalingComponent::onDisconnected);
	sIOClientComponent_->OnSocketNamespaceDisconnected.AddDynamic(this, &UVideoCoreSignalingComponent::onNamespaceDisconnected);
	
	sIOClientComponent_->OnNativeEvent(TEXT("newClient"), [&](const FString&, const TSharedPtr<FJsonValue>& data) {
		auto m = data->AsObject();
		FString clientName = m->GetStringField(TEXT("name"));
		FString clientId = m->GetStringField(TEXT("id"));

		getClientRecord(clientId) = FVideoCoreMediaServerClientInfo(clientId, clientName, EClientState::NotProducing, 0, 0);

		UE_LOG(LogTemp, Log, TEXT("new client %s %s"), *clientName, *clientId);
		
		OnNewClientConnected.Broadcast(clientName, clientId);
		onNewClient_.Broadcast(clientName, clientId);
	});

	sIOClientComponent_->OnNativeEvent(TEXT("clientDisconnected"), [&](const FString& msg, const TSharedPtr<FJsonValue>& data) {
		UE_LOG(LogTemp, Log, TEXT("client disconnected %s %s"), *msg, *USIOJConvert::ToJsonString(data));

		clientRoster.Remove(*USIOJConvert::ToJsonString(data));

		OnClientLeft.Broadcast(data->AsString());
		onClientLeft_.Broadcast(data->AsString());
	});

	sIOClientComponent_->OnNativeEvent(TEXT("newProducer"),
		[&](const FString& msg, const TSharedPtr<FJsonValue>& data)
	{
		auto m = data->AsObject();
		FString clientId = m->GetStringField(TEXT("clientId"));
		FString producerId = m->GetStringField(TEXT("producerId"));
		FString kind = m->GetStringField(TEXT("kind"));

		getClientRecord(clientId).state = EClientState::Producing;
		getClientRecord(clientId).nProducers += 1;

		UE_LOG(LogTemp, Log, TEXT("client %s created new producer %s (%s)"), *clientId, *producerId, *kind);
		onNewProducer_.Broadcast(clientId, producerId, kind);
	});

	sIOClientComponent_->OnNativeEvent(TEXT("admit"), [&](const FString&, const TSharedPtr<FJsonValue>& response) {
		auto m = response->AsObject();
		UE_LOG(LogTemp, Log, TEXT("Admitted as %s (id %s)"), *m->GetStringField(TEXT("name")), *m->GetStringField(TEXT("id")));

		this->clientName = m->GetStringField(TEXT("name"));
		this->clientId = m->GetStringField(TEXT("id"));

		this->initRtcSubsystem();
	});

	sIOClientComponent_->OnNativeEvent(TEXT("forbidden"), [&](const FString&, const TSharedPtr<FJsonValue>& response) {
		auto m = response->AsObject();
		this->lastError = FString("forbidden");

		if (m->HasField("error"))
		{
			this->lastError += ": " + m->GetStringField("error");
			OnRtcSignalingFailure.Broadcast(m->GetStringField("error"));
		}
		else
			OnRtcSignalingFailure.Broadcast("Unknown");

	});
}

void UVideoCoreSignalingComponent::initRtcSubsystem()
{
	// check for old transport
	if (recvTransport_)
		cleanupTransport<mediasoupclient::RecvTransport>(recvTransport_);
	if (sendTransport_)
		cleanupTransport<mediasoupclient::SendTransport>(sendTransport_);

	sIOClientComponent_->EmitNative(TEXT("getRouterRtpCapabilities"), nullptr,
		[this](auto response) {
		auto m = response[0]->AsObject();

		UE_LOG(LogTemp, Log, TEXT("GOT ROUTER CAPABILITIES %s"), *USIOJConvert::ToJsonString(m));

		if (!videocore::getDevice().IsLoaded())
		{
			videocore::loadMediaSoupDevice(response[0]);

			if (videocore::getDevice().IsLoaded())
			{
				USIOJsonObject* uobj = USIOJsonObject::ConstructJsonObject(this);
				USIOJsonValue* caps = USIOJsonValue::ConstructJsonValue(this,
					videocore::fromJsonObject(videocore::getDevice().GetRtpCapabilities()));
				uobj->SetField(TEXT("rtpCapabilities"), caps);

				OnRtcSubsystemInitialized.Broadcast(uobj);
			}
			else
			{
				this->lastError = FString("Failed to load mediasoupclient Device");
				OnRtcSubsystemFailed.Broadcast("Failed to load mediasoupclient Device");
			}
		}

		videocore::ensureDeviceLoaded(
			[this](mediasoupclient::Device& d)
		{
			setupConsumerTransport(d);
			setupProducerTransport(d);
		},
			[this](string reason)
		{
			this->lastError = FString(reason.c_str());
			OnRtcSubsystemFailed.Broadcast(reason.c_str());
		});
	});
}

void UVideoCoreSignalingComponent::setupConsumerTransport(mediasoupclient::Device& d)
{
	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetBoolField(TEXT("forceTcp"), false);

	sIOClientComponent_->EmitNative(TEXT("createConsumerTransport"), obj, [this](auto response) {
		auto m = response[0]->AsObject();
		FString errorMsg;

		if (m->TryGetStringField(TEXT("error"), errorMsg))
		{
			UE_LOG(LogTemp, Warning, TEXT("Server failed to create consumer transport: %s"), *errorMsg);
			
			OnRtcSubsystemFailed.Broadcast(FString::Printf(TEXT("consumer transport setup failed: %s"), *errorMsg));
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

			if (recvTransport_)
				onTransportReady_.Broadcast(FString(recvTransport_->GetId().c_str()), TEXT("recv"));
		}
	});
}

void UVideoCoreSignalingComponent::setupProducerTransport(mediasoupclient::Device& d)
{
	nlohmann::json body =
	{
		{ "forceTcp", false },
		{ "rtpCapabilities", getDevice().GetRtpCapabilities() }
	};

	sIOClientComponent_->EmitNative(TEXT("createProducerTransport"), fromJsonObject(body), [this](auto response) {
		auto m = response[0]->AsObject();
		FString errorMsg;

		if (m->TryGetStringField(TEXT("error"), errorMsg))
		{
			UE_LOG(LogTemp, Warning, TEXT("Server failed to create producer transport: %s"), *errorMsg);

			OnRtcSubsystemFailed.Broadcast(FString::Printf(TEXT("producer transport setup failed: %s"), *errorMsg));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Server created producer transport. Reply %s"),
				*USIOJConvert::ToJsonString(m));

			nlohmann::json producerData = fromFJsonObject(m);
			sendTransport_ = getDevice().CreateSendTransport(this,
				producerData["id"].get<string>(),
				producerData["iceParameters"],
				producerData["iceCandidates"],
				producerData["dtlsParameters"]);

			if (sendTransport_)
				onTransportReady_.Broadcast(FString(sendTransport_->GetId().c_str()), TEXT("send"));
		}
	});
}

template<typename T>
void UVideoCoreSignalingComponent::cleanupTransport(T*& t)
{
	if (t)
	{
		// schedule cleanup on game thread
		FCULambdaRunnable::RunShortLambdaOnGameThread([this, t]() {
			if (!t->IsClosed())
			{
				UE_LOG(LogTemp, Log, TEXT("Closing transport %s"), ANSI_TO_TCHAR(t->GetId().c_str()));
				t->Close(); // this should trigger OnTransportClose for all producers/consumers
			}

			delete t;

			// sorry about that
			if (is_same<T, mediasoupclient::RecvTransport>::value)
				recvTransportConnectCb_.clear();
			// TODO: fix crash below
			if (is_same<T, mediasoupclient::SendTransport>::value)
				transportProduceCb_.clear();
		});

		// nullify pointer now
		t = nullptr;
	}
}

std::future<void>
UVideoCoreSignalingComponent::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
	UE_LOG(LogTemp, Log, TEXT("Transport OnConnect"));

	std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();

	if (!(getRecvTransport() || getSendTransport()))
	{
		promise->set_exception(make_exception_ptr("Unknown transport trying to connect"));
	}
	else
	{
		auto obj = USIOJConvert::MakeJsonObject();
		obj->SetStringField(TEXT("transportId"), transport->GetId().c_str());
		obj->SetField(TEXT("dtlsParameters"), fromJsonObject(dtlsParameters));
		FString emit = (getRecvTransport() && getRecvTransport()->GetId() == transport->GetId() ? 
			TEXT("connectConsumerTransport") : TEXT("connectProducerTransport"));

		sIOClientComponent_->EmitNative(emit, obj, [promise](auto response) {});
	}
	promise->set_value();
	return promise->get_future();
}

void
UVideoCoreSignalingComponent::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
	// possible states:
	//	closed
	//	failed
	//	disonnected
	//	new
	//	connecting
	//	connected
	//	checking
	//	completed

	FString str(connectionState.c_str());
	UE_LOG(LogTemp, Log, TEXT("Transport (%s) connection state change %s"), ANSI_TO_TCHAR(transport->GetId().c_str()), *str);

	if (connectionState == "failed")// ||
		//connectionState == "disconnected")
	{
		if (transport == recvTransport_)
			cleanupTransport<mediasoupclient::RecvTransport>(recvTransport_);
		if (transport == sendTransport_)
			cleanupTransport<mediasoupclient::SendTransport>(sendTransport_);
	}

	if (connectionState == "connected" && transport == recvTransport_)
		FCULambdaRunnable::RunShortLambdaOnGameThread([this]() {
			for (auto cb : recvTransportConnectCb_) cb();
		});
		//sIOClientComponent_->EmitNative(TEXT("resume"), nullptr, [](auto response) {});
}

std::future<std::string> 
UVideoCoreSignalingComponent::OnProduce(mediasoupclient::SendTransport* t, const std::string& kind,
	nlohmann::json rtpParameters, const nlohmann::json& appData)
{
	if (appData.contains("trackId"))
	{
		map<string, OnTransportProduce>::iterator it = transportProduceCb_.find(appData["trackId"]);
		if (it != transportProduceCb_.end())
		{
			return it->second(t, kind, rtpParameters, appData);
		}
	}

	std::shared_ptr<std::promise<string>> promise = std::make_shared<std::promise<string>>();

	promise->set_exception(make_exception_ptr("produce unknown track"));

	return promise->get_future();
}

std::future<std::string> 
UVideoCoreSignalingComponent::OnProduceData(mediasoupclient::SendTransport*,
	const nlohmann::json& sctpStreamParameters, const std::string& label, const std::string& protocol,
	const nlohmann::json& appData)
{
	std::shared_ptr<std::promise<string>> promise = std::make_shared<std::promise<string>>();

	promise->set_exception(make_exception_ptr("not implemented"));

	return promise->get_future();
}

void UVideoCoreSignalingComponent::onConnectedToServer(FString SessionId, bool bIsReconnection)
{
	UE_LOG(LogTemp, Log, TEXT("VideoCore media server connected. Is Reconnect: %d"), bIsReconnection);
	state = ESignalingClientState::Connected;
	OnRtcSignalingConnected.Broadcast();
}

void UVideoCoreSignalingComponent::onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("VideoCore mediaserver disconnected. Reason: %s"),
		(Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));

	cleanupTransport<mediasoupclient::RecvTransport>(recvTransport_);
	cleanupTransport<mediasoupclient::SendTransport>(sendTransport_);

	state = ESignalingClientState::NotConnected;
	OnRtcSiganlingDisconnected.Broadcast((Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));
}

void UVideoCoreSignalingComponent::onNamespaceDisconnected(FString nmspc)
{
	UE_LOG(LogTemp, Log, TEXT("Namespace disconnected: %s"), *nmspc);

	cleanupTransport<mediasoupclient::RecvTransport>(recvTransport_);
	cleanupTransport<mediasoupclient::SendTransport>(sendTransport_);

	OnRtcSiganlingDisconnected.Broadcast("Server disonnect");
}

FVideoCoreMediaServerClientInfo& UVideoCoreSignalingComponent::getClientRecord(FString clientId)
{
	if (!clientRoster.Contains(clientId))
		clientRoster.Add(clientId);

	return clientRoster[clientId];
}