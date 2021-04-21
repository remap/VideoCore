//
// video-core-rtc.cpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#include "video-core-rtc.hpp"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_device/include/fake_audio_device.h"
#include "api/create_peerconnection_factory.h"
#include "system_wrappers/include/clock.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "SIOJConvert.h"

// change this to true to use WebrRTC's default audio device module
static bool useDefaultAudioDeviceModule = true; 

/* MediaStreamTrack holds reference to the threads of the PeerConnectionFactory.
 * Use plain pointers in order to avoid threads being destructed before tracks.
 */
static std::unique_ptr<rtc::Thread> networkThread;
static std::unique_ptr<rtc::Thread> signalingThread;
static std::unique_ptr<rtc::Thread> workerThread;

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory;

DECLARE_MULTICAST_DELEGATE(FVideoCoreRtcOnDeviceLoaded);
DECLARE_MULTICAST_DELEGATE_OneParam(FVideoCoreRtcOnDeviceLoadFailed, FString);
static FVideoCoreRtcOnDeviceLoaded OnDeviceLoaded;
static FVideoCoreRtcOnDeviceLoadFailed OnDeviceLoadFailed;
static FCriticalSection deviceSync;
static mediasoupclient::Device device;

int videocore::kDefaultTextureWidth = 1920;
int videocore::kDefaultTextureHeight = 1080;

void createWebRtcFactory(bool);

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

bool videocore::getIsUsingDefaultAdm()
{
	return useDefaultAudioDeviceModule;
}

mediasoupclient::Device& videocore::getDevice()
{
	return device;
}

rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> videocore::getWebRtcFactory()
{
	if (!factory)
		createWebRtcFactory(useDefaultAudioDeviceModule);

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

		if (!device.IsLoaded())
		{
			FScopeLock Lock(&deviceSync);
			OnDeviceLoadFailed.Broadcast(ANSI_TO_TCHAR(e.what()));
			OnDeviceLoadFailed.Clear();
		}
	}

	if (device.IsLoaded())
	{
		FScopeLock Lock(&deviceSync);
		OnDeviceLoaded.Broadcast();
		OnDeviceLoaded.Clear();
	}
}

void videocore::ensureDeviceLoaded(std::function<void(mediasoupclient::Device&)> cb, 
	std::function<void(std::string reason)> errCb)
{
	FScopeLock Lock(&deviceSync);

	if (device.IsLoaded())
		cb(device);
	else
	{
		OnDeviceLoaded.AddLambda([cb]() {
			cb(device);
		});
		if (errCb)
			OnDeviceLoadFailed.AddLambda([errCb](FString reason) {
				errCb(std::string(TCHAR_TO_ANSI(*reason)));
			});
	}
}

std::string videocore::generateUUID()
{
	FGuid guid = FGuid::NewGuid();

	return TCHAR_TO_ANSI(*guid.ToString());
}

// ****
void createWebRtcFactory(bool useDefaultAdm)
{
	networkThread = rtc::Thread::Create(); // .release();
	signalingThread = rtc::Thread::Create(); //  .release();
	workerThread = rtc::Thread::Create(); // .release();

	networkThread->SetName("network_thread", nullptr);
	signalingThread->SetName("signaling_thread", nullptr);
	workerThread->SetName("worker_thread", nullptr);

	if (!networkThread->Start() || !signalingThread->Start() || !workerThread->Start())
	{
		UE_LOG(LogTemp, Error, TEXT("thread start errored"));
	}

	//webrtc::PeerConnectionInterface::RTCConfiguration config;
	
	rtc::scoped_refptr<webrtc::AudioDeviceModule> adm(useDefaultAdm ? nullptr : new rtc::RefCountedObject<webrtc::FakeAudioDeviceModule>());

	if (!useDefaultAdm && !adm)
	{
		UE_LOG(LogTemp, Error, TEXT("audio capture module creation errored. using default ADM instead"));
	}

	factory = webrtc::CreatePeerConnectionFactory(
		networkThread.get(),
		workerThread.get(),
		signalingThread.get(),
		adm, // nullptr -- default ADM
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