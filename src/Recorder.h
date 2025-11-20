#ifndef RECORDER_H
#define RECORDER_H

#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <string>

class Recorder {
public:
    Recorder();
    ~Recorder();
    
    void setSampleRate(int sampleRate);
    void processAudio(const float* bufferL, const float* bufferR, int numSamples);
    
    // Recording control
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recording_.load(); }
    
    // File management
    bool saveToFile(const std::string& filepath);
    void setAutoSavePath(const std::string& path) { autoSavePath_ = path; }
    
    // Status
    float getRecordingDuration() const;
    bool hasRecordedAudio() const { return recordedFrames_ > 0; }
    void clearRecording();
    
private:
    void writeWavFile(const std::string& filepath);
    void writeThread();
    
    std::atomic<bool> recording_{false};
    std::atomic<bool> stopWriteThread_{false};
    
    std::vector<float> recordBufferL_;
    std::vector<float> recordBufferR_;
    std::mutex bufferMutex_;
    
    int sampleRate_{48000};
    size_t recordedFrames_{0};
    size_t maxRecordFrames_;
    
    std::string autoSavePath_;
    
    // Ring buffer for thread-safe recording
    static const int RING_BUFFER_SIZE = 48000 * 10; // 10 seconds buffer
    std::vector<float> ringBufferL_;
    std::vector<float> ringBufferR_;
    int ringWritePos_{0};
    int ringReadPos_{0};
    
    std::thread writeThread_;
    std::condition_variable writeCV_;
    std::mutex writeMutex_;
};

#endif // RECORDER_H