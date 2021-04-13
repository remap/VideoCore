//
// VideoCoreMediaReceiver.cpp
//
//  Generated on February 18 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreMediaReceiver.h"
#include "native/video-core-rtc.hpp"
#include "native/video-core-audio-buffer.hpp"
#include "SIOJConvert.h"
#include "Texture2DResource.h"
#include "third_party/libyuv/include/libyuv.h"
#include "VideoCoreSoundWave.h"
#include "CULambdaRunnable.h"
#include "Misc/ScopeTryLock.h"

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
	, remoteClientState_(EClientState::Offline)
	, stream_(nullptr)
	, audioConsumer_(nullptr)
	, videoConsumer_(nullptr)
	, audioBuffer_(make_shared<videocore::AudioBuffer>(256, 2048))
{
	initTexture(kTextureWidth, kTextureHeight);
}

UVideoCoreMediaReceiver::~UVideoCoreMediaReceiver()
{
}

void UVideoCoreMediaReceiver::Init(UVideoCoreSignalingComponent* vcSiganlingComponent)
{
	UE_LOG(LogTemp, Log, TEXT("VideoCoreMediaReceiver::Init"));

	vcComponent_ = vcSiganlingComponent;
	setupRenderThreadCallback();

	// setup callbacks
	setupSocketCallbacks();

	// setup consumer objects
	vcComponent_->invokeWhenRecvTransportReady([&](const mediasoupclient::RecvTransport* t) {
		createStream();

		// check if we should auto-consume
		if (this->AutoConsume && !this->clientId.IsEmpty())
			Consume(this->clientId);
	});
}

void UVideoCoreMediaReceiver::Consume(FString clientId)
{
	if (audioConsumer_ || videoConsumer_)
		Stop();

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
			FString videoToFetch = videoStreamHintFilter_.IsEmpty() ? "" : videoStreamHintFilter_;
			FString audioToFetch = audioStreamHintFilter_.IsEmpty() ? "" : audioStreamHintFilter_;

			for (auto d : m->GetArrayField(this->clientId))
			{
				auto obj = d->AsObject();
				auto sId = obj->GetStringField("id");
				auto hint = obj->GetStringField("hint");

				if (obj->GetStringField("kind").Equals("video") && !consumeVideo &&
					(videoToFetch.IsEmpty() || videoToFetch.Equals(hint)))
				{
					consume(vcComponent_->getRecvTransport(), TCHAR_TO_ANSI(*sId));
					consumeVideo = true;
				}
				if (obj->GetStringField("kind").Equals("audio") && !consumeAudio &&
					(audioToFetch.IsEmpty() || audioToFetch.Equals(hint)))
				{
					consume(vcComponent_->getRecvTransport(), TCHAR_TO_ANSI(*sId));
					consumeAudio = true;
				}
			}

			setState(m->GetArrayField(this->clientId).Num() ? EClientState::Producing : EClientState::NotProducing);
		}
		else
		{
			setState(EClientState::Offline);
			UE_LOG(LogTemp, Warning, TEXT("received error fetching client streams (client id %s): client is offline"),
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
		stats.AudioBufferSize = IsValid(soundWave_) ? soundWave_->GetAvailableAudioByteCount() : 0; // audioBuffer_->length();
		stats.AudioBufferState.X = audioBuffer_->size();
		stats.AudioBufferState.Y = audioBuffer_->capacity();
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

FString UVideoCoreMediaReceiver::GetVideoProducerId() const
{
	return videoProducerId;
}

FString UVideoCoreMediaReceiver::GetAudioProducerId() const
{
	return audioProducerId;
}

void UVideoCoreMediaReceiver::SetVideoStreamFilter(FString videoStreamHint)
{
	videoStreamHintFilter_ = videoStreamHint;
}

void UVideoCoreMediaReceiver::SetAudioStreamFilter(FString audioStreamHint)
{
	audioStreamHintFilter_ = audioStreamHint;
}

void UVideoCoreMediaReceiver::BeginDestroy()
{
	shutdown();

	for (auto hndl : callbackHandles_)
		hndl.Reset();

	// Call the base implementation of 'BeginDestroy'
	Super::BeginDestroy();
}

// ****
void UVideoCoreMediaReceiver::setupRenderThreadCallback()
{
	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
	FCoreDelegates::OnEndFrameRT.AddUObject(this, &UVideoCoreMediaReceiver::captureVideoFrame);
}

void UVideoCoreMediaReceiver::setupSocketCallbacks()
{
	FDelegateHandle hndl = 
	vcComponent_->onNewClient_.AddLambda([&](FString clientName, FString clientId) {
		if (clientId.Equals(this->clientId))
		{
			setState(EClientState::NotProducing);
		}
	});
	callbackHandles_.Add(hndl);

	hndl =
	vcComponent_->onClientLeft_.AddLambda([&](FString clientId) 
	{
		if (this->clientId.Equals(clientId))
		{
			stopStreaming("audio", "client left");
			stopStreaming("video", "client left");
			setState(EClientState::Offline);
		}
	});
	callbackHandles_.Add(hndl);

	hndl =
	vcComponent_->onNewProducer_.AddLambda([&](FString clientId, FString producerId, FString kind, FString hint) 
	{
		if (this->AutoConsume && isTargetProducer(clientId, producerId, kind, hint))
		{ // our client, started producing media
			UE_LOG(LogTemp, Log, TEXT("new producer %s %s"), *clientId, *producerId);
			setState(EClientState::Producing);

			if (canConsume(kind))
				consume(vcComponent_->getRecvTransport(), TCHAR_TO_ANSI(*producerId));
		}
	});
	callbackHandles_.Add(hndl);
}

void UVideoCoreMediaReceiver::createStream()
{
	UE_LOG(LogTemp, Log, TEXT("Transport ready. Creating stream for consuming"));
	assert(!stream_);

	stream_ = getWebRtcFactory()->CreateLocalMediaStream(vcComponent_->getRecvTransport()->GetId());
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
			
			// special handling for cases when transport hasn't connected yet
			vcComponent_->invokeOnRecvTransportConnect([this, kind]() {
				resumeTrack(kind == "video" ? videoConsumer_ : audioConsumer_);
			});

			consumer = vcComponent_->getRecvTransport()->Consume(this,
				producerId,
				TCHAR_TO_ANSI(*(m->GetStringField(TEXT("producerId")))),
				kind,
				&rtpParams
			);

			setupConsumerTrack(consumer);
			resumeTrack(consumer);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Can not consume stream %s (client %s): %s"),
				ANSI_TO_TCHAR(streamId.c_str()), *this->clientId, *m->GetStringField("error"));
			
			OnStreamingStopped.Broadcast(FString(streamId.c_str()), EMediaTrackKind::Unknown, m->GetStringField("error"));
		}
	});
}

void
UVideoCoreMediaReceiver::setupConsumerTrack(mediasoupclient::Consumer* consumer)
{
	assert(consumer);
	assert(stream_);

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
		if (!videocore::getIsUsingDefaultAdm())
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
		OnStreamingStarted.Broadcast(consumer->GetId().c_str(),
			consumer->GetKind() == "video" ? EMediaTrackKind::Video : EMediaTrackKind::Audio);
	}
}

void
UVideoCoreMediaReceiver::resumeTrack(mediasoupclient::Consumer* consumer)
{
	//nlohmann::json {{"consumerId", consumer->GetId()}}
	auto jj = fromJsonObject({ {"consumerId", consumer->GetId()} });
	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("resume"), jj, [](auto response) {});
}

void 
UVideoCoreMediaReceiver::stopStreaming(const string& kind, const string& reason)
{
	// stop tracks
	if (stream_)
	{
		if (kind == "video")
		{
			FScopeLock RenderLock(&frameBufferSync_);

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

			audioBuffer_->reset();
		}
	}

	string producerId;

	if (videoConsumer_ && kind == "video")
	{
		FScopeLock RenderLock(&frameBufferSync_);

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

		producerId = audioConsumer_->GetId();
		audioConsumer_->Close();
		delete audioConsumer_;
		audioConsumer_ = nullptr;
		audioProducerId.Reset();

		UE_LOG(LogTemp, Log, TEXT("closed audio consumer %s"), ANSI_TO_TCHAR(producerId.c_str()));
	}

	if (producerId != "")
		OnStreamingStopped.Broadcast(producerId.c_str(),
			kind == "video" ? EMediaTrackKind::Video : EMediaTrackKind::Audio,
			FString(ANSI_TO_TCHAR(reason.c_str())));
}

void
UVideoCoreMediaReceiver::shutdown()
{
	stopStreaming("audio", "shutdown");
	stopStreaming("video", "shutdown");

	if (frameBgraBuffer_)
	{
		free(frameBgraBuffer_);
		bufferSize_ = 0;
	}
}

void
UVideoCoreMediaReceiver::OnTransportClose(mediasoupclient::Consumer* consumer)
{
	UE_LOG(LogTemp, Log, TEXT("Consumer transport closed"));
	
	if (consumer == audioConsumer_)
		stopStreaming("audio", "transport closed");

	if (consumer == videoConsumer_)
		stopStreaming("video", "transport closed");
}

// called by webrtc on worker thread
void
UVideoCoreMediaReceiver::OnFrame(const webrtc::VideoFrame& vf)
{
	/*UE_LOG(LogTemp, Log, TEXT("Incoming frame %dx%d %udms %d bytes, texture %d"),
		vf.width(), vf.height(), vf.timestamp(), vf.size(),
		vf.is_texture());*/

	int vfWidth = vf.width();
	int vfHeight = vf.height();

	if (!frameBgraBuffer_ ||
		frameWidth_ != vfWidth || frameHeight_ != vfHeight)
	{
		UE_LOG(LogTemp, Log, TEXT("need frame buffer %dx%d (current %dx%d)"),
			vfWidth, vfHeight, frameWidth_, frameHeight_);

		initFrameBuffer(vfWidth, vfHeight);
	}

	webrtc::VideoFrameBuffer::Type t = vf.video_frame_buffer()->type();
	if (t == webrtc::VideoFrameBuffer::Type::kI420)
	{
		const webrtc::I420BufferInterface* vfBuf = vf.video_frame_buffer()->GetI420();

		if (vfBuf)
		{
			FScopeLock RenderLock(&frameBufferSync_);
			assert(frameBgraBuffer_);

			libyuv::I420ToARGB
				//libyuv::I420ToBGRA
				(vfBuf->DataY(), vfBuf->StrideY(),
					vfBuf->DataU(), vfBuf->StrideU(),
					vfBuf->DataV(), vfBuf->StrideV(),
					frameBgraBuffer_, 4 * frameWidth_, frameWidth_, frameHeight_);

			framesReceived_++;
			hasNewFrame_ = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Incoming frame empty buffer"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Unsupported incoming frame type: %d"), t);
	}

	// TODO: track this
	// sometimes, the callback gets cleared
	// track here https://udn.unrealengine.com/s/question/0D54z00006u2hUxCAI/onendframert-callback-gets-reset
	if (!FCoreDelegates::OnEndFrameRT.IsBoundToObject(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s capture frame callback got reset"), *clientId);
		setupRenderThreadCallback();
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
		{
			if (!isCreatingSounWave_)
			{
				AsyncTask(ENamedThreads::GameThread, [=]() {
					setupSoundWave(number_of_channels, bits_per_sample, sample_rate);
				});
				isCreatingSounWave_ = true;
			}
			return;
		}
		else
		{
			AudioDataDescription add{ number_of_channels, bits_per_sample, sample_rate };
			if (adataDesc_ != add)
			{
				soundWave_->SetSampleRate(sample_rate);
				soundWave_->NumChannels = number_of_channels;
				soundWave_->SampleByteSize = (int32)ceil((float)bits_per_sample / 8.);
				adataDesc_ = add;
			}

			size_t nBytes = number_of_channels * number_of_frames * (int32)ceil((float)bits_per_sample / 8.);
			soundWave_->QueueAudio((uint8*)audio_data, nBytes);
		}

		/*size_t nBytes = number_of_channels * number_of_frames * (int32)ceil((float)bits_per_sample / 8.);
		audioBuffer_->enque((uint8_t*)audio_data, nBytes);
		samplesReceived_ += number_of_frames;*/
	}
}

// called on game render thread
void
UVideoCoreMediaReceiver::captureVideoFrame()
{
	FScopeTryLock TryTextureLock(&videoTextureSync_);

	if (TryTextureLock.IsLocked() && hasNewFrame_)
	{
		// have to check texture resource size instead of GetSizeX()
		// more info here https://udn.unrealengine.com/s/question/0D54z00006u2GRvCAM/correct-way-to-update-utexture-size
		bool textureInitialized = IsValid(videoTexture_) &&
			videoTexture_->Resource->TextureRHI->GetSizeXYZ().X == frameWidth_ &&
			videoTexture_->Resource->TextureRHI->GetSizeXYZ().Y == frameHeight_;

		if (!textureInitialized)
		{
			assert(frameWidth_ > 0); assert(frameHeight_ > 0);

			//textureInitActive_ = true;
			FCULambdaRunnable::RunShortLambdaOnGameThread([this]() {
				FScopeLock TextureLock(&videoTextureSync_);

				UE_LOG(LogTemp, Log, TEXT("texture mismatch. need %dx%d current %dx%d"),
					frameWidth_, frameHeight_,
					IsValid(videoTexture_) ? videoTexture_->Resource->TextureRHI->GetSizeXYZ().X : 0,
					IsValid(videoTexture_) ? videoTexture_->Resource->TextureRHI->GetSizeXYZ().Y : 0);

				initTexture(frameWidth_, frameHeight_);
				//textureInitActive_ = false;
			});
		}
		else
		{
			FScopeLock RenderLock(&frameBufferSync_);

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
		//videoTexture_->ReleaseResource();

		if (FTextureResource* TextureResource = videoTexture_->Resource)// new FTextureResource())
		{
			//videoTexture_->Resource = TextureResource;

			// Set the default video texture to reference nothing
			TRefCountPtr<FRHITexture2D> RenderableTexture;
			FRHIResourceCreateInfo CreateInfo = { FClearValueBinding(FLinearColor::Black) };

			RenderableTexture = RHICreateTexture2D(width, height,
				EPixelFormat::PF_B8G8R8A8, 1, 1,
				TexCreate_Dynamic | TexCreate_SRGB, CreateInfo);

			TextureResource->TextureRHI = (FTextureRHIRef&)RenderableTexture;

			ENQUEUE_RENDER_COMMAND(FVCVideoTexture2DUpdateTextureReference)(
				[this, RenderableTexture](FRHICommandListImmediate& RHICmdList) {

				RHIUpdateTextureReference(videoTexture_->TextureReference.TextureReferenceRHI, RenderableTexture);
			});

			FlushRenderingCommands();
		}
	}
}

void
UVideoCoreMediaReceiver::initFrameBuffer(int width, int height)
{
	// TODO: replace with smarter realloc-y logic
	if (frameBgraBuffer_)
		free(frameBgraBuffer_);

	frameWidth_ = width;
	frameHeight_ = height;
	bufferSize_ = width * height * 4;
	frameBgraBuffer_ = (uint8_t*)malloc(bufferSize_);
	memset(frameBgraBuffer_, 0, bufferSize_);
	hasNewFrame_ = false;

	UE_LOG(LogTemp, Log, TEXT("frame buffer initialized - %d bytes"), bufferSize_);
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
	isCreatingSounWave_ = false;
}

int32 
UVideoCoreMediaReceiver::GeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	FScopeLock Lock(&audioSyncContext_);

	int32 bytesPerSample = (int32)ceil(((double)adataDesc_.bps_) / 8.);
	if (bytesPerSample)
	{
		int32 nCopiedSamples = 0;

		int32 bytesAsked = bytesPerSample * NumSamples;
		
		OutAudio.Reset();

		int32 bytesCopy = audioBuffer_->deque(OutAudio.GetData(), bytesAsked);

		nCopiedSamples = bytesCopy / bytesPerSample;
		return nCopiedSamples;
	}
	return 0;
}

void
UVideoCoreMediaReceiver::setState(EClientState state)
{
	remoteClientState_ = state;
	OnClientStateChanged.Broadcast(remoteClientState_);
}

bool
UVideoCoreMediaReceiver::isTargetProducer(FString clientId, FString producerId, FString kind, FString hint)
{
	// check if filters enabled
	if (kind.Equals("video") && !videoStreamHintFilter_.IsEmpty())
	{
		return clientId.Equals(this->clientId) && hint.Equals(videoStreamHintFilter_);
	}
	else if (kind.Equals("audio") && !audioStreamHintFilter_.IsEmpty())
	{
		return clientId.Equals(this->clientId) && hint.Equals(audioStreamHintFilter_);
	}
	else
	{
		return clientId.Equals(this->clientId);
	}

	return false;
}

bool 
UVideoCoreMediaReceiver::canConsume(FString kind)
{
	return !audioConsumer_ && kind.Equals("audio") || !videoConsumer_ && kind.Equals("video");
}