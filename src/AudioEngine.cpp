#define MINIAUDIO_IMPLEMENTATION
#include "AudioEngine.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif
#include "DSPChain.h"
#include "Looper.h"
#include "Recorder.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>
#include <atomic>
#include <memory>
#include <string>

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
            // Miniaudio device IDs are opaque; use index as ID string.
            info.id = std::to_string(i);
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
            info.id = std::to_string(i);
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
    // Low latency tuning
    lowLatencyMode_ = wasapiExclusive; // reuse checkbox as hint user wants lowest latency
    if (lowLatencyMode_) {
        config.performanceProfile = ma_performance_profile_low_latency;
        // Use user-selected buffer size as the period; request 2 periods.
        config.periodSizeInFrames = bufferSize;
        config.periods = 2; // Fewer periods => lower latency, higher risk of xruns.
        // Fields noPreSilencing / noClip are not present in this bundled miniaudio version; removed.
    }
    config.capture.format = ma_format_f32;
    config.capture.channels = 1; // Mono input
    config.playback.format = ma_format_f32;
    config.playback.channels = 2; // Stereo output
    config.sampleRate = sampleRate;
    if (!lowLatencyMode_) {
        // Normal path still honors user buffer size but keeps defaults for periods.
        config.periodSizeInFrames = bufferSize;
    }
    config.dataCallback = audioCallback;
    config.pUserData = this;
    
#ifdef _WIN32
    if (wasapiExclusive) {
        config.wasapi.noAutoConvertSRC = MA_TRUE;
        config.wasapi.noDefaultQualitySRC = MA_TRUE;
        config.wasapi.noHardwareOffloading = MA_TRUE;
        // Try enabling exclusive mode flag if available in header.
#ifdef MA_WASAPI_FLAG_EXCLUSIVE_MODE
        config.wasapi.flags |= MA_WASAPI_FLAG_EXCLUSIVE_MODE;
#endif
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
    
    // Preallocate buffers based on the device's period size to avoid dynamic allocations each callback.
    uint32_t allocFrames = device_.playback.internalPeriodSizeInFrames; // internal resolved size
    if (allocFrames == 0) allocFrames = bufferSize; // fallback
    inputBuffer_.resize(allocFrames);
    processedLeft_.resize(allocFrames);
    processedRight_.resize(allocFrames);
    outputMono_.resize(allocFrames);

    // Inform DSP chain of low latency mode so it can skip high-latency effects.
    dspChain_->setLowLatency(lowLatencyMode_);
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
    // Apply input gain and meter
    for (uint32_t i = 0; i < frameCount; ++i) {
        inputBuffer_[i] = input[i] * inputGain_.load();
    }
    updateMeters(inputBuffer_.data(), frameCount, inputLevel_, inputPeak_);

    // DSP processing
    dspChain_->process(inputBuffer_.data(), processedLeft_.data(), processedRight_.data(), frameCount);
    looper_->process(processedLeft_.data(), processedRight_.data(), frameCount);

    // Output gain
    float outGain = outputGain_.load();
    for (uint32_t i = 0; i < frameCount; ++i) {
        processedLeft_[i] *= outGain;
        processedRight_[i] *= outGain;
    }

    // Recorder
    recorder_->processAudio(processedLeft_.data(), processedRight_.data(), frameCount);

    // Interleave
    for (uint32_t i = 0; i < frameCount; ++i) {
        output[i * 2] = processedLeft_[i];
        output[i * 2 + 1] = processedRight_[i];
    }

    // Meter output
    for (uint32_t i = 0; i < frameCount; ++i) {
        outputMono_[i] = (processedLeft_[i] + processedRight_[i]) * 0.5f;
    }
    updateMeters(outputMono_.data(), frameCount, outputLevel_, outputPeak_);
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