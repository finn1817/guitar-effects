#include "AudioEngine.h"
#include "DSPChain.h"
#include "Looper.h"
#include "Recorder.h"
#include <cmath>
#include <algorithm>
#include <cstring>

AudioEngine::AudioEngine()
{
    ma_context_config contextConfig = ma_context_config_init();
    ma_result result = ma_context_init(nullptr, 0, &contextConfig, &context_);
    if (result != MA_SUCCESS) {
        // Handle error
    }
    
    dspChain_ = std::make_unique<DSPChain>();
    looper_ = std::make_unique<Looper>();
    recorder_ = std::make_unique<Recorder>();
}

AudioEngine::~AudioEngine()
{
    stop();
    ma_context_uninit(&context_);
}

std::vector<AudioDeviceInfo> AudioEngine::getInputDevices()
{
    std::vector<AudioDeviceInfo> devices;
    
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    
    ma_result result = ma_context_get_devices(&context_, &pPlaybackInfos, &playbackCount,
                                               &pCaptureInfos, &captureCount);
    
    if (result == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < captureCount; ++i) {
            AudioDeviceInfo info;
            info.id = std::string((char*)&pCaptureInfos[i].id, sizeof(pCaptureInfos[i].id));
            info.name = pCaptureInfos[i].name;
            info.isDefault = pCaptureInfos[i].isDefault;
            devices.push_back(info);
        }
    }
    
    return devices;
}

std::vector<AudioDeviceInfo> AudioEngine::getOutputDevices()
{
    std::vector<AudioDeviceInfo> devices;
    
    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    
    ma_result result = ma_context_get_devices(&context_, &pPlaybackInfos, &playbackCount,
                                               &pCaptureInfos, &captureCount);
    
    if (result == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            AudioDeviceInfo info;
            info.id = std::string((char*)&pPlaybackInfos[i].id, sizeof(pPlaybackInfos[i].id));
            info.name = pPlaybackInfos[i].name;
            info.isDefault = pPlaybackInfos[i].isDefault;
            devices.push_back(info);
        }
    }
    
    return devices;
}

bool AudioEngine::start(const std::string& inputDeviceId, const std::string& outputDeviceId,
                        int sampleRate, int bufferSize, bool wasapiExclusive)
{
    if (running_) {
        stop();
    }
    
    sampleRate_ = sampleRate;
    bufferSize_ = bufferSize;
    
    // Initialize components
    dspChain_->setSampleRate(sampleRate);
    looper_->setSampleRate(sampleRate);
    recorder_->setSampleRate(sampleRate);
    
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1; // Mono input
    config.playback.format = ma_format_f32;
    config.playback.channels = 2; // Stereo output
    config.sampleRate = sampleRate;
    config.periodSizeInFrames = bufferSize;
    config.dataCallback = audioCallback;
    config.pUserData = this;
    
#ifdef _WIN32
    if (wasapiExclusive) {
        config.wasapi.noAutoConvertSRC = MA_TRUE;
        config.wasapi.noDefaultQualitySRC = MA_TRUE;
        config.wasapi.noHardwareOffloading = MA_TRUE;
        config.shareMode = ma_share_mode_exclusive;
    }
#endif
    
    ma_result result = ma_device_init(&context_, &config, &device_);
    if (result != MA_SUCCESS) {
        return false;
    }
    
    result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&device_);
        return false;
    }
    
    running_.store(true);
    return true;
}

void AudioEngine::stop()
{
    if (!running_) return;
    
    running_.store(false);
    ma_device_stop(&device_);
    ma_device_uninit(&device_);
    
    recorder_->stopRecording();
}

void AudioEngine::audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    AudioEngine* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    engine->processAudio(static_cast<float*>(pOutput), static_cast<const float*>(pInput), frameCount);
}

void AudioEngine::processAudio(float* output, const float* input, uint32_t frameCount)
{
    // Clear output buffer
    std::memset(output, 0, frameCount * 2 * sizeof(float));
    
    // Apply input gain and meter
    std::vector<float> inputBuffer(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        inputBuffer[i] = input[i] * inputGain_.load();
    }
    updateMeters(inputBuffer.data(), frameCount, inputLevel_, inputPeak_);
    
    // Process through DSP chain
    std::vector<float> processedLeft(frameCount);
    std::vector<float> processedRight(frameCount);
    dspChain_->process(inputBuffer.data(), processedLeft.data(), processedRight.data(), frameCount);
    
    // Mix in looper
    looper_->process(processedLeft.data(), processedRight.data(), frameCount);
    
    // Apply output gain
    float outGain = outputGain_.load();
    for (uint32_t i = 0; i < frameCount; ++i) {
        processedLeft[i] *= outGain;
        processedRight[i] *= outGain;
    }
    
    // Send to recorder
    recorder_->processAudio(processedLeft.data(), processedRight.data(), frameCount);
    
    // Interleave to output
    for (uint32_t i = 0; i < frameCount; ++i) {
        output[i * 2] = processedLeft[i];
        output[i * 2 + 1] = processedRight[i];
    }
    
    // Meter output (average L+R)
    std::vector<float> outputMono(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        outputMono[i] = (processedLeft[i] + processedRight[i]) * 0.5f;
    }
    updateMeters(outputMono.data(), frameCount, outputLevel_, outputPeak_);
}

void AudioEngine::updateMeters(const float* buffer, uint32_t frameCount,
                                std::atomic<float>& level, std::atomic<float>& peak)
{
    float sum = 0.0f;
    float maxVal = 0.0f;
    
    for (uint32_t i = 0; i < frameCount; ++i) {
        float absVal = std::abs(buffer[i]);
        sum += absVal * absVal;
        maxVal = std::max(maxVal, absVal);
    }
    
    float rms = std::sqrt(sum / frameCount);
    level.store(rms);
    
    float currentPeak = peak.load();
    if (maxVal > currentPeak) {
        peak.store(maxVal);
    }
}

void AudioEngine::resetPeaks()
{
    inputPeak_.store(0.0f);
    outputPeak_.store(0.0f);
}

void AudioEngine::setInputGain(float gain)
{
    inputGain_.store(std::max(0.0f, std::min(2.0f, gain)));
}

void AudioEngine::setOutputGain(float gain)
{
    outputGain_.store(std::max(0.0f, std::min(2.0f, gain)));
}