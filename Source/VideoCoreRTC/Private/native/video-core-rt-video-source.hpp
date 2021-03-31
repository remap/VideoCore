//
// video-core-rt-video-source.hpp
//
//  Generated on March 31 2020
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include "CoreMinimal.h"
#include <mutex>
#include "rtc_base/atomic_ops.h"
#include "media/base/adapted_video_track_source.h"
#include "api/video/i420_buffer.h"

namespace videocore {
	class RenderTargetVideoTrackSource : public rtc::AdaptedVideoTrackSource {
	public:
		RenderTargetVideoTrackSource();

		void SetFrameSize(int width, int height);
		bool CaptureFrame(uint8_t* argbData, FTimecode tc);
		void PublishFrame();
		void SetState(webrtc::MediaSourceInterface::SourceState s) { state_ = s; }

		// AdaptedVideoTrackSource interface override
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
		std::mutex captureSync_;

		int width_, height_;
		rtc::scoped_refptr<webrtc::I420Buffer> i420Buffer_;

		webrtc::MediaSourceInterface::SourceState state_;
		mutable volatile int ref_count_;

		void setupBuffers();
	};
}