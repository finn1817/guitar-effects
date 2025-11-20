#include "Looper.h"
#include <algorithm>
#include <cstring>

Looper::Looper()
{
    setSampleRate(48000);
}

void Looper::setSampleRate(int sampleRate)
{
    sampleRate_ = sampleRate;
    maxLengthSamples_ = sampleRate * MAX_LOOP_SECONDS;
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    loopBufferL_.resize(maxLengthSamples_, 0.0f);
    loopBufferR_.resize(maxLengthSamples_, 0.0f);
}

void Looper::process(float* bufferL, float* bufferR, int numSamples)
{
    LooperState state = state_.load();
    float level = loopLevel_.load();
    
    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Single-loop recording / overdub / playback (legacy primary loop)
    if (state != LooperState::Off) {
        for (int i = 0; i < numSamples; ++i) {
            if (state == LooperState::Recording) {
                if (position_ < maxLengthSamples_) {
                    loopBufferL_[position_] = bufferL[i];
                    loopBufferR_[position_] = bufferR[i];
                    position_++;
                }
            } else if (state == LooperState::Playing) {
                if (loopLength_ > 0) {
                    bufferL[i] += loopBufferL_[position_] * level;
                    bufferR[i] += loopBufferR_[position_] * level;
                    position_ = (position_ + 1) % loopLength_;
                }
            } else if (state == LooperState::Overdubbing) {
                if (loopLength_ > 0) {
                    bufferL[i] += loopBufferL_[position_] * level;
                    bufferR[i] += loopBufferR_[position_] * level;
                    loopBufferL_[position_] = loopBufferL_[position_] * 0.7f + bufferL[i] * 0.3f;
                    loopBufferR_[position_] = loopBufferR_[position_] * 0.7f + bufferR[i] * 0.3f;
                    position_ = (position_ + 1) % loopLength_;
                }
            }
        }
    }

    // Multi-slot playback/mix
    if (!slots_.empty()) {
        for (int i = 0; i < numSamples; ++i) {
            for (auto &slot : slots_) {
                if (slot.active && slot.length > 0) {
                    bufferL[i] += slot.left[slot.position] * level;
                    bufferR[i] += slot.right[slot.position] * level;
                    slot.position = (slot.position + 1) % slot.length;
                }
            }
        }
    }
}

void Looper::startRecording()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    // Clear existing loop
    std::fill(loopBufferL_.begin(), loopBufferL_.end(), 0.0f);
    std::fill(loopBufferR_.begin(), loopBufferR_.end(), 0.0f);
    
    position_ = 0;
    loopLength_ = 0;
    state_.store(LooperState::Recording);
}

void Looper::stopRecording()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    // Set loop length to recorded length
    loopLength_ = position_;
    position_ = 0;
    
    state_.store(LooperState::Off); // We'll copy into slot explicitly via addRecordedLoop()
}

void Looper::startPlaying()
{
    // Legacy single loop play
    if (loopLength_ > 0) { position_ = 0; state_.store(LooperState::Playing); }
    // Also start selected slots
    playSelectedSlots();
}

void Looper::stopPlaying()
{
    state_.store(LooperState::Off);
    position_ = 0;
    stopSlots();
}

void Looper::startOverdub()
{
    if (loopLength_ > 0) {
        state_.store(LooperState::Overdubbing);
    }
}

void Looper::stopOverdub()
{
    if (loopLength_ > 0) {
        state_.store(LooperState::Playing);
    } else {
        state_.store(LooperState::Off);
    }
}

void Looper::clear()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    std::fill(loopBufferL_.begin(), loopBufferL_.end(), 0.0f);
    std::fill(loopBufferR_.begin(), loopBufferR_.end(), 0.0f);
    
    loopLength_ = 0;
    position_ = 0;
    state_.store(LooperState::Off);
}

int Looper::addRecordedLoop()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (loopLength_ <= 0) return -1;
    LoopSlot slot;
    slot.length = loopLength_;
    slot.left.assign(loopBufferL_.begin(), loopBufferL_.begin() + loopLength_);
    slot.right.assign(loopBufferR_.begin(), loopBufferR_.begin() + loopLength_);
    slot.selected = true; // auto-select
    slots_.push_back(std::move(slot));
    return (int)slots_.size() - 1;
}

void Looper::toggleSlotSelection(int index)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    if (index < 0 || index >= (int)slots_.size()) return;
    slots_[index].selected = !slots_[index].selected;
}

void Looper::playSelectedSlots()
{
    for (auto &slot : slots_) {
        if (slot.selected) {
            slot.position = 0;
            slot.active = true;
        }
    }
}

void Looper::stopSlots()
{
    for (auto &slot : slots_) {
        slot.active = false;
        slot.position = 0;
    }
}

void Looper::clearAllSlots()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    slots_.clear();
}

bool Looper::isSlotSelected(int index) const
{
    if (index < 0 || index >= (int)slots_.size()) return false;
    return slots_[index].selected;
}

void Looper::setLoopLevel(float level)
{
    loopLevel_.store(std::max(0.0f, std::min(2.0f, level)));
}