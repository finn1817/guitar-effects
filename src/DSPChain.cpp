#include "DSPChain.h"
#include <algorithm>
#include <cstring>

DSPChain::DSPChain()
{
    pitchShifter_ = std::make_unique<PitchShifter>();
    
    // Initialize comb filter lengths (prime numbers for better diffusion)
    const int combLengths[NUM_COMBS] = {1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116};
    for (int i = 0; i < NUM_COMBS; ++i) {
        combBuffersL_[i].resize(combLengths[i], 0.0f);
        combBuffersR_[i].resize(combLengths[i], 0.0f);
        combFeedback_[i] = 0.84f + (i * 0.01f);
    }
}

void DSPChain::setSampleRate(int sampleRate)
{
    sampleRate_ = sampleRate;
    pitchShifter_->setSampleRate(sampleRate);
    
    // Resize delay buffers (max 2 seconds)
    delayBufferL_.resize(sampleRate * 2, 0.0f);
    delayBufferR_.resize(sampleRate * 2, 0.0f);
    delayWritePos_ = 0;
}

void DSPChain::process(const float* input, float* outputL, float* outputR, int numSamples)
{
    // Copy input to working buffer
    std::vector<float> buffer(numSamples);
    std::memcpy(buffer.data(), input, numSamples * sizeof(float));
    
    // Gate
    if (!params_.gateBypass.load()) {
        processGate(buffer.data(), numSamples);
    }
    
    // Drive
    if (!params_.driveBypass.load()) {
        processDrive(buffer.data(), numSamples);
    }
    
    // EQ (process to stereo from here)
    std::memcpy(outputL, buffer.data(), numSamples * sizeof(float));
    std::memcpy(outputR, buffer.data(), numSamples * sizeof(float));
    
    if (!params_.eqBypass.load()) {
        processEQ(outputL, numSamples);
        processEQ(outputR, numSamples);
    }
    
    // Compressor
    if (!params_.compBypass.load()) {
        processCompressor(outputL, numSamples);
        processCompressor(outputR, numSamples);
    }
    
    // Pitch Shift
    if (!params_.pitchBypass.load() && params_.pitchMode.load() != 0) {
        // Create temp buffer from current output
        std::vector<float> monoTemp(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            monoTemp[i] = (outputL[i] + outputR[i]) * 0.5f;
        }
        processPitchShift(monoTemp.data(), outputL, outputR, numSamples);
    }
    
    // Delay
    if (!params_.delayBypass.load()) {
        processDelay(outputL, outputR, numSamples);
    }
    
    // Reverb
    if (!params_.reverbBypass.load()) {
        processReverb(outputL, outputR, numSamples);
    }
}

void DSPChain::processGate(float* buffer, int numSamples)
{
    float threshold = std::pow(10.0f, params_.gateThreshold.load() / 20.0f);
    float attack = 1.0f - std::exp(-1.0f / (params_.gateAttack.load() * sampleRate_));
    float release = 1.0f - std::exp(-1.0f / (params_.gateRelease.load() * sampleRate_));
    
    for (int i = 0; i < numSamples; ++i) {
        float input = std::abs(buffer[i]);
        
        if (input > threshold) {
            gateEnvelope_ += (1.0f - gateEnvelope_) * attack;
        } else {
            gateEnvelope_ += (0.0f - gateEnvelope_) * release;
        }
        
        buffer[i] *= gateEnvelope_;
    }
}

void DSPChain::processDrive(float* buffer, int numSamples)
{
    float amount = params_.driveAmount.load();
    int type = params_.driveType.load();
    float gain = 1.0f + amount * 20.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float x = buffer[i] * gain;
        
        switch (type) {
            case 0: // Soft clip
                buffer[i] = std::tanh(x);
                break;
            case 1: // Hard clip
                buffer[i] = std::max(-1.0f, std::min(1.0f, x));
                break;
            case 2: // Asymmetric
                if (x > 0) {
                    buffer[i] = std::tanh(x * 1.5f) * 0.7f;
                } else {
                    buffer[i] = std::tanh(x * 0.7f) * 1.3f;
                }
                break;
        }
        
        buffer[i] *= (1.0f / (1.0f + amount * 0.5f)); // Compensate
    }
}

void DSPChain::processEQ(float* buffer, int numSamples)
{
    // Determine if this is left or right channel based on buffer pointer
    bool isLeft = (buffer == &buffer[0]); // Simplified check
    int ch = isLeft ? 0 : 1;
    
    // Calculate coefficients
    float b0_low, b1_low, b2_low, a1_low, a2_low;
    float b0_mid, b1_mid, b2_mid, a1_mid, a2_mid;
    float b0_high, b1_high, b2_high, a1_high, a2_high;
    
    calculateBiquadCoeffs(params_.lowFreq.load(), 0.707f, params_.lowGain.load(), 
                         true, b0_low, b1_low, b2_low, a1_low, a2_low);
    calculateBiquadCoeffs(params_.midFreq.load(), params_.midQ.load(), params_.midGain.load(), 
                         false, b0_mid, b1_mid, b2_mid, a1_mid, a2_mid);
    calculateBiquadCoeffs(params_.highFreq.load(), 0.707f, params_.highGain.load(), 
                         true, b0_high, b1_high, b2_high, a1_high, a2_high);
    
    for (int i = 0; i < numSamples; ++i) {
        float sample = buffer[i];
        
        // Low shelf
        sample = processBiquad(sample, &lowZ1_[ch], &lowZ2_[ch], 
                              b0_low, b1_low, b2_low, a1_low, a2_low);
        
        // Mid peak
        sample = processBiquad(sample, &midZ1_[ch], &midZ2_[ch], 
                              b0_mid, b1_mid, b2_mid, a1_mid, a2_mid);
        
        // High shelf
        sample = processBiquad(sample, &highZ1_[ch], &highZ2_[ch], 
                              b0_high, b1_high, b2_high, a1_high, a2_high);
        
        buffer[i] = sample;
    }
}

float DSPChain::processBiquad(float input, float* z1, float* z2,
                              float b0, float b1, float b2, float a1, float a2)
{
    float output = b0 * input + *z1;
    *z1 = b1 * input - a1 * output + *z2;
    *z2 = b2 * input - a2 * output;
    return output;
}

void DSPChain::calculateBiquadCoeffs(float freq, float q, float gain, bool isShelf,
                                     float& b0, float& b1, float& b2, float& a1, float& a2)
{
    float w0 = 2.0f * M_PI * freq / sampleRate_;
    float cosw0 = std::cos(w0);
    float sinw0 = std::sin(w0);
    float A = std::pow(10.0f, gain / 40.0f);
    float alpha = sinw0 / (2.0f * q);
    
    if (isShelf) {
        float beta = std::sqrt(A) / q;
        b0 = A * ((A + 1) + (A - 1) * cosw0 + beta * sinw0);
        b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
        b2 = A * ((A + 1) + (A - 1) * cosw0 - beta * sinw0);
        float a0 = (A + 1) - (A - 1) * cosw0 + beta * sinw0;
        a1 = 2 * ((A - 1) - (A + 1) * cosw0);
        a2 = (A + 1) - (A - 1) * cosw0 - beta * sinw0;
        
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    } else {
        // Peak
        b0 = 1 + alpha * A;
        b1 = -2 * cosw0;
        b2 = 1 - alpha * A;
        float a0 = 1 + alpha / A;
        a1 = -2 * cosw0;
        a2 = 1 - alpha / A;
        
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }
}

void DSPChain::processCompressor(float* buffer, int numSamples)
{
    float threshold = std::pow(10.0f, params_.compThreshold.load() / 20.0f);
    float ratio = params_.compRatio.load();
    float attack = 1.0f - std::exp(-1.0f / (params_.compAttack.load() * sampleRate_));
    float release = 1.0f - std::exp(-1.0f / (params_.compRelease.load() * sampleRate_));
    
    for (int i = 0; i < numSamples; ++i) {
        float input = std::abs(buffer[i]);
        
        // Envelope follower
        if (input > compEnvelope_) {
            compEnvelope_ += (input - compEnvelope_) * attack;
        } else {
            compEnvelope_ += (input - compEnvelope_) * release;
        }
        
        // Compute gain reduction
        float gain = 1.0f;
        if (compEnvelope_ > threshold) {
            float excess = compEnvelope_ / threshold;
            gain = std::pow(excess, (1.0f / ratio) - 1.0f);
        }
        
        buffer[i] *= gain;
    }
}

void DSPChain::processPitchShift(const float* input, float* outputL, float* outputR, int numSamples)
{
    int mode = params_.pitchMode.load();
    float semitones = (mode == 1) ? -1.0f : 1.0f;
    pitchShifter_->process(input, outputL, outputR, numSamples, semitones);
}

void DSPChain::processDelay(float* bufferL, float* bufferR, int numSamples)
{
    float time = params_.delayTime.load();
    float feedback = params_.delayFeedback.load();
    float mix = params_.delayMix.load();
    
    int delaySamples = static_cast<int>(time * sampleRate_);
    delaySamples = std::max(1, std::min(delaySamples, static_cast<int>(delayBufferL_.size()) - 1));
    
    for (int i = 0; i < numSamples; ++i) {
        int readPos = (delayWritePos_ - delaySamples + delayBufferL_.size()) % delayBufferL_.size();
        
        float delayOutL = delayBufferL_[readPos];
        float delayOutR = delayBufferR_[readPos];
        
        // Apply feedback with high-cut
        delayFeedbackL_ = delayOutL * feedback;
        delayFeedbackR_ = delayOutR * feedback;
        
        delayBufferL_[delayWritePos_] = bufferL[i] + delayFeedbackL_;
        delayBufferR_[delayWritePos_] = bufferR[i] + delayFeedbackR_;
        
        bufferL[i] = bufferL[i] * (1.0f - mix) + delayOutL * mix;
        bufferR[i] = bufferR[i] * (1.0f - mix) + delayOutR * mix;
        
        delayWritePos_ = (delayWritePos_ + 1) % delayBufferL_.size();
    }
}

void DSPChain::processReverb(float* bufferL, float* bufferR, int numSamples)
{
    float mix = params_.reverbMix.load();
    float damping = params_.reverbDamping.load();
    
    for (int i = 0; i < numSamples; ++i) {
        float reverbL = 0.0f;
        float reverbR = 0.0f;
        
        for (int c = 0; c < NUM_COMBS; ++c) {
            int pos = combPositions_[c];
            float* combBufL = combBuffersL_[c].data();
            float* combBufR = combBuffersR_[c].data();
            int len = combBuffersL_[c].size();
            
            float outL = combBufL[pos];
            float outR = combBufR[pos];
            
            combBufL[pos] = bufferL[i] + outL * combFeedback_[c] * (1.0f - damping);
            combBufR[pos] = bufferR[i] + outR * combFeedback_[c] * (1.0f - damping);
            
            reverbL += outL;
            reverbR += outR;
            
            combPositions_[c] = (pos + 1) % len;
        }
        
        reverbL /= NUM_COMBS;
        reverbR /= NUM_COMBS;
        
        bufferL[i] = bufferL[i] * (1.0f - mix) + reverbL * mix;
        bufferR[i] = bufferR[i] * (1.0f - mix) + reverbR * mix;
    }
}