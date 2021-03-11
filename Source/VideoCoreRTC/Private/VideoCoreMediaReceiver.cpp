//
// VideoCoreMediaReceiver.cpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreMediaReceiver.h"
#include "native/video-core-rtc.hpp"
#include "SIOJConvert.h"
#include "Texture2DResource.h"
#include "third_party/libyuv/include/libyuv.h"
#include "VideoCoreSoundWave.h"

#include <Engine/Texture.h>
#include <ExternalTexture.h>
#include <TextureResource.h>
#include <Misc/Guid.h>

// TODO: make it configurable from the editor
static int kTextureWidth = 1920; 
static int kTextureHeight = 1080;

using namespace std;
using namespace videocore;

UVideoCoreMediaReceiver::UVideoCoreMediaReceiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bufferSize_(0)
	, frameWidth_(0)
	, frameHeight_(0)
	, frameBgraBuffer_(nullptr)
	, videoTexture_(nullptr)
	, clientState_(EClientState::Offline)
	, stream_(nullptr)
	, recvTransport_(nullptr)
	, audioConsumer_(nullptr)
	, videoConsumer_(nullptr)
{
	initTexture(kTextureWidth, kTextureHeight);
}

UVideoCoreMediaReceiver::~UVideoCoreMediaReceiver()
{
}

void UVideoCoreMediaReceiver::Init(UVideoCoreSignalingComponent* vcSiganlingComponent)
{
	vcComponent_ = vcSiganlingComponent;
	
	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
	FCoreDelegates::OnEndFrameRT.AddUObject(this, &UVideoCoreMediaReceiver::captureVideoFrame);

	// setup callbacks
	setupSocketCallbacks();

	// setup consumer objects
	ensureDeviceLoaded([this](mediasoupclient::Device&) {
		setupConsumer();
	});

	// check if we should auto-consume
	if (AutoConsume && !this->clientId.IsEmpty())
		OnTransportReady.AddLambda([this]() { 
			Consume(this->clientId); 
		});
}

void UVideoCoreMediaReceiver::Consume(FString clientId)
{
	this->clientId = clientId;

	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetBoolField(TEXT("forceTcp"), false);
	TArray< TSharedPtr<FJsonValue> > arr;
	arr.Add(USIOJConvert::ToJsonValue(this->clientId));
	obj->SetArrayField(TEXT("clientIds"), arr);

	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("getClientStreams"), obj, [this](auto response) {
		auto m = response[0]->AsObject();
		if (m->HasField(this->clientId))
		{
			bool consumeVideo = false, consumeAudio = false;
			for (auto d : m->GetArrayField(this->clientId))
			{
				auto obj = d->AsObject();
				// TODO: check if already streaming video and reset
				if (obj->GetStringField("kind").Equals("video") && !consumeVideo)
				{
					consume(recvTransport_, TCHAR_TO_ANSI(*obj->GetStringField("id")));
					consumeVideo = true;
				}
				if (obj->GetStringField("kind").Equals("audio") && !consumeAudio)
				{
					consume(recvTransport_, TCHAR_TO_ANSI(*obj->GetStringField("id")));
					consumeAudio = true;
				}
			}

			setState(m->GetArrayField(this->clientId).Num() ? EClientState::Producing : EClientState::NotProducing);
		}
		else
		{
			setState(EClientState::Offline);
			UE_LOG(LogTemp, Warning, TEXT("received error fetching client streams (client id %s): client if offline"),
				*this->clientId);
		}
	});
}

void UVideoCoreMediaReceiver::Stop()
{
	stopStreaming("video");
	stopStreaming("audio");
}

bool UVideoCoreMediaReceiver::hasTrackOfType(EMediaTrackKind Kind) const
{
	if (stream_.get())
		return Kind == EMediaTrackKind::Video ? stream_->GetVideoTracks().size() : stream_->GetAudioTracks().size();

	return false;
}

bool UVideoCoreMediaReceiver::getIsTrackEnabled(EMediaTrackKind Kind) const
{
	if (hasTrackOfType(Kind))
	{
		if (Kind == EMediaTrackKind::Video)
			return stream_->GetVideoTracks()[0]->enabled();
		else
			return stream_->GetAudioTracks()[0]->enabled();
	}

	return false;
}

void UVideoCoreMediaReceiver::setTrackEnabled(EMediaTrackKind Kind, bool enabled)
{
	if (hasTrackOfType(Kind))
	{
		if (Kind == EMediaTrackKind::Video)
			stream_->GetVideoTracks()[0]->set_enabled(enabled);
		else
			stream_->GetAudioTracks()[0]->set_enabled(enabled);
	}
}

EMediaTrackState UVideoCoreMediaReceiver::getTrackState(EMediaTrackKind Kind) const
{
	if (hasTrackOfType(Kind))
	{
		auto s = (Kind == EMediaTrackKind::Video ? stream_->GetVideoTracks()[0]->state() : stream_->GetAudioTracks()[0]->state());

		return s == webrtc::MediaStreamTrackInterface::TrackState::kEnded ? EMediaTrackState::Ended : EMediaTrackState::Live;
	}

	return EMediaTrackState::Unknown;
}

FVideoCoreMediaStreamStatistics UVideoCoreMediaReceiver::getStats() const
{
	FVideoCoreMediaStreamStatistics stats;

	FMemory::Memset(&stats, 0, sizeof(stats));

	if (hasTrackOfType(EMediaTrackKind::Audio))
	{
		stats.AudioLevel = stream_->GetAudioTracks()[0]->GetSignalLevel(&(stats.AudioLevel));
		if (stream_->GetAudioTracks()[0]->GetAudioProcessor())
		{
			webrtc::AudioProcessorInterface::AudioProcessorStatistics ss = stream_->GetAudioTracks()[0]->GetAudioProcessor()->GetStats(true);
			stats.TypingNoiseDetected = ss.typing_noise_detected;
			stats.VoiceDetected = ss.apm_statistics.voice_detected.has_value() ? ss.apm_statistics.voice_detected.value() : false;
		}
		
		// TODO: addd more stats if needed
		stats.SamplesReceived = (int)samplesReceived_;
	}

	if (hasTrackOfType(EMediaTrackKind::Video))
	{
		webrtc::VideoTrackInterface::ContentHint hint = stream_->GetVideoTracks()[0]->content_hint();
		switch (hint)
		{
		case webrtc::VideoTrackInterface::ContentHint::kNone:
			stats.VideoContentHint = FString("None");
			break;
		case webrtc::VideoTrackInterface::ContentHint::kFluid:
			stats.VideoContentHint = FString("Fluid");
			break;
		case webrtc::VideoTrackInterface::ContentHint::kDetailed:
			stats.VideoContentHint = FString("Detailed");
			break;
		case webrtc::VideoTrackInterface::ContentHint::kText:
			stats.VideoContentHint = FString("Text");
			break;
		default:
			stats.VideoContentHint = FString("Unknown");
			break;
		}
		
		stats.FramesReceived = (int)framesReceived_;
	}

	return stats;
}

void UVideoCoreMediaReceiver::BeginDestroy()
{
	shutdown();

	for (auto hndl : callbackHandles_)
		hndl.Reset();

	// Call the base implementation of 'BeginDestroy'
	Super::BeginDestroy();
}

string UVideoCoreMediaReceiver::generateUUID()
{
	FGuid guid = FGuid::NewGuid();

	return TCHAR_TO_ANSI(*guid.ToString());
}

// ****
void UVideoCoreMediaReceiver::setupSocketCallbacks()
{
	FDelegateHandle hndl = 
	vcComponent_->onNewClient_.AddLambda([&](FString clientName, FString clientId) {
		if (clientId.Equals(this->clientId))
		{
			setState(EClientState::NotProducing);
		}
	});

	hndl =
	vcComponent_->onClientLeft_.AddLambda([&](FString clientId) 
	{
		if (this->clientId.Equals(clientId))
		{
			stopStreaming("audio");
			stopStreaming("video");
			setState(EClientState::Offline);
		}
	});
	callbackHandles_.Add(hndl);

	hndl =
	vcComponent_->onNewProducer_.AddLambda([&](FString clientId, FString producerId) 
	{
		if (!this->clientId.IsEmpty() && this->clientId == clientId && this->AutoConsume)
		{ // our client, started producing media
			UE_LOG(LogTemp, Log, TEXT("new producer %s %s"), *clientId, *producerId);
			setState(EClientState::Producing);
			consume(recvTransport_, TCHAR_TO_ANSI(*producerId));
		}
	});
	callbackHandles_.Add(hndl);
}

void UVideoCoreMediaReceiver::setupConsumer()
{
	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetBoolField(TEXT("forceTcp"), false);

	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("createConsumerTransport"), obj, [this](auto response) {
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

			stream_ = getWebRtcFactory()->CreateLocalMediaStream(consumerData["id"].get<std::string>()); // UVideoCoreMediaReceiver::generateUUID());

			OnTransportReady.Broadcast();
		}
	});
}

void
UVideoCoreMediaReceiver::consume(mediasoupclient::RecvTransport* t, const string& streamId)
{
	nlohmann::json caps = { 
		{"rtpCapabilities", getDevice().GetRtpCapabilities()}, 
		{"streamId", streamId }
	};
	auto jj = fromJsonObject(caps);

	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("consume"), jj, [this, streamId](auto response) {
		auto m = response[0]->AsObject();

		/*UE_LOG(LogTemp, Log, TEXT("Server consume reply %s"), *USIOJConvert::ToJsonString(m));*/

		if (!m->HasField(TEXT("error")))
		{
			nlohmann::json rtpParams = fromFJsonObject(m->GetObjectField(TEXT("rtpParameters")));
			std::string producerId(TCHAR_TO_ANSI(*(m->GetStringField(TEXT("id")))));

			string kind(TCHAR_TO_ANSI(*(m->GetStringField(TEXT("kind")))));
			stopStreaming(kind);
			mediasoupclient::Consumer*& consumer = (kind == "video" ? videoConsumer_ : audioConsumer_);

			UE_LOG(LogTemp, Log, TEXT("setup %s consumer for producer %s"), 
				ANSI_TO_TCHAR(kind.c_str()),
				ANSI_TO_TCHAR(producerId.c_str()));

			consumer = recvTransport_->Consume(this,
				producerId,
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("producerId")))),
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("kind")))),
				&rtpParams
			);

			setupConsumerTrack(consumer);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Can not consume stream %s (client %s): %s"),
				ANSI_TO_TCHAR(streamId.c_str()), *this->clientId, *m->GetStringField("error"));
			
			OnStreamingStopped.Broadcast(FString(streamId.c_str()), m->GetStringField("error"));
		}
	});
}

void
UVideoCoreMediaReceiver::setupConsumerTrack(mediasoupclient::Consumer* consumer)
{
	assert(consumer);

	if (stream_)
	{
		if (consumer->GetKind() == "video")
		{
			framesReceived_ = 0;
			stream_->AddTrack((webrtc::VideoTrackInterface*)consumer->GetTrack());

			rtc::VideoSinkWants sinkWants;
			// TODO: use these for downgrading/upgrading the resolution
			//sinkWants.max_pixel_count
			//sinkWants.target_pixel_count
			((webrtc::VideoTrackInterface*)consumer->GetTrack())->AddOrUpdateSink(this, sinkWants);

			videoProducerId = FString(consumer->GetId().c_str());
		}
		else
		{
			samplesReceived_ = 0;
			stream_->AddTrack((webrtc::AudioTrackInterface*)consumer->GetTrack());
			((webrtc::AudioTrackInterface*)consumer->GetTrack())->AddSink(this);

			audioProducerId = FString(consumer->GetId().c_str());
		}

		UE_LOG(LogTemp, Log, TEXT("added %s track %s (consumer %s)"),
			ANSI_TO_TCHAR(consumer->GetKind().c_str()),
			ANSI_TO_TCHAR(consumer->GetTrack()->id().c_str()),
			ANSI_TO_TCHAR(consumer->GetId().c_str()));

		// TODO: check if call to set_enabled is really needed
		webrtc::MediaStreamTrackInterface::TrackState s = consumer->GetTrack()->state();
		if (s == webrtc::MediaStreamTrackInterface::kLive)
		{
			consumer->GetTrack()->set_enabled(true);
			OnStreamingStarted.Broadcast(consumer->GetId().c_str(), consumer->GetKind().c_str());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("stream object is NULL"));
	}
}

void 
UVideoCoreMediaReceiver::stopStreaming(const string& kind)
{
	// stop tracks
	if (stream_)
	{
		if (kind == "video")
		{
			FScopeLock RenderLock(&renderSyncContext_);

			for (auto t : stream_->GetVideoTracks())
			{
				t->set_enabled(false);
				t->RemoveSink(this);
				stream_->RemoveTrack(t);

				UE_LOG(LogTemp, Log, TEXT("removed video track %s"), ANSI_TO_TCHAR(t->id().c_str()));
			}
		}
		else
		{
			FScopeLock AudioLock(&audioSyncContext_);

			for (auto t : stream_->GetAudioTracks())
			{
				t->set_enabled(false);
				t->RemoveSink(this);
				stream_->RemoveTrack(t);

				UE_LOG(LogTemp, Log, TEXT("removed audio track %s"), ANSI_TO_TCHAR(t->id().c_str()));
			}

			audioBuffer_.clear();
		}
	}

	string producerId;

	if (videoConsumer_ && kind == "video")
	{
		FScopeLock RenderLock(&renderSyncContext_);

		producerId = videoConsumer_->GetId();
		videoConsumer_->Close();
		delete videoConsumer_;
		videoConsumer_ = nullptr;
		videoProducerId.Reset();

		UE_LOG(LogTemp, Log, TEXT("closed video consumer %s"), ANSI_TO_TCHAR(producerId.c_str()));
	}
	if (audioConsumer_ && kind == "audio")
	{
		FScopeLock AudioLock(&audioSyncContext_);

		producerId = videoConsumer_->GetId();
		audioConsumer_->Close();
		delete audioConsumer_;
		audioConsumer_ = nullptr;
		audioProducerId.Reset();

		UE_LOG(LogTemp, Log, TEXT("closed audio consumer %s"), ANSI_TO_TCHAR(producerId.c_str()));
	}

	if (producerId != "")
		OnStreamingStopped.Broadcast(producerId.c_str(), "initiated by client");
}

void
UVideoCoreMediaReceiver::shutdown()
{
	FScopeLock RenderLock(&renderSyncContext_);
	FScopeLock AudioLock(&audioSyncContext_);

	if (stream_)
	{
		for (auto t : stream_->GetVideoTracks())
		{
			t->set_enabled(false);
			t->RemoveSink(this);
			stream_->RemoveTrack(t);
		}

		for (auto t : stream_->GetAudioTracks())
		{
			t->set_enabled(false);
			t->RemoveSink(this);

			stream_->RemoveTrack(t);
		}
	}

	// TODO: notify server we're closing transport and consumer
	if (videoConsumer_)
	{
		videoConsumer_->Close();
		delete videoConsumer_;
	}
	if (audioConsumer_)
	{
		audioConsumer_->Close();
		delete audioConsumer_;
	}
	if (recvTransport_)
	{
		recvTransport_->Close();
		delete recvTransport_;
	}

	if (frameBgraBuffer_)
	{
		free(frameBgraBuffer_);
		bufferSize_ = 0;
	}
}

std::future<void>
UVideoCoreMediaReceiver::OnConnect(mediasoupclient::Transport* transport, const nlohmann::json& dtlsParameters)
{
	UE_LOG(LogTemp, Log, TEXT("Transport OnConnect"));

	std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();

	nlohmann::json m = {
		{"transportId", },
		{"dtlsParameters", dtlsParameters} };

	auto obj = USIOJConvert::MakeJsonObject();
	obj->SetStringField(TEXT("transportId"), transport->GetId().c_str());
	obj->SetField(TEXT("dtlsParameters"), fromJsonObject(dtlsParameters));

	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("connectConsumerTransport"), obj, [promise](auto response) {
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
UVideoCoreMediaReceiver::OnConnectionStateChange(mediasoupclient::Transport* transport, const std::string& connectionState)
{
	FString str(connectionState.c_str());
	UE_LOG(LogTemp, Log, TEXT("Transport connection state change %s"), *str);

	if (connectionState == "connected")
		vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("resume"), nullptr, [](auto response) {
	});
}

void
UVideoCoreMediaReceiver::OnTransportClose(mediasoupclient::Consumer* consumer)
{
	UE_LOG(LogTemp, Log, TEXT("Consumer transport closed"));
	// TODO: cleanup and notify user
}

void
UVideoCoreMediaReceiver::OnFrame(const webrtc::VideoFrame& vf)
{
	/*UE_LOG(LogTemp, Log, TEXT("Incoming frame %dx%d %udms %d bytes, texture %d"),
		vf.width(), vf.height(), vf.timestamp(), vf.size(),
		vf.is_texture());*/

	if (!videoTexture_ ||
		frameWidth_ != vf.width() ||
		frameHeight_ != vf.height() ||
		!frameBgraBuffer_)
	{
		FScopeLock RenderLock(&renderSyncContext_);

		frameWidth_ = vf.width();
		frameHeight_ = vf.height();
		needFrameBuffer_ = true;
	}

	webrtc::VideoFrameBuffer::Type t = vf.video_frame_buffer()->type();
	if (t == webrtc::VideoFrameBuffer::Type::kI420)
	{
		const webrtc::I420BufferInterface *vfBuf = vf.video_frame_buffer()->GetI420();

		if (vfBuf)
		{
			FScopeLock RenderLock(&renderSyncContext_);

			if (frameBgraBuffer_ && !needFrameBuffer_)
			{
				libyuv::I420ToARGB
				//libyuv::I420ToBGRA
					(vfBuf->DataY(), vfBuf->StrideY(),
						vfBuf->DataU(), vfBuf->StrideU(),
						vfBuf->DataV(), vfBuf->StrideV(),
						frameBgraBuffer_, 4 * frameWidth_, frameWidth_, frameHeight_);
				hasNewFrame_ = true;
				framesReceived_++;
			}
		}
	}
	else
	{
		// TODO: set error
	}
}

void
UVideoCoreMediaReceiver::OnData(const void* audio_data, int bits_per_sample, int sample_rate, 
	size_t number_of_channels, size_t number_of_frames)
{
	/*UE_LOG(LogTemp, Log, TEXT("receiveing audio data bps %d sample rate %d ch %d frame# %d"), 
		bits_per_sample, sample_rate, number_of_channels, number_of_frames);*/

	{
		FScopeLock Lock(&audioSyncContext_);
		if (!IsValid(soundWave_))
			AsyncTask(ENamedThreads::GameThread, [=]() {
				setupSoundWave(number_of_channels, bits_per_sample, sample_rate);
			});
		
		// copy/store audio data
		// TODO: wrap in a struct, track if anything changes
		nFrames_ = number_of_frames;
		nChannels_ = number_of_channels;
		bps_ = bits_per_sample;
		sampleRate_ = sample_rate;

		// TODO: refactor audiobuffer to reuse memory
		size_t nBytes = number_of_channels * number_of_frames * (int32)ceil((float)bits_per_sample / 8.);
		audioBuffer_.insert(audioBuffer_.end(), (uint8_t*)audio_data, (uint8_t*)audio_data + nBytes);
		samplesReceived_ += number_of_frames;
	}
}

void
UVideoCoreMediaReceiver::captureVideoFrame()
{
	FScopeLock RenderLock(&renderSyncContext_);

	if (needFrameBuffer_)
	{
		initFrameBuffer(frameWidth_, frameHeight_);
		needFrameBuffer_ = false;
	}

	if (frameBgraBuffer_ && hasNewFrame_)
	{
		FUpdateTextureRegion2D region;
		region.SrcX = 0;
		region.SrcY = 0;
		region.DestX = 0;
		region.DestY = 0;
		region.Width = frameWidth_;
		region.Height = frameHeight_;

		FTexture2DResource* res = (FTexture2DResource*)videoTexture_->Resource;
		RHIUpdateTexture2D(res->GetTexture2DRHI(), 0, region, frameWidth_ * 4, frameBgraBuffer_);

		hasNewFrame_ = false;
	}
}

void
UVideoCoreMediaReceiver::initTexture(int width, int height)
{
	if (!videoTexture_)
	{
		videoTexture_ = UTexture2D::CreateTransient(width, height, EPixelFormat::PF_B8G8R8A8);
		videoTexture_->UpdateResource();
		videoTexture_->RefreshSamplerStates();
	}
	else if (videoTexture_->GetSizeX() != width || videoTexture_->GetSizeY() != height)
	{
		videoTexture_->ReleaseResource();

		if (FTextureResource* TextureResource = new FTextureResource())
		{
			videoTexture_->Resource = TextureResource;

			// Set the default video texture to reference nothing
			TRefCountPtr<FRHITexture2D> RenderableTexture;
			FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(FLinearColor::Black) };

			RenderableTexture = RHICreateTexture2D(width, height,
				EPixelFormat::PF_B8G8R8A8, 1, 1,
				TexCreate_Dynamic | TexCreate_SRGB, CreateInfo);

			TextureResource->TextureRHI = (FTextureRHIRef&)RenderableTexture;

			ENQUEUE_RENDER_COMMAND(FNDIMediaTexture2DUpdateTextureReference)(
				[&](FRHICommandListImmediate& RHICmdList) {

				RHIUpdateTextureReference(videoTexture_->TextureReference.TextureReferenceRHI, TextureResource->TextureRHI);
			});
		}
	}
}

void
UVideoCoreMediaReceiver::initFrameBuffer(int width, int height)
{
	initTexture(width, height);

	// TODO: replace with smarter realloc-y logic
	if (frameBgraBuffer_)
		free(frameBgraBuffer_);

	frameWidth_ = width;
	frameHeight_ = height;
	bufferSize_ = width * height * 4;
	frameBgraBuffer_ = (uint8_t*)malloc(bufferSize_);
	memset(frameBgraBuffer_, 0, bufferSize_);
	hasNewFrame_ = false;

	assert(videoTexture_->GetSizeX() == frameWidth_);
	assert(videoTexture_->GetSizeY() == frameHeight_);
}

void
UVideoCoreMediaReceiver::setupSoundWave(int nChannels, int bps, int sampleRate)
{	
	FScopeLock Lock(&audioSyncContext_);

	FString AudioSource(string("audio_source_" + stream_->id()).c_str());
	FName AudioWaveName = FName(*AudioSource);
	EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transient | RF_MarkAsNative;

	soundWave_ = NewObject<UVideoCoreSoundWave>(
		GetTransientPackage(),
		UVideoCoreSoundWave::StaticClass(),
		AudioWaveName,
		Flags);

	if (IsValid(soundWave_))
	{
		soundWave_->Init(this, nChannels, bps, sampleRate);
		OnSoundSourceReady.Broadcast(soundWave_);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create sound wave object"));
	}
}

int32 
UVideoCoreMediaReceiver::GeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	FScopeLock Lock(&audioSyncContext_);

	int32 bytesPerSample = (int32)ceil((float)bps_ / 8.);
	int32 nCopiedSamples = 0;
	
	int32 bytesAsked = bytesPerSample * NumSamples;
	int32 bytesCopy = (bytesAsked > audioBuffer_.size() ? audioBuffer_.size() : bytesAsked);

	OutAudio.Reset();
	OutAudio.AddZeroed(bytesCopy);
	FMemory::Memcpy(OutAudio.GetData(), audioBuffer_.data(), bytesCopy);
	nCopiedSamples = bytesCopy / bytesPerSample;
	audioBuffer_.erase(audioBuffer_.begin(), audioBuffer_.begin() + bytesCopy);

	//UE_LOG(LogTemp, Log, TEXT("buffer level %d bytes"), audioBuffer_.size());

	return nCopiedSamples;
}

void
UVideoCoreMediaReceiver::setState(EClientState state)
{
	clientState_ = state;
	OnClientStateChanged.Broadcast(clientState_);
}
