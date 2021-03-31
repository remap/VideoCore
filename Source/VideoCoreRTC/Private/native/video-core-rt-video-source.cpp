//
// video-core-rt-video-source.cpp
//
//  Generated on March 31 2020
//  Copyright 2021 Regents of the University of California
//

#include "video-core-rt-video-source.hpp"
#include "third_party/libyuv/include/libyuv.h"
#include "common_video/include/video_frame_buffer.h"

using namespace videocore;
using namespace std;

RenderTargetVideoTrackSource::RenderTargetVideoTrackSource() :
	state_(webrtc::MediaSourceInterface::SourceState::kLive)
	, width_(1920)
	, height_(1080)
	, captureIdx_(0)
	, publishIdx_(0)
{
	setupBuffers();
}

void RenderTargetVideoTrackSource::SetFrameSize(int width, int height)
{
	if (width != width_ || height != height_)
	{
		width_ = width;
		height_ = height;
		setupBuffers();
	}
}

bool RenderTargetVideoTrackSource::CaptureFrame(uint8_t* argbData, FTimecode tc)
{
	unique_lock<mutex> lock(captureSync_, try_to_lock);

	if (lock)
	{
		libyuv::ARGBToI420(argbData,
			4 * width_,
			i420Buffer_->MutableDataY(), i420Buffer_->StrideY(),
			i420Buffer_->MutableDataU(), i420Buffer_->StrideU(),
			i420Buffer_->MutableDataV(), i420Buffer_->StrideV(),
			width_, height_);

		lastCapture_ = tc;
		captureIdx_ += 1;
	}

	return lock.owns_lock();
}

void RenderTargetVideoTrackSource::PublishFrame()
{
	lock_guard<mutex> lock(captureSync_);
	if (captureIdx_ > 0 &&
		lastPublish_ != lastCapture_)
	{
		auto n = chrono::high_resolution_clock::now().time_since_epoch();
		int64_t uts = chrono::duration_cast<chrono::microseconds>(n).count();

		frameBuilder_.set_id(publishIdx_)
			.set_timestamp_us(uts)
			.set_video_frame_buffer(i420Buffer_);

		OnFrame(frameBuilder_.build());

		lastPublish_ = lastCapture_;
		publishIdx_ += 1;
	}

}

void RenderTargetVideoTrackSource::setupBuffers()
{
	lock_guard<mutex> lock(captureSync_);
	i420Buffer_ = webrtc::I420Buffer::Create(width_, height_);
}