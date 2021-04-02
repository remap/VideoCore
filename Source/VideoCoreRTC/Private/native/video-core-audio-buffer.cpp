//
// video-core-audio-buffer.cpp
//
//  Generated on April 2 2021
//  Copyright 2021 Regents of the University of California
//

#include "video-core-audio-buffer.hpp"
#include <memory.h>

using namespace std;
using namespace videocore;

AudioBuffer::AudioBuffer(size_t nSlots, size_t defaultSlotSize)
	: initialPoolSize_(nSlots)
	, defaultSlotSize_(defaultSlotSize)
	, cachedLength_(0)
{
	reservePool(nSlots);
}

void AudioBuffer::enque(uint8_t* data, size_t len)
{
	buffer_.push_back(reserveSlot(data, len));
	cachedLength_ += len;

	UE_LOG(LogTemp, Log, TEXT(">>>>> ENQUEUE %d %d"), len, cachedLength_);
}

size_t AudioBuffer::deque(uint8_t* dest, size_t length)
{
	size_t nCopied = 0;

	if (buffer_.size() && length)
	{
		do
		{
			AudioBufferSlot s = buffer_.front();
			size_t nToCopy = (s->size() <= length - nCopied) ? s->size() : length - nCopied;

			memcpy(dest + nCopied, s->data(), nToCopy);
			nCopied += nToCopy;

			if (nToCopy < s->size())
				// TODO: avoid this by making slot smarter -- store read pointer
				s->erase(s->begin(), s->begin() + nToCopy);
			else
			{
				buffer_.pop_front();
				recycleSlot(s);
			}
		} while (nCopied != length && buffer_.size());

		cachedLength_ -= nCopied;
	}

	UE_LOG(LogTemp, Log, TEXT("<<<<<    DEQUEUE %d %d %d"), length, nCopied, cachedLength_);
	return nCopied;
}

void AudioBuffer::reset()
{
	for (auto s : buffer_)
		recycleSlot(s);

	buffer_.clear();
	cachedLength_ = 0;
}

AudioBuffer::AudioBufferSlot AudioBuffer::reserveSlot(uint8_t* data, size_t len)
{
	if (!pool_.size()) // TODO: is this a bad idea?
		reservePool(initialPoolSize_);

	AudioBufferSlot s = pool_.back();
	pool_.pop_back();

	s->resize(len);
	memcpy(s->data(), data, len);

	return s;
}

void AudioBuffer::recycleSlot(AudioBuffer::AudioBufferSlot slot)
{
	pool_.push_back(slot);
}

void AudioBuffer::reservePool(size_t nSlots)
{
	for (int i = 0; i < nSlots; ++i)
		pool_.push_back(make_shared<vector<uint8_t>>(defaultSlotSize_, 0));
}