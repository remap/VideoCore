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

using namespace videocore;

UVideoCoreMediaReceiver::UVideoCoreMediaReceiver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer) 
{
	frameBgraBuffer_ = nullptr;
	videoTexture_ = UTexture2D::CreateTransient(640, 480, EPixelFormat::PF_B8G8R8A8);
	videoTexture_->UpdateResource();
	videoTexture_->RefreshSamplerStates();
}

void UVideoCoreMediaReceiver::Init(UVideoCoreSignalingComponent* vcSiganlingComponent)
{
	vcComponent_ = vcSiganlingComponent;
	
	FCoreDelegates::OnEndFrameRT.RemoveAll(this);
	FCoreDelegates::OnEndFrameRT.AddUObject(this, &UVideoCoreMediaReceiver::captureVideoFrame);
}

void UVideoCoreMediaReceiver::Consume(FString producerId)
{
	subscribe();
}

void UVideoCoreMediaReceiver::BeginDestroy()
{
	shutdown();

	// Call the base implementation of 'BeginDestroy'
	Super::BeginDestroy();
}

// ****
void UVideoCoreMediaReceiver::subscribe()
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

			consume(recvTransport_);
		}
	});
}

void
UVideoCoreMediaReceiver::consume(mediasoupclient::RecvTransport* t)
{
	nlohmann::json caps = { {"rtpCapabilities", getDevice().GetRtpCapabilities()} };
	auto jj = fromJsonObject(caps);

	vcComponent_->getSocketIOClientComponent()->EmitNative(TEXT("consume"), fromJsonObject(caps), [this](auto response) {
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

			stream_ = getWebRtcFactory()->CreateLocalMediaStream("recv_stream");

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
}

void
UVideoCoreMediaReceiver::OnFrame(const webrtc::VideoFrame& vf)
{
	UE_LOG(LogTemp, Log, TEXT("Incoming frame %dx%d %udms %d bytes, texture %d"),
		vf.width(), vf.height(), vf.timestamp(), vf.size(),
		vf.is_texture());

	if (!videoTexture_ ||
		videoTexture_->GetSizeX() != vf.width() ||
		videoTexture_->GetSizeY() != vf.height() ||
		!frameBgraBuffer_)
	{
		initTexture(vf.width(), vf.height());
	}

	webrtc::VideoFrameBuffer::Type t = vf.video_frame_buffer()->type();
	if (t == webrtc::VideoFrameBuffer::Type::kI420)
	{
		const webrtc::I420BufferInterface *vfBuf = vf.video_frame_buffer()->GetI420();

		if (vfBuf)
		{
			FScopeLock RenderLock(&renderSyncContext_);

			//libyuv::I420ToRGBA
			libyuv::I420ToARGB
			//libyuv::I420ToBGRA
			(vfBuf->DataY(), vfBuf->StrideY(),
				vfBuf->DataU(), vfBuf->StrideU(),
				vfBuf->DataV(), vfBuf->StrideV(),
				frameBgraBuffer_, 4 * vf.width(), vf.width(), vf.height());
			hasNewFrame_ = true;
		}
	}
	else
	{
		// set error
	}
}

void
UVideoCoreMediaReceiver::captureVideoFrame()
{
	FScopeLock RenderLock(&renderSyncContext_);

	if (frameBgraBuffer_ && hasNewFrame_)
	{
		FUpdateTextureRegion2D region;
		region.SrcX = 0;
		region.SrcY = 0;
		region.DestX = 0;
		region.DestY = 0;
		region.Width = videoTexture_->GetSizeX();
		region.Height = videoTexture_->GetSizeY();

		FTexture2DResource* res = (FTexture2DResource*)videoTexture_->Resource;
		RHIUpdateTexture2D(res->GetTexture2DRHI(), 0, region, region.Width * 4, frameBgraBuffer_);

		hasNewFrame_ = false;
	}
}

void
UVideoCoreMediaReceiver::initTexture(int width, int height)
{
	FScopeLock RenderLock(&renderSyncContext_);

	/*videoTexture_ = UTexture2D::CreateTransient(width, height, EPixelFormat::PF_B8G8R8A8);*/
	
	// TODO: replace with smarter realloc-y logic
	if (frameBgraBuffer_)
		free(frameBgraBuffer_);

	bufferSize_ = width * height * 4;
	frameBgraBuffer_ = (uint8_t*) malloc(bufferSize_);
	memset(frameBgraBuffer_, 0, bufferSize_);
	hasNewFrame_ = false;
}

void
UVideoCoreMediaReceiver::shutdown()
{
	FScopeLock RenderLock(&renderSyncContext_);
	
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

	consumer_->Close();
	recvTransport_->Close();

	free(frameBgraBuffer_);
	bufferSize_ = 0;

	// TODO: notify server we're closing transport and consumer
	delete recvTransport_;
	delete consumer_;
}