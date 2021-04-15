//
// VideoCoreMediaSender.cpp
//
//  Generated on March 23 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreMediaSender.h"
#include "CULambdaRunnable.h"
#include "native/video-core-rtc.hpp"
#include "native/video-core-rt-video-source.hpp"

using namespace std;
using namespace videocore;

UVideoCoreMediaSender::UVideoCoreMediaSender(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, videoTrackState_(EMediaTrackState::Unknown)
	, audioTrackState_(EMediaTrackState::Unknown)
{

}

UVideoCoreMediaSender::~UVideoCoreMediaSender()
{

}

bool UVideoCoreMediaSender::Init(UVideoCoreSignalingComponent* vcSiganlingComponent)
{
	vcComponent_ = vcSiganlingComponent;

	if (vcComponent_)
	{
		setupBackBufferTexture();
		setupRenderThreadCallback();
		vcComponent_->invokeWhenSendTransportReady([this](const mediasoupclient::SendTransport*) {
			createStream();

			if (vcComponent_->getSocketIOClientComponent()->bIsConnected)
				checkAutoProduce();
			
		});

		return true;
	}

	return false;
}

bool UVideoCoreMediaSender::Produce(FString contentHint, EMediaTrackKind trackKind)
{
	setStreamHint(contentHint, trackKind);

	if (stream_)
		return startStream(videocore::generateUUID(), trackKind, TCHAR_TO_ANSI(*contentHint));
	else
	{
		onMediaStreamReady_.AddLambda([this, contentHint, trackKind]() {
			this->startStream(videocore::generateUUID(), trackKind, TCHAR_TO_ANSI(*contentHint));
		});
	}

	return false;
}

void UVideoCoreMediaSender::Stop(EMediaTrackKind trackKind)
{
	// notify server we're closing
	string producerId = "";
	switch (trackKind)
	{
	case EMediaTrackKind::Audio:
		if (audioProducer_)
			producerId = audioProducer_->GetId();
		break;
	case EMediaTrackKind::Video:
		if (videoProducer_)
			producerId = videoProducer_->GetId();
		break;
	case EMediaTrackKind::Data:
		break;
	default:
		break;
	}

	if (!producerId.empty())
		vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("closeProducer"),
			videocore::fromJsonObject({ { "id", producerId } }));

	stopStream(trackKind, "user initiated");
}

bool UVideoCoreMediaSender::hasTrackOfType(EMediaTrackKind Kind) const
{
	if (stream_.get())
		return Kind == EMediaTrackKind::Video ? stream_->GetVideoTracks().size() : stream_->GetAudioTracks().size();

	return false;
}

EMediaTrackState UVideoCoreMediaSender::getTrackState(EMediaTrackKind Kind) const
{
	if (hasTrackOfType(Kind))
	{
		return (Kind == EMediaTrackKind::Video ? videoTrackState_ : audioTrackState_);
	}

	return EMediaTrackState::Unknown;
}

void UVideoCoreMediaSender::BeginDestroy()
{
	shutdown();

	for (auto hndl : callbackHandles_)
		hndl.Reset();

	// Call the base implementation of 'BeginDestroy'
	Super::BeginDestroy();
}

// ***
void UVideoCoreMediaSender::OnTransportClose(mediasoupclient::Producer* p)
{
	UE_LOG(LogTemp, Log, TEXT("Producer transport closed."));

	stopStream(EMediaTrackKind::Video, "transport closed");
	stopStream(EMediaTrackKind::Audio, "transport closed");
}

void UVideoCoreMediaSender::setupRenderThreadCallback()
{
	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
	FCoreDelegates::OnEndFrameRT.AddUObject(this, &UVideoCoreMediaSender::tryCopyRenderTarget);
}

bool UVideoCoreMediaSender::startStream(string trackId, EMediaTrackKind trackKind, string hint)
{
	switch (trackKind) {
	case EMediaTrackKind::Audio:
	{
		UE_LOG(LogTemp, Log, TEXT("Setting up audio stream"));
	}
	break;
	case EMediaTrackKind::Video:
	{
		if (videoTrackState_ < EMediaTrackState::Initializing)
		{
			if (IsValid(RenderTarget))
			{
				UE_LOG(LogTemp, Log, TEXT("Setting up video stream"));

				videoTrackState_ = EMediaTrackState::Initializing;

				createVideoSource();
				setupRenderTarget(FrameSize, FrameRate);
				createVideoTrack(trackId);
				createProducer(hint);
				return true;
			}
			else
			{
				OnProduceFailure.Broadcast(trackId.c_str(), trackKind, "render target is not set");
			}
		}
		else
		{
			OnProduceFailure.Broadcast(trackId.c_str(), trackKind, "already producing");
		}
	}
	break;
	default:
	{
		OnProduceFailure.Broadcast(trackId.c_str(), trackKind, "unknown track kind");
	} break;
	};

	return false;
}

void UVideoCoreMediaSender::stopStream(EMediaTrackKind trackKind, string reason)
{
	if (stream_)
	{
		switch (trackKind)
		{
		case EMediaTrackKind::Audio:
		{
			audioTrackState_ = EMediaTrackState::Ended;
		}
			break;
		case EMediaTrackKind::Video:
		{
			{
				FScopeLock RenderLock(&renderSync_);
				videoTrackState_ = EMediaTrackState::Ended;

				if (videoSource_) 
					videoSource_->SetState(webrtc::MediaSourceInterface::SourceState::kEnded);
			}

			for (auto t : stream_->GetVideoTracks())
			{
				t->set_enabled(false);
				stream_->RemoveTrack(t);

				OnStoppedProducing.Broadcast(t->id().c_str(), trackKind, reason.c_str());
				UE_LOG(LogTemp, Log, TEXT("removed video track %s"), ANSI_TO_TCHAR(t->id().c_str()));
			}

			videoTrack_.release();

			if (videoProducer_)
			{
				videoProducer_->Close();
				delete videoProducer_;
				videoProducer_ = nullptr;
			}
		}
			break;
		case EMediaTrackKind::Data:
		{
		}
			break;
		case EMediaTrackKind::Unknown:
		default:
			break;
		}
	}
}

void UVideoCoreMediaSender::setupBackBufferTexture()
{
	BackbufferTexture = UTexture2D::CreateTransient(videocore::kDefaultTextureWidth, videocore::kDefaultTextureHeight);
	BackbufferTexture->UpdateResource();
	BackbufferTexture->RefreshSamplerStates();
}

void UVideoCoreMediaSender::setupRenderTarget(FIntPoint InFrameSize, FFrameRate InFrameRate)
{
	UE_LOG(LogTemp, Log, TEXT("Set up render target %dx%d (%d FPS)"),
		InFrameSize.X, InFrameSize.Y, FrameRate.Numerator);

	FrameSize = InFrameSize;
	FrameRate = InFrameRate;

	// a resource creation structure
	FRHIResourceCreateInfo CreateInfo;

	// perform cleanup on the backbuffer texture
	if (this->backBufferTexture_.IsValid())
	{
		this->backBufferTexture_.SafeRelease();
		this->backBufferTexture_ = nullptr;
	}

	// Recreate the read back texture
	backBufferTexture_ = RHICreateTexture2D(
		FrameSize.X, FrameSize.Y,
		PF_B8G8R8A8, 1, 1,
		TexCreate_CPUReadback,
		CreateInfo
	);

	{ // update resource for BackBuffer Texture2D
		if (FTextureResource* TextureResource = BackbufferTexture->Resource)
		{
			TextureResource->TextureRHI = (FTextureRHIRef&)backBufferTexture_;

			if (IsInRenderingThread())
			{
				RHIUpdateTextureReference(BackbufferTexture->TextureReference.TextureReferenceRHI, backBufferTexture_);
			}
			else
			{
				ENQUEUE_RENDER_COMMAND(FVCVideoTexture2DUpdateTextureReference)(
					[this](FRHICommandListImmediate& RHICmdList) {
					BackbufferTexture->Resource->ReleaseRHI();
					BackbufferTexture->Resource->TextureRHI = (FTextureRHIRef&)backBufferTexture_;
					RHIUpdateTextureReference(BackbufferTexture->TextureReference.TextureReferenceRHI, backBufferTexture_);
				});
				FlushRenderingCommands();
			}
		}
	}

	// Create the RenderTarget descriptor
	renderTargetDesc_ = FPooledRenderTargetDesc::Create2DDesc(
		FrameSize,
		PF_B8G8R8A8,
		FClearValueBinding::None,
		TexCreate_None,
		TexCreate_RenderTargetable,
		false
	);

	// If our RenderTarget is valid change the size
	if (IsValid(this->RenderTarget))
	{
		// Ensure that our render target is the same size as we expect		
		this->RenderTarget->ResizeTarget(FrameSize.X, FrameSize.Y);
	}

	if (videoSource_)
		videoSource_->SetFrameSize(FrameSize.X, FrameSize.Y);
}

void UVideoCoreMediaSender::createStream()
{
	UE_LOG(LogTemp, Log, TEXT("Transport ready. Creating stream for producing"));

	stream_ = getWebRtcFactory()->CreateLocalMediaStream(vcComponent_->getRecvTransport()->GetId());
	onMediaStreamReady_.Broadcast();
}

void UVideoCoreMediaSender::createVideoTrack(string tId)
{
	if (videoSource_)
	{
		UE_LOG(LogTemp, Log, TEXT("Create video track"));

		videoTrack_ = getWebRtcFactory()->CreateVideoTrack(tId, videoSource_.get());
		stream_->AddTrack(videoTrack_);
	}
	
	if (vcComponent_ && vcComponent_->GetWorld())
	{
		vcComponent_->GetWorld()->GetTimerManager().ClearTimer(captureHandle_);
		vcComponent_->GetWorld()->GetTimerManager().SetTimer(captureHandle_, this, &UVideoCoreMediaSender::trySendFrame,
			1 / ((float)FrameRate.Numerator / (float)FrameRate.Denominator), true);
	}
}

void UVideoCoreMediaSender::createAudioTrack(string tId)
{

}

void UVideoCoreMediaSender::createVideoSource()
{
	if (!videoSource_)
	{
		videoSource_.reset();
		videoSource_ = make_shared<RenderTargetVideoTrackSource>();

		UE_LOG(LogTemp, Log, TEXT("Created new video source"));
	}

	videoSource_->SetState(webrtc::MediaSourceInterface::SourceState::kInitializing);
	UE_LOG(LogTemp, Log, TEXT("Set up video source"));
}

void UVideoCoreMediaSender::createProducer(string hint)
{
	if (vcComponent_)
	{
		// TODO: setup layers here
		vector<webrtc::RtpEncodingParameters> encodings;

		if (Bitrates.Num())
		{
			for (auto bps : Bitrates)
			{
				// TODO: expand for configurable FPS and other settings
				webrtc::RtpEncodingParameters p;
				p.max_bitrate_bps = bps;
				encodings.push_back(p);
			}
		}
		else
		{
			webrtc::RtpEncodingParameters p;
			p.max_bitrate_bps = 500000;
			encodings.push_back(p);
			Bitrates.Add(p.max_bitrate_bps.value());
		}

		vcComponent_->invokeOnTransportProduce(videoTrack_->id(), 
			[this, hint](mediasoupclient::SendTransport* t, const std::string& kind,
			nlohmann::json rtpParameters, const nlohmann::json& appData)
		{
			std::shared_ptr<std::promise<string>> promise = std::make_shared<std::promise<string>>();

			// ask server to produce
			nlohmann::json params = {
				{ "transportId", t->GetId() },
				{ "kind", kind },
				{ "rtpParameters", rtpParameters }
			};

			if (!hint.empty())
				params["streamHint"] = hint;

			vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("produce"), fromJsonObject(params),
				[this, promise](auto response) 
			{
				auto m = response[0]->AsObject();
				if (m->HasField("id"))
				{
					promise->set_value(TCHAR_TO_ANSI(*m->GetStringField("id")));

					UE_LOG(LogTemp, Log, TEXT("Server produce reply successfull: %s"), *m->GetStringField("id"));
				}
				else
				{
					promise->set_exception(make_exception_ptr(runtime_error("server:produce returned invalid response: " +
						string(TCHAR_TO_ANSI(*m->GetStringField("error"))))));

					UE_LOG(LogTemp, Log, TEXT("Server produce reply error: %s"), *USIOJConvert::ToJsonString(m));
				}	
			});

			UE_LOG(LogTemp, Log, TEXT("Awaiting server produce reply"));
			return promise->get_future();
		});

		// invoke on non-game thread because this will trigger callback 
		// with request to the server, shouldn't block game thread waiting for the reply
		FCULambdaRunnable::RunLambdaOnBackGroundThread([this, encodings]() {
			UE_LOG(LogTemp, Log, TEXT("Creating video producer"));
			
			string reason = "";
			try
			{
				this->videoProducer_ = vcComponent_->getSendTransport()->Produce(this, videoTrack_, &encodings, nullptr,
					{ { "trackId", videoTrack_->id() } });

				videoTrackState_ = EMediaTrackState::Live;
				videoSource_->SetState(webrtc::MediaSourceInterface::SourceState::kLive);
				videoTrack_->set_enabled(true);

				UE_LOG(LogTemp, Log, TEXT("Producing video now"));
			}
			catch (exception& e)
			{
				videoTrackState_ = EMediaTrackState::Unknown;
				reason = e.what();

				UE_LOG(LogTemp, Log, TEXT("Producing failure: %s"), ANSI_TO_TCHAR(e.what()));
			}

			FCULambdaRunnable::RunShortLambdaOnGameThread([this, reason]() {
				if (videoTrackState_ == EMediaTrackState::Live)
					OnStartProducing.Broadcast(videoTrack_->id().c_str(), EMediaTrackKind::Video);
				else
					OnProduceFailure.Broadcast(videoTrack_->id().c_str(), EMediaTrackKind::Video, reason.c_str());
			});
		});	
	}
}

void UVideoCoreMediaSender::shutdown()
{
	stopStream(EMediaTrackKind::Video, "shutdown");
	stopStream(EMediaTrackKind::Audio, "shutdown");

	{
		FScopeLock RenderLock(&renderSync_);

		// reset the resources
		this->renderTargetDesc_.Reset();
		this->backBufferTexture_.SafeRelease();

		this->backBufferTexture_ = nullptr;
	}
}

void UVideoCoreMediaSender::checkAutoProduce()
{
	if (AutoProduce)
	{
		UE_LOG(LogTemp, Log, TEXT("Auto-produce is ON: setup streams"));

		// TODO: introduce stream hints (ids) as properties and use them here
		if (videoTrackState_ < EMediaTrackState::Initializing) Produce(VideoStreamHint, EMediaTrackKind::Video);
		if (audioTrackState_ < EMediaTrackState::Initializing) Produce(AudioStreamHint, EMediaTrackKind::Audio);
	}
}

void UVideoCoreMediaSender::setStreamHint(FString hint, EMediaTrackKind trackKind)
{
	switch (trackKind)
	{
	case EMediaTrackKind::Audio:
		AudioStreamHint = hint;
		break;
	case EMediaTrackKind::Video:
		VideoStreamHint = hint;
		break;
	case EMediaTrackKind::Data:
		break;
	default:
		break;
	}
}

// called on render thread
void UVideoCoreMediaSender::tryCopyRenderTarget()
{
	FScopeLock RenderLock(&renderSync_);
	int64 time_code = FDateTime::Now().GetTimeOfDay().GetTicks();

	if (videoTrackState_ == EMediaTrackState::Live &&
		IsValid(RenderTarget) && RenderTarget->Resource != nullptr)
	{
		FTimecode tc = FTimecode::FromTimespan(
			FTimespan::FromSeconds(time_code / (float)1e+7),
			FrameRate,
			FTimecode::IsDropFormatTimecodeSupported(FrameRate),
			true	// use roll-over timecode
		);

		if (tc != videoSource_->getLastCaptureTimecode())
		{
			// Get the command list interface
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

			FTexture2DRHIRef SourceTexture = (FTexture2DRHIRef&)RenderTarget->Resource->TextureRHI;
			if (SourceTexture.IsValid())
			{
				bool needsResize = false;
//#if UE_EDITOR
				if (SourceTexture->GetSizeXY() == backBufferTexture_->GetSizeXY())
				{
					RHICmdList.CopyToResolveTarget(SourceTexture, backBufferTexture_, FResolveParams());
				}
				else
//#endif
				{
					needsResize = true;
					setupRenderTarget(SourceTexture->GetSizeXY(), this->FrameRate);
					// TODO: implement conversion?
				}

				if (!needsResize)
				{
					uint8_t* buffer;
					int32 Width = 0, Height = 0;

					// Map the staging surface so we can copy the buffer 
					RHICmdList.MapStagingSurface(backBufferTexture_, (void*&)buffer, Width, Height);

					// If we don't have a draw result, resize our frame
					if (FrameSize != FIntPoint(Width, Height))
					{
						// Change the render target configuration based on what the RHI determines the size to be
						setupRenderTarget(FIntPoint(Width, Height), this->FrameRate);
					}
					else
					{
						videoSource_->CaptureFrame(buffer, tc);
					}

					// unmap the staging surface
					RHICmdList.UnmapStagingSurface(backBufferTexture_);
				}
			}
		}
	}
}

// game thread
void UVideoCoreMediaSender::trySendFrame()
{
	if (videoTrackState_ == EMediaTrackState::Live && videoSource_)
	{
		videoSource_->PublishFrame();
	}
	
	// TODO: track this
	// sometimes, the callback gets cleared
	// track here https://udn.unrealengine.com/s/question/0D54z00006u2hUxCAI/onendframert-callback-gets-reset
	if (!FCoreDelegates::OnEndFrameRT.IsBoundToObject(this))
	{
		UE_LOG(LogTemp, Warning, TEXT("send frame callback got reset - media sender"));
		setupRenderThreadCallback();
	}
}
