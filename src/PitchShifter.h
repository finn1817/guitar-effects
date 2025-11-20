#ifndef PITCHSHIFTER_H
#define PITCHSHIFTER_H

#include <vector>
#include <cmath>
#include <complex>

class PitchShifter {
public:
    PitchShifter();
    
    void setSampleRate(int sampleRate);
    void process(const float* input, float* outputL, float* outputR, int numSamples, float semitones);
    
private:
    void processFrame(const float* input, float* outputL, float* outputR, int frameSize, float pitchRatio);
    void applyWindow(float* buffer, int size);
    void fft(std::complex<float>* data, int size, bool inverse);
    
    int sampleRate_{48000};
    static const int FFT_SIZE = 2048;
    static const int HOP_SIZE = 512;
    static const int OVERLAP = FFT_SIZE / HOP_SIZE;
    
    std::vector<float> inputBuffer_;
    std::vector<float> outputBufferL_;
    std::vector<float> outputBufferR_;
    std::vector<float> window_;
    
    std::vector<std::complex<float>> fftBuffer_;
    std::vector<float> lastPhase_;
    std::vector<float> sumPhase_;
    
    int inputPos_{0};
    int outputPos_{0};
    
    // Overlap-add buffers
    std::vector<float> overlapL_;
    std::vector<float> overlapR_;
};

#endif // PITCHSHIFTER_H