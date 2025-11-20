#ifndef LOOPER_H
#define LOOPER_H

#include <vector>
#include <atomic>
#include <mutex>

enum class LooperState {
    Off,
    Recording,
    Playing,
    Overdubbing
};

class Looper {
public:
    Looper();
    
    void setSampleRate(int sampleRate);
    void process(float* bufferL, float* bufferR, int numSamples);
    
    // Controls
    void startRecording();
    void stopRecording();
    void startPlaying();
    void stopPlaying();
    void startOverdub();
    void stopOverdub();
    void clear();
    
    // Parameters
    void setLoopLevel(float level);
    float getLoopLevel() const { return loopLevel_.load(); }
    
    // State
    LooperState getState() const { return state_.load(); }
    int getLoopLength() const { return loopLength_; }
    int getCurrentPosition() const { return position_; }
    float getMaxLength() const { return maxLengthSeconds_; }
    
private:
    static const int MAX_LOOP_SECONDS = 60;
    
    std::atomic<LooperState> state_{LooperState::Off};
    std::atomic<float> loopLevel_{1.0f};
    
    std::vector<float> loopBufferL_;
    std::vector<float> loopBufferR_;
    
    int sampleRate_{48000};
    int maxLengthSamples_;
    int loopLength_{0};
    int position_{0};
    
    std::mutex bufferMutex_;
};

#endif // LOOPER_H