#include "Recorder.h"
#include <fstream>
#include <cstring>
#include <algorithm>

Recorder::Recorder()
{
    maxRecordFrames_ = 48000 * 600; // 10 minutes max
    recordBufferL_.reserve(maxRecordFrames_);
    recordBufferR_.reserve(maxRecordFrames_);
    
    ringBufferL_.resize(RING_BUFFER_SIZE, 0.0f);
    ringBufferR_.resize(RING_BUFFER_SIZE, 0.0f);
    
    // Start write thread
    stopWriteThread_.store(false);
    writeThread_ = std::thread(&Recorder::writeThread, this);
}

Recorder::~Recorder()
{
    stopRecording();
    stopWriteThread_.store(true);
    writeCV_.notify_one();
    if (writeThread_.joinable()) {
        writeThread_.join();
    }
}

void Recorder::setSampleRate(int sampleRate)
{
    sampleRate_ = sampleRate;
    maxRecordFrames_ = sampleRate * 600;
    recordBufferL_.reserve(maxRecordFrames_);
    recordBufferR_.reserve(maxRecordFrames_);
}

void Recorder::processAudio(const float* bufferL, const float* bufferR, int numSamples)
{
    if (!recording_.load()) {
        return;
    }
    
    // Write to ring buffer
    for (int i = 0; i < numSamples; ++i) {
        ringBufferL_[ringWritePos_] = bufferL[i];
        ringBufferR_[ringWritePos_] = bufferR[i];
        
        ringWritePos_ = (ringWritePos_ + 1) % RING_BUFFER_SIZE;
        
        // Check for overflow
        if (ringWritePos_ == ringReadPos_) {
            // Buffer full, advance read position (drop oldest)
            ringReadPos_ = (ringReadPos_ + 1) % RING_BUFFER_SIZE;
        }
    }
    
    writeCV_.notify_one();
}

void Recorder::writeThread()
{
    while (!stopWriteThread_.load()) {
        std::unique_lock<std::mutex> lock(writeMutex_);
        writeCV_.wait(lock, [this] { 
            return stopWriteThread_.load() || ringReadPos_ != ringWritePos_; 
        });
        
        if (stopWriteThread_.load()) {
            break;
        }
        
        // Copy from ring buffer to main buffer
        std::lock_guard<std::mutex> bufferLock(bufferMutex_);
        
        while (ringReadPos_ != ringWritePos_ && recordedFrames_ < maxRecordFrames_) {
            recordBufferL_.push_back(ringBufferL_[ringReadPos_]);
            recordBufferR_.push_back(ringBufferR_[ringReadPos_]);
            
            ringReadPos_ = (ringReadPos_ + 1) % RING_BUFFER_SIZE;
            recordedFrames_++;
        }
    }
}

void Recorder::startRecording()
{
    clearRecording();
    recording_.store(true);
}

void Recorder::stopRecording()
{
    recording_.store(false);
}

void Recorder::clearRecording()
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    recordBufferL_.clear();
    recordBufferR_.clear();
    recordedFrames_ = 0;
    ringReadPos_ = ringWritePos_;
}

float Recorder::getRecordingDuration() const
{
    return static_cast<float>(recordedFrames_) / sampleRate_;
}

bool Recorder::saveToFile(const std::string& filepath)
{
    std::lock_guard<std::mutex> lock(bufferMutex_);
    
    if (recordedFrames_ == 0) {
        return false;
    }
    
    writeWavFile(filepath);
    return true;
}

void Recorder::writeWavFile(const std::string& filepath)
{
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return;
    }
    
    // WAV header
    const int numChannels = 2;
    const int bitsPerSample = 24;
    const int bytesPerSample = bitsPerSample / 8;
    const int byteRate = sampleRate_ * numChannels * bytesPerSample;
    const int blockAlign = numChannels * bytesPerSample;
    const int dataSize = recordedFrames_ * numChannels * bytesPerSample;
    
    // RIFF header
    file.write("RIFF", 4);
    int chunkSize = 36 + dataSize;
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);
    
    // fmt chunk
    file.write("fmt ", 4);
    int fmtSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    short audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    short channels = numChannels;
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate_), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    short blockAlignShort = blockAlign;
    file.write(reinterpret_cast<const char*>(&blockAlignShort), 2);
    short bps = bitsPerSample;
    file.write(reinterpret_cast<const char*>(&bps), 2);
    
    // data chunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    
    // Write interleaved samples
    for (size_t i = 0; i < recordedFrames_; ++i) {
        // Convert float to 24-bit PCM
        auto floatTo24bit = [](float sample) -> int {
            sample = std::max(-1.0f, std::min(1.0f, sample));
            return static_cast<int>(sample * 8388607.0f); // 2^23 - 1
        };
        
        int sampleL = floatTo24bit(recordBufferL_[i]);
        int sampleR = floatTo24bit(recordBufferR_[i]);
        
        // Write 24-bit samples (little-endian)
        char bytes[3];
        
        bytes[0] = sampleL & 0xFF;
        bytes[1] = (sampleL >> 8) & 0xFF;
        bytes[2] = (sampleL >> 16) & 0xFF;
        file.write(bytes, 3);
        
        bytes[0] = sampleR & 0xFF;
        bytes[1] = (sampleR >> 8) & 0xFF;
        bytes[2] = (sampleR >> 16) & 0xFF;
        file.write(bytes, 3);
    }
    
    file.close();
}