#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

#include "miniaudio.h" // Implementation compiled in AudioEngine.cpp

class DSPChain;
class Looper;
class Recorder;

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    bool isDefault;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    
    // Device management
    std::vector<AudioDeviceInfo> getInputDevices();
    std::vector<AudioDeviceInfo> getOutputDevices();
    
    // Engine control
    bool start(const std::string& inputDeviceId, const std::string& outputDeviceId,
               int sampleRate, int bufferSize, bool wasapiExclusive);
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Parameters
    void setInputGain(float gain);
    void setOutputGain(float gain);
    float getInputGain() const { return inputGain_.load(); }
    float getOutputGain() const { return outputGain_.load(); }
    
    // Metering
    float getInputLevel() const { return inputLevel_.load(); }
    float getOutputLevel() const { return outputLevel_.load(); }
    float getInputPeak() const { return inputPeak_.load(); }
    float getOutputPeak() const { return outputPeak_.load(); }
    void resetPeaks();
    
    // Component access
    DSPChain* getDSPChain() { return dspChain_.get(); }
    Looper* getLooper() { return looper_.get(); }
    Recorder* getRecorder() { return recorder_.get(); }
    
    // Info
    int getSampleRate() const { return sampleRate_; }
    int getBufferSize() const { return bufferSize_; }
    
private:
    static void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
    void processAudio(float* output, const float* input, uint32_t frameCount);
    void updateMeters(const float* buffer, uint32_t frameCount, 
                      std::atomic<float>& level, std::atomic<float>& peak);
    
    ma_context context_;
    ma_device device_;
    
    std::atomic<bool> running_{false};
    std::atomic<float> inputGain_{1.0f};
    std::atomic<float> outputGain_{1.0f};
    
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};
    std::atomic<float> inputPeak_{0.0f};
    std::atomic<float> outputPeak_{0.0f};
    
    int sampleRate_{48000};
    int bufferSize_{128};
    
    std::unique_ptr<DSPChain> dspChain_;
    std::unique_ptr<Looper> looper_;
    std::unique_ptr<Recorder> recorder_;
    
    // Smoothing for meters
    float inputLevelSmooth_{0.0f};
    float outputLevelSmooth_{0.0f};
};

#endif // AUDIOENGINE_H