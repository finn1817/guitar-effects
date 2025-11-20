#include "PitchShifter.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace {
    constexpr float PI = 3.14159265358979323846f;
}

PitchShifter::PitchShifter()
{
    inputBuffer_.resize(FFT_SIZE * 2, 0.0f);
    outputBufferL_.resize(FFT_SIZE * 2, 0.0f);
    outputBufferR_.resize(FFT_SIZE * 2, 0.0f);
    window_.resize(FFT_SIZE);
    fftBuffer_.resize(FFT_SIZE);
    lastPhase_.resize(FFT_SIZE / 2 + 1, 0.0f);
    sumPhase_.resize(FFT_SIZE / 2 + 1, 0.0f);
    overlapL_.resize(FFT_SIZE, 0.0f);
    overlapR_.resize(FFT_SIZE, 0.0f);
    
    // Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * PI * i / (FFT_SIZE - 1)));
    }
}

void PitchShifter::setSampleRate(int sampleRate)
{
    sampleRate_ = sampleRate;
}

void PitchShifter::process(const float* input, float* outputL, float* outputR, 
                           int numSamples, float semitones)
{
    float pitchRatio = std::pow(2.0f, semitones / 12.0f);
    // Simple dry mix ratio to preserve body and mitigate hollow sound.
    const float dryMix = 0.15f; // retain some original
    const float wetMix = 1.0f - dryMix;
    float rmsInAccum = 0.0f;
    int rmsCount = 0;
    
    for (int i = 0; i < numSamples; ++i) {
        // Add input to buffer
        inputBuffer_[inputPos_] = input[i];
        inputPos_ = (inputPos_ + 1) % inputBuffer_.size();
        
        // Check if we have enough samples for processing
        if (inputPos_ % HOP_SIZE == 0) {
            // Extract frame
            std::vector<float> frame(FFT_SIZE);
            for (int j = 0; j < FFT_SIZE; ++j) {
                int idx = (inputPos_ - FFT_SIZE + j + inputBuffer_.size()) % inputBuffer_.size();
                frame[j] = inputBuffer_[idx];
            }
            
            // Process frame
            std::vector<float> outL(FFT_SIZE);
            std::vector<float> outR(FFT_SIZE);
            processFrame(frame.data(), outL.data(), outR.data(), FFT_SIZE, pitchRatio);
            
            // Overlap-add
            for (int j = 0; j < FFT_SIZE; ++j) {
                overlapL_[j] += outL[j];
                overlapR_[j] += outR[j];
            }
        }
        
        // Output with overlap (will normalize later)
        float wetL = overlapL_[0];
        float wetR = overlapR_[0];
        outputL[i] = wetL * wetMix + input[i] * dryMix;
        outputR[i] = wetR * wetMix + input[i] * dryMix;
        rmsInAccum += input[i] * input[i];
        rmsCount++;
        
        // Shift overlap buffer
        std::memmove(overlapL_.data(), overlapL_.data() + 1, (FFT_SIZE - 1) * sizeof(float));
        std::memmove(overlapR_.data(), overlapR_.data() + 1, (FFT_SIZE - 1) * sizeof(float));
        overlapL_[FFT_SIZE - 1] = 0.0f;
        overlapR_[FFT_SIZE - 1] = 0.0f;
    }
    // Post normalization: scale wet output so RMS roughly matches input RMS.
    if (rmsCount > 0) {
        float rmsIn = std::sqrt(rmsInAccum / rmsCount);
        float rmsOutAccum = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            rmsOutAccum += (outputL[i] + outputR[i]) * 0.5f * (outputL[i] + outputR[i]) * 0.5f;
        }
        float rmsOut = std::sqrt(rmsOutAccum / numSamples);
        if (rmsOut > 0.00001f && rmsIn > 0.00001f) {
            float scale = rmsIn / rmsOut;
            for (int i = 0; i < numSamples; ++i) {
                outputL[i] *= scale;
                outputR[i] *= scale;
            }
        }
    }
}

void PitchShifter::processFrame(const float* input, float* outputL, float* outputR, 
                                int frameSize, float pitchRatio)
{
    // Apply window
    std::vector<float> windowed(frameSize);
    for (int i = 0; i < frameSize; ++i) {
        windowed[i] = input[i] * window_[i];
    }
    
    // Copy to complex buffer
    for (int i = 0; i < frameSize; ++i) {
        fftBuffer_[i] = std::complex<float>(windowed[i], 0.0f);
    }
    
    // Forward FFT
    fft(fftBuffer_.data(), frameSize, false);
    
    // Phase vocoder processing
    std::vector<std::complex<float>> shiftedFFT(frameSize);
    const float freqPerBin = static_cast<float>(sampleRate_) / frameSize;
    const float expectedPhaseAdvance = 2.0f * PI * HOP_SIZE / frameSize;
    
    for (int i = 0; i < frameSize / 2 + 1; ++i) {
        // Get magnitude and phase
        float magnitude = std::abs(fftBuffer_[i]);
        float phase = std::arg(fftBuffer_[i]);
        
        // Calculate phase difference
        float phaseDiff = phase - lastPhase_[i];
        lastPhase_[i] = phase;
        
        // Subtract expected phase advance
        phaseDiff -= i * expectedPhaseAdvance;
        
        // Map to -PI to PI
        int qpd = static_cast<int>(phaseDiff / PI);
        if (qpd >= 0) qpd += qpd & 1;
        else qpd -= qpd & 1;
        phaseDiff -= PI * qpd;
        
        // Get deviation from bin frequency
        float deviation = phaseDiff * frameSize / (HOP_SIZE * 2.0f * PI);
        
        // Compute the true frequency
        float trueFreq = (i + deviation) * freqPerBin;
        
        // Scale frequency by pitch ratio
        float scaledFreq = trueFreq * pitchRatio;
        
        // Find target bin
        int targetBin = static_cast<int>(scaledFreq / freqPerBin + 0.5f);
        
        if (targetBin >= 0 && targetBin < frameSize / 2 + 1) {
            // Calculate accumulated phase
            sumPhase_[targetBin] += expectedPhaseAdvance * targetBin + 
                                     phaseDiff * pitchRatio;
            
            // Store in shifted FFT
            shiftedFFT[targetBin] = std::polar(magnitude, sumPhase_[targetBin]);
        }
    }
    
    // Create conjugate symmetric spectrum for real IFFT
    for (int i = frameSize / 2 + 1; i < frameSize; ++i) {
        shiftedFFT[i] = std::conj(shiftedFFT[frameSize - i]);
    }
    
    // Inverse FFT
    fft(shiftedFFT.data(), frameSize, true);
    
    // Apply window and output (stereo widening)
    for (int i = 0; i < frameSize; ++i) {
        float sample = shiftedFFT[i].real() * window_[i] * 2.0f / OVERLAP;
        outputL[i] = sample;
        outputR[i] = sample; // Could add slight delay or phase shift for stereo
    }
}

void PitchShifter::applyWindow(float* buffer, int size)
{
    for (int i = 0; i < size; ++i) {
        buffer[i] *= window_[i];
    }
}

void PitchShifter::fft(std::complex<float>* data, int size, bool inverse)
{
    // Cooley-Tukey FFT
    if (size <= 1) return;
    
    // Bit reversal
    int j = 0;
    for (int i = 0; i < size; ++i) {
        if (i < j) {
            std::swap(data[i], data[j]);
        }
        int m = size / 2;
        while (m >= 1 && j >= m) {
            j -= m;
            m /= 2;
        }
        j += m;
    }
    
    // FFT computation
    for (int s = 1; s <= std::log2(size); ++s) {
        int m = 1 << s;
        int m2 = m / 2;
        std::complex<float> w(1.0f, 0.0f);
        float angle = (inverse ? 1.0f : -1.0f) * PI / m2;
        std::complex<float> wm(std::cos(angle), std::sin(angle));
        
        for (int k = 0; k < size; k += m) {
            std::complex<float> t = w;
            for (int j = 0; j < m2; ++j) {
                std::complex<float> u = data[k + j];
                std::complex<float> v = t * data[k + j + m2];
                data[k + j] = u + v;
                data[k + j + m2] = u - v;
                t *= wm;
            }
        }
        w = wm;
    }
    
    // Scale for inverse transform
    if (inverse) {
        for (int i = 0; i < size; ++i) {
            data[i] /= static_cast<float>(size);
        }
    }
}