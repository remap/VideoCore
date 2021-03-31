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
	, isProducingVideo_(false)
	, isProducingAudio_(false)
{

}

UVideoCoreMediaSender::~UVideoCoreMediaSender()
{

}

void UVideoCoreMediaSender::Init(UVideoCoreSignalingComponent* vcSiganlingComponent)
{
	vcComponent_ = vcSiganlingComponent;

	if (vcComponent_)
	{
		FCoreDelegates::OnEndFrameRT.RemoveAll(this);
		FCoreDelegates::OnEndFrameRT.AddUObject(this, &UVideoCoreMediaSender::tryCopyRenderTarget);

		vcComponent_->invokeWhenSendTransportReady([this](const mediasoupclient::SendTransport*) {
			createStream();
		});
	}
	// TODO: return false
}

bool UVideoCoreMediaSender::Produce(FString trackId, EMediaTrackKind trackKind)
{
	string tId = trackId.IsEmpty() ? videocore::generateUUID() : TCHAR_TO_ANSI(*trackId);

	if (stream_)
		return startStream(tId, trackKind);
	else
	{
		onMediaStreamReady_.AddLambda([this, tId, trackKind]() {
			this->startStream(tId, trackKind);
		});
	}

	return false;
}

void UVideoCoreMediaSender::Stop(EMediaTrackKind trackKind)
{
	isProducingVideo_ = false;
	// TODO: do cleanup
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
	// TODO: cleanup here
}

bool UVideoCoreMediaSender::startStream(string trackId, EMediaTrackKind trackKind)
{
	switch (trackKind) {
	case EMediaTrackKind::Audio:
	{

	}
	break;
	case EMediaTrackKind::Video:
	{
		// TODO: check already producing
		if (!isProducingVideo_)
		{
			createVideoSource();
			setupRenderTarget(FrameSize, FrameRate);
			createVideoTrack(trackId);
			createProducer();
			return true;
		}
	}
	break;
	default:;
	};

	return false;
}

void UVideoCoreMediaSender::setupRenderTarget(FIntPoint InFrameSize, FFrameRate InFrameRate)
{
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
	UE_LOG(LogTemp, Log, TEXT("Transport ready. Creating stream"));

	stream_ = getWebRtcFactory()->CreateLocalMediaStream(vcComponent_->getRecvTransport()->GetId());
	onMediaStreamReady_.Broadcast();
}

void UVideoCoreMediaSender::createVideoTrack(string tId)
{
	if (videoSource_)
	{
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
	// TODO: check source already exists

	videoSource_.reset();
	videoSource_ = make_shared<RenderTargetVideoTrackSource>();
}

void UVideoCoreMediaSender::createProducer()
{
	if (vcComponent_)
	{
		// TODO: setup layers here
		vector<webrtc::RtpEncodingParameters> encodings;
		webrtc::RtpEncodingParameters p;
		p.max_bitrate_bps = 100000;
		encodings.push_back(p);
		p.max_bitrate_bps = 300000;
		encodings.push_back(p);
		p.max_bitrate_bps = 900000;
		encodings.push_back(p);

		vcComponent_->invokeOnTransportProduce(videoTrack_->id(), 
			[this](mediasoupclient::SendTransport* t, const std::string& kind,
			nlohmann::json rtpParameters, const nlohmann::json& appData)
		{
			std::shared_ptr<std::promise<string>> promise = std::make_shared<std::promise<string>>();

			// ask server to produce
			nlohmann::json params = {
				{ "transportId", t->GetId() },
				{ "kind", kind },
				{ "rtpParameters", rtpParameters }
			};
			vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("produce"), fromJsonObject(params), [this, promise](auto response) {
				auto m = response[0]->AsObject();
				if (m->HasField("id"))
				{
					promise->set_value(TCHAR_TO_ANSI(*m->GetStringField("id")));
				}
				else
					promise->set_exception(make_exception_ptr("server:produce returned invalid response: " +
						string(TCHAR_TO_ANSI(*USIOJConvert::ToJsonString(m)))));
					
			});
			//promise->set_exception(make_exception_ptr("produce unknown track"));

			return promise->get_future();
		});

		// invoke on non-game thread because this will trigger callback 
		// with request to the server, shouldn't block game thread waiting for the reply
		FCULambdaRunnable::RunLambdaOnBackGroundThread([this, encodings]() {
			this->videoProducer_ = vcComponent_->getSendTransport()->Produce(this, videoTrack_, &encodings, nullptr,
				{ { "trackId", videoTrack_->id() } });
			isProducingVideo_ = true;
			videoTrack_->set_enabled(true);
		});	
	}
}

void UVideoCoreMediaSender::shutdown()
{
	{
		FScopeLock RenderLock(&renderSync_);

		// reset the resources
		this->renderTargetDesc_.Reset();
		this->backBufferTexture_.SafeRelease();

		this->backBufferTexture_ = nullptr;
	}
}

// render thread
void UVideoCoreMediaSender::tryCopyRenderTarget()
{
	FScopeLock RenderLock(&renderSync_);
	int64 time_code = FDateTime::Now().GetTimeOfDay().GetTicks();

	if (isProducingVideo_ &&
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
#if UE_EDITOR
				if (SourceTexture->GetSizeXY() == backBufferTexture_->GetSizeXY())
				{
					RHICmdList.CopyToResolveTarget(SourceTexture, backBufferTexture_, FResolveParams());
				}
				else
#endif
				{
					// TODO: implement conversion
					assert(0);
				}

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

// game thread
void UVideoCoreMediaSender::trySendFrame()
{
	if (isProducingVideo_ && videoSource_)
	{
		videoSource_->PublishFrame();
	}
}