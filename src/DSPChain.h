#ifndef DSPCHAIN_H
#define DSPCHAIN_H

#include <atomic>
#include <vector>
#include <cmath>
#include "PitchShifter.h"

struct DSPParams {
    // Gate
    std::atomic<bool> gateBypass{true};
    std::atomic<float> gateThreshold{-60.0f};
    std::atomic<float> gateAttack{0.001f};
    std::atomic<float> gateRelease{0.05f};
    
    // Drive
    std::atomic<bool> driveBypass{true};
    std::atomic<float> driveAmount{0.5f};
    std::atomic<int> driveType{0}; // 0=soft, 1=hard, 2=asym
    
    // EQ
    std::atomic<bool> eqBypass{true};
    std::atomic<float> lowGain{0.0f};
    std::atomic<float> lowFreq{100.0f};
    std::atomic<float> midGain{0.0f};
    std::atomic<float> midFreq{1000.0f};
    std::atomic<float> midQ{1.0f};
    std::atomic<float> highGain{0.0f};
    std::atomic<float> highFreq{8000.0f};
    
    // Compressor
    std::atomic<bool> compBypass{true};
    std::atomic<float> compThreshold{-20.0f};
    std::atomic<float> compRatio{4.0f};
    std::atomic<float> compAttack{0.005f};
    std::atomic<float> compRelease{0.1f};
    
    // Pitch Shift
    std::atomic<bool> pitchBypass{true};
    std::atomic<int> pitchMode{0}; // 0=off, 1=down, 2=up
    
    // Delay
    std::atomic<bool> delayBypass{true};
    std::atomic<float> delayTime{0.25f};
    std::atomic<float> delayFeedback{0.3f};
    std::atomic<float> delayMix{0.3f};
    std::atomic<float> delayHighCut{5000.0f};
    
    // Reverb
    std::atomic<bool> reverbBypass{true};
    std::atomic<float> reverbSize{0.5f};
    std::atomic<float> reverbDamping{0.5f};
    std::atomic<float> reverbMix{0.25f};
};

class DSPChain {
public:
    DSPChain();
    
    void setSampleRate(int sampleRate);
    void process(const float* input, float* outputL, float* outputR, int numSamples);
    
    DSPParams& getParams() { return params_; }
    
private:
    void processGate(float* buffer, int numSamples);
    void processDrive(float* buffer, int numSamples);
    void processEQ(float* buffer, int numSamples);
    void processCompressor(float* buffer, int numSamples);
    void processPitchShift(const float* input, float* outputL, float* outputR, int numSamples);
    void processDelay(float* bufferL, float* bufferR, int numSamples);
    void processReverb(float* bufferL, float* bufferR, int numSamples);
    
    float processBiquad(float input, float* z1, float* z2, 
                       float b0, float b1, float b2, float a1, float a2);
    void calculateBiquadCoeffs(float freq, float q, float gain, bool isShelf,
                              float& b0, float& b1, float& b2, float& a1, float& a2);
    
    DSPParams params_;
    int sampleRate_{48000};
    
    // Gate state
    float gateEnvelope_{0.0f};
    
    // EQ state (separate for L/R)
    float lowZ1_[2]{}, lowZ2_[2]{};
    float midZ1_[2]{}, midZ2_[2]{};
    float highZ1_[2]{}, highZ2_[2]{};
    
    // Compressor state
    float compEnvelope_{0.0f};
    
    // Pitch shifter
    std::unique_ptr<PitchShifter> pitchShifter_;
    
    // Delay buffers
    std::vector<float> delayBufferL_;
    std::vector<float> delayBufferR_;
    int delayWritePos_{0};
    float delayFeedbackL_{0.0f};
    float delayFeedbackR_{0.0f};
    
    // Reverb (simple comb filters)
    static const int NUM_COMBS = 8;
    std::vector<float> combBuffersL_[NUM_COMBS];
    std::vector<float> combBuffersR_[NUM_COMBS];
    int combPositions_[NUM_COMBS]{};
    float combFeedback_[NUM_COMBS];
};

#endif // DSPCHAIN_H