//
// video-core-rtc.cpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#include "video-core-rtc.hpp"
#include "modules/audio_device/include/audio_device.h"
#include "api/create_peerconnection_factory.h"
#include "system_wrappers/include/clock.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "SIOJConvert.h"

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;
/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
static rtc::Thread* networkThread;
static rtc::Thread* signalingThread;
static rtc::Thread* workerThread;

DECLARE_MULTICAST_DELEGATE(FVideoCoreRtcOnDeviceLoaded);
static FVideoCoreRtcOnDeviceLoaded OnDeviceLoaded;
static FCriticalSection deviceSync;
static mediasoupclient::Device device;

void createWebRtcFactory();

using namespace videocore;

void videocore::initialize() 
{
	mediasoupclient::Initialize();
	FString msVer(mediasoupclient::Version().c_str());

	UE_LOG(LogTemp, Log, TEXT("Initialized mediasoupclient module, version %s"), *msVer);
}

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

mediasoupclient::Device& videocore::getDevice()
{
	return device;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> videocore::getWebRtcFactory()
{
	if (!factory)
		createWebRtcFactory();

	return factory;
}

void videocore::loadMediaSoupDevice(TSharedPtr<FJsonValue> rtpCapabilities)
{
	try
	{
		nlohmann::json caps = fromFJsonValue(rtpCapabilities);
		mediasoupclient::PeerConnection::Options opt;
		opt.factory = getWebRtcFactory();
		device.Load(caps, &opt);
	}
	catch (std::exception& e)
	{
		UE_LOG(LogTemp, Warning, TEXT("Caught exception trying to load mediasoup device: %s"),
			ANSI_TO_TCHAR(e.what()));
	}

	if (device.IsLoaded())
	{
		FScopeLock Lock(&deviceSync);
		OnDeviceLoaded.Broadcast();
		OnDeviceLoaded.Clear();
	}
}

void videocore::ensureDeviceLoaded(std::function<void(mediasoupclient::Device&)> cb)
{
	FScopeLock Lock(&deviceSync);

	OnDeviceLoaded.AddLambda([cb]() {
		cb(device);
	});
}

// ****
void createWebRtcFactory()
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