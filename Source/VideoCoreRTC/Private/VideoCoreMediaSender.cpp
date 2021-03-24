//
// VideoCoreMediaSender.cpp
//
//  Generated on March 23 2020
//  Copyright 2021 Regents of the University of California
//

#include "VideoCoreMediaSender.h"
#include "CULambdaRunnable.h"
#include "native/video-core-rtc.hpp"
#include "media/base/adapted_video_track_source.h"
#include "rtc_base/atomic_ops.h"
#include "third_party/libyuv/include/libyuv.h"
#include "common_video/include/video_frame_buffer.h"

using namespace std;
using namespace videocore;

// ***
// TODO: move to a separate file
namespace videocore
{
	class RenderTargetVideoTrackSource : public rtc::AdaptedVideoTrackSource
	{
	public:
		RenderTargetVideoTrackSource() :
			state_(webrtc::MediaSourceInterface::SourceState::kInitializing)
			, width_(1920)
			, height_(1080)
			, yBuffer_(nullptr)
			, yStride_(0)
			, uBuffer_(nullptr)
			, uStride_(0)
			, vBuffer_(nullptr)
			, vStride_(0)
			, captureIdx_(0)
			, publishIdx_(0)
		{
			setupBuffers();
		}

		void SetFrameSize(int width, int height)
		{
			if (width != width_ || height != height_)
			{
				width_ = width;
				height_ = height;
				setupBuffers();
			}
		}

		bool CaptureFrame(uint8_t* argbData, FTimecode tc)
		{
			unique_lock<mutex> lock(captureSync_, try_to_lock);

			if (lock)
			{
				libyuv::ARGBToI420(argbData,
					4 * width_, yBuffer_, yStride_,
					uBuffer_, uStride_,
					vBuffer_, vStride_,
					width_, height_);

				lastCapture_ = tc;
				captureIdx_ += 1;
			}
			
			return lock.owns_lock();
		}

		void PublishFrame()
		{
			lock_guard<mutex> lock(captureSync_);
			if (captureIdx_ > 0 &&
				lastPublish_ != lastCapture_)
			{
				auto n = chrono::high_resolution_clock::now();
				int64_t uts = chrono::duration_cast<chrono::microseconds>(n.time_since_epoch()).count();

				frameBuilder_.set_id(publishIdx_)
					.set_timestamp_us(uts)
					.set_video_frame_buffer(webrtc::WrapI420Buffer(width_, height_,
						yBuffer_, yStride_,
						uBuffer_, uStride_,
						vBuffer_, vStride_,
						rtc::Callback0<void>()));
				
				OnFrame(frameBuilder_.build());

				lastPublish_ = lastCapture_;
				publishIdx_ += 1;
			}
			
		}

		void AddRef() const override {
			rtc::AtomicOps::Increment(&ref_count_);
		}

		rtc::RefCountReleaseStatus Release() const override {
			const int count = rtc::AtomicOps::Decrement(&ref_count_);
			if (count == 0) {
				return rtc::RefCountReleaseStatus::kDroppedLastRef;
			}
			return rtc::RefCountReleaseStatus::kOtherRefsRemained;
		}

		webrtc::MediaSourceInterface::SourceState state() const override {
			return state_;
		}

		bool remote() const override {
			return false;
		}

		bool is_screencast() const override {
			return false;
		}

		absl::optional<bool> needs_denoising() const override {
			return false;
		}

		FTimecode getLastCaptureTimecode() const { return lastCapture_; }

	private:
		webrtc::VideoFrame::Builder frameBuilder_;
		uint64_t captureIdx_, publishIdx_;
		FTimecode lastPublish_, lastCapture_;
		mutex captureSync_;

		int width_, height_;
		// I420 buffer data
		uint8_t* yBuffer_, yStride_, * uBuffer_, uStride_, * vBuffer_, vStride_;

		webrtc::MediaSourceInterface::SourceState state_;
		mutable volatile int ref_count_;

		void setupBuffers()
		{
			lock_guard<mutex> lock(captureSync_);

			if (yBuffer_) free(yBuffer_);
			yBuffer_ = (uint8_t*)malloc(width_ * height_);
			yStride_ = width_;

			int halfW = width_ / 2; 
			int halfH = height_ / 2;
			
			if (uBuffer_) free(uBuffer_);
			uBuffer_ = (uint8_t*)malloc(halfW * halfH);
			uStride_ = halfW;

			if (vBuffer_) free(vBuffer_);
			vBuffer_ = (uint8_t*)malloc(halfW * halfH);
			vStride_ = halfW;
		}
	};
}
// ***

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
	videoTrack_ = getWebRtcFactory()->CreateVideoTrack(tId, videoSource_.get());

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