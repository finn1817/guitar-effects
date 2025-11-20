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
    
    if (state == LooperState::Off) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    for (int i = 0; i < numSamples; ++i) {
        if (state == LooperState::Recording) {
            // First pass - record and set loop length
            if (position_ < maxLengthSamples_) {
                loopBufferL_[position_] = bufferL[i];
                loopBufferR_[position_] = bufferR[i];
                position_++;
            }
        }
        else if (state == LooperState::Playing) {
            // Playback only
            if (loopLength_ > 0) {
                bufferL[i] += loopBufferL_[position_] * level;
                bufferR[i] += loopBufferR_[position_] * level;
                
                position_ = (position_ + 1) % loopLength_;
            }
        }
        else if (state == LooperState::Overdubbing) {
            // Mix new audio with existing loop
            if (loopLength_ > 0) {
                // Add existing loop to output
                bufferL[i] += loopBufferL_[position_] * level;
                bufferR[i] += loopBufferR_[position_] * level;
                
                // Overdub new audio into loop (mix)
                loopBufferL_[position_] = loopBufferL_[position_] * 0.7f + bufferL[i] * 0.3f;
                loopBufferR_[position_] = loopBufferR_[position_] * 0.7f + bufferR[i] * 0.3f;
                
                position_ = (position_ + 1) % loopLength_;
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
    
    if (loopLength_ > 0) {
        state_.store(LooperState::Playing);
    } else {
        state_.store(LooperState::Off);
    }
}

void Looper::startPlaying()
{
    if (loopLength_ > 0) {
        position_ = 0;
        state_.store(LooperState::Playing);
    }
}

void Looper::stopPlaying()
{
    state_.store(LooperState::Off);
    position_ = 0;
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

void Looper::setLoopLevel(float level)
{
    loopLevel_.store(std::max(0.0f, std::min(2.0f, level)));
}