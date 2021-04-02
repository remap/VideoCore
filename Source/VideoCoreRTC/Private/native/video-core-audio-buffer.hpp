//
// video-core-audio-buffer.hpp
//
//  Generated on April 2 2021
//  Copyright 2021 Regents of the University of California
//

#pragma once

#include <vector>
#include <deque>
#include <memory>
#include "CoreMinimal.h"

namespace videocore {
	class AudioBuffer {
	public:
		AudioBuffer(size_t nSlots, size_t defaultSlotSize);

		void enque(uint8_t* data, size_t len);
		size_t deque(uint8_t* dest, size_t length);
		void reset();

		size_t length() const { return cachedLength_; }
		size_t size() const { return buffer_.size(); }
		size_t capacity() const { return pool_.size(); }

	private:
		size_t initialPoolSize_;
		size_t defaultSlotSize_;
		size_t cachedLength_;
		typedef std::shared_ptr<std::vector<uint8_t>> AudioBufferSlot;
		std::vector<AudioBufferSlot> pool_;
		std::deque<AudioBufferSlot> buffer_;

		AudioBufferSlot reserveSlot(uint8_t* data, size_t len);
		void recycleSlot(AudioBufferSlot s);
		void reservePool(size_t nSlots);
	};
}

#pragma once
