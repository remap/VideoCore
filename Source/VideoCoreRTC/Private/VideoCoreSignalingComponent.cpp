// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCoreSignalingComponent.h"

// *****
namespace videocore {
	nlohmann::json fromFString(FString jsonString);
	nlohmann::json fromFJsonValue(const TSharedPtr<FJsonValue>& jsonValue);
	nlohmann::json fromFJsonObject(const TSharedPtr<FJsonObject>& jsonObject);

	TSharedPtr<FJsonValue> fromJsonObject(const nlohmann::json& jobj);
}

using namespace videocore;

nlohmann::json videocore::fromFString(FString jsonString)
{
	return nlohmann::json::parse(TCHAR_TO_ANSI(*jsonString));
}

nlohmann::json videocore::fromFJsonValue(const TSharedPtr<FJsonValue>& jsonValue)
{
	return fromFString(USIOJConvert::ToJsonString(jsonValue));
}

nlohmann::json videocore::fromFJsonObject(const TSharedPtr<FJsonObject>& jsonObject)
{
	return fromFString(USIOJConvert::ToJsonString(jsonObject));
}

TSharedPtr<FJsonValue> videocore::fromJsonObject(const nlohmann::json& jobj)
{
	return USIOJConvert::JsonStringToJsonValue(jobj.dump().c_str());
}
//*******
#include "modules/audio_device/include/audio_device.h"
#include "api/create_peerconnection_factory.h"
#include "system_wrappers/include/clock.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;

/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
static rtc::Thread* networkThread;
static rtc::Thread* signalingThread;
static rtc::Thread* workerThread;

static void createFactory()
{
	networkThread = rtc::Thread::Create().release();
	signalingThread = rtc::Thread::Create().release();
	workerThread = rtc::Thread::Create().release();

	networkThread->SetName("network_thread", nullptr);
	signalingThread->SetName("signaling_thread", nullptr);
	workerThread->SetName("worker_thread", nullptr);

	if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
	{
		UE_LOG(LogTemp, Error, TEXT("thread start errored"));
	}

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	//webrtc::AudioDeviceModule::AudioLayer audioLayer = webrtc::AudioDeviceModule::kPlatformDefaultAudio;
	//webrtc::Create
	//auto audioDeviceModule = webrtc::AudioDeviceModule::Create(audioLayer, );
	//if (!audioDeviceModule)
	//{
	//	UE_LOG(LogTemp, Error, TEXT("audio capture module creation errored"));
	//}

	factory = webrtc::CreatePeerConnectionFactory(
		networkThread,
		workerThread,
		signalingThread,
		nullptr, // default ADM
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(),
		nullptr /*audio_mixer*/,
		nullptr /*audio_processing*/);

	if (!factory)
	{
		UE_LOG(LogTemp, Error, TEXT("error ocurred creating peerconnection factory"));
	}
}

//******

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
		UE_LOG(LogTemp, Log, TEXT("Found SIOClientComponent: %s"), *sIOClientComponent->GetDesc());

		mediasoupclient::Initialize();
		UE_LOG(LogTemp, Log, TEXT("Initialized mediasoupclient module, version %s"),
				mediasoupclient::Version().c_str());
		
		createFactory();


		//sIOClientComponent->bShouldAutoConnect = false;
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

			UE_LOG(LogTemp, Log, TEXT("GOT ROUTER CAPABILITIES %s"), *USIOJConvert::ToJsonString(m));
			loadMediaSoupDevice(response[0]);
		});
	}
}

void UVideoCoreSignalingComponent::onDisconnected(TEnumAsByte<ESIOConnectionCloseReason> Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("VideoCore mediaserver disconnected. Reason: %s"),
		(Reason == CLOSE_REASON_NORMAL ? "NORMAL" : "DROP"));
}

void UVideoCoreSignalingComponent::loadMediaSoupDevice(TSharedPtr<FJsonValue> rtpCapabilities)
{
	try
	{
		nlohmann::json caps = fromFJsonValue(rtpCapabilities);
		mediasoupclient::PeerConnection::Options opt;
		opt.factory = factory;

		device_.Load(caps, &opt);

		subscribe();
	}
	catch (std::exception& e)
	{
		UE_LOG(LogTemp, Warning, TEXT("Caught exception trying to load mediasoup device: %s"),
			e.what());
	}
}

void UVideoCoreSignalingComponent::subscribe()
{
	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetBoolField(TEXT("forceTcp"), false);

	sIOClientComponent->EmitNative(TEXT("createConsumerTransport"), obj, [this](auto response) {
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
			recvTransport_ = device_.CreateRecvTransport(this,
				consumerData["id"].get<std::string>(),
				consumerData["iceParameters"],
				consumerData["iceCandidates"],
				consumerData["dtlsParameters"]);

			consume(recvTransport_);
		}
	});
}

void
UVideoCoreSignalingComponent::consume(mediasoupclient::RecvTransport* t)
{
	nlohmann::json caps = { {"rtpCapabilities", device_.GetRtpCapabilities()} };
	auto jj = fromJsonObject(caps);

	sIOClientComponent->EmitNative(TEXT("consume"), fromJsonObject(caps), [this](auto response) {
		auto m = response[0]->AsObject();

		UE_LOG(LogTemp, Log, TEXT("Server consume reply %s"), *USIOJConvert::ToJsonString(m));

		if (m->HasField(TEXT("id")))
		{
			nlohmann::json rtpParams = fromFJsonObject(m->GetObjectField(TEXT("rtpParameters")));

			consumer_ = recvTransport_->Consume(this,
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("id")))),
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("producerId")))),
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("kind")))),
				&rtpParams
			);

			stream_ = factory->CreateLocalMediaStream("recv_stream");

			if (stream_)
			{
				stream_->AddTrack((webrtc::VideoTrackInterface*)consumer_->GetTrack());

				rtc::VideoSinkWants sinkWants;
				((webrtc::VideoTrackInterface*)consumer_->GetTrack())->AddOrUpdateSink(this, sinkWants);

				webrtc::MediaStreamTrackInterface::TrackState s = consumer_->GetTrack()->state();
				
				if (s == webrtc::MediaStreamTrackInterface::kLive)
				{
					consumer_->GetTrack()->set_enabled(true);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Can not consume"));
		}
	}); 
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

	sIOClientComponent->EmitNative(TEXT("connectConsumerTransport"), obj, [promise](auto response) {
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
		sIOClientComponent->EmitNative(TEXT("resume"), nullptr, [](auto response) {
	});
}

void
UVideoCoreSignalingComponent::OnTransportClose(mediasoupclient::Consumer* consumer)
{
	UE_LOG(LogTemp, Log, TEXT("Consumer transport closed"));
}

void 
UVideoCoreSignalingComponent::OnFrame(const webrtc::VideoFrame& vf)
{
	UE_LOG(LogTemp, Log, TEXT("Incoming frame %dx%d %dms %d bytes"),
		vf.width(), vf.height(), vf.timestamp(), vf.size());
}