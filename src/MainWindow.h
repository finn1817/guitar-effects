#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QToolButton>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTabWidget>
#include <QProgressBar>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <memory>

class AudioEngine;
class DSPChain;
class Looper;
class Recorder;
class ClipManager;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Audio I/O
    void onStartEngine();
    void onStopEngine();
    void onInputGainChanged(int value);
    void onOutputGainChanged(int value);
    void onResetPeaks();
    
    // Effects
    void onEffectBypassChanged();
    void onEffectParameterChanged();
    void updateEffectsUI();
    
    // Looper
    void onLooperRecord();
    void onLooperPlayStop();
    void onLooperOverdub();
    void onLooperClear();
    void onLooperLevelChanged(int value);
    
    // Recording
    void onStartRecording();
    void onStopRecording();
    void onDownloadRecording();
    
    // Playback
    void onClipSelected();
    void onPlayClip();
    void onPauseClip();
    void onStopClip();
    void onRenameClip();
    void onDeleteClip();
    void onRevealClip();
    void onClipVolumeChanged(int value);
    void updatePlaybackPosition();
    
    // Presets
    void onSavePreset();
    void onLoadPreset();
    void onDeletePreset();
    void refreshPresetList();
    
    // UI Updates
    void updateMeters();
    void updateLooperStatus();
    void updateRecorderStatus();

private:
    void setupUI();
    void createAudioIOPanel();
    void createMetersPanel();
    void createEffectsPanel();
    void createLooperPanel();
    void createRecorderPanel();
    void createPlaybackPanel();
    void createPresetsPanel();
    QWidget* makeCollapsible(QGroupBox* box);
    
    // Preset functions
    void savePresetToFile(const QString& name);
    void loadPresetFromFile(const QString& name);
    QString getPresetsDirectory();
    
    // Helper functions
    QString formatTime(float seconds);
    float dBToLinear(float dB);
    float linearTodB(float linear);
    
    std::unique_ptr<AudioEngine> audioEngine_;
    std::unique_ptr<ClipManager> clipManager_;
    
    QTimer* updateTimer_;
    
    // Media playback
    QMediaPlayer* mediaPlayer_;
    QAudioOutput* audioOutput_;
    
    // Audio I/O widgets
    QComboBox* inputDeviceCombo_;
    QComboBox* outputDeviceCombo_;
    QSpinBox* sampleRateSpin_;
    QSpinBox* bufferSizeSpin_;
    QCheckBox* wasapiCheck_;
    QPushButton* startButton_;
    QPushButton* stopButton_;
    QSlider* inputGainSlider_;
    QSlider* outputGainSlider_;
    QLabel* inputGainLabel_;
    QLabel* outputGainLabel_;
    
    // Meters
    QProgressBar* inputMeter_;
    QProgressBar* outputMeter_;
    QLabel* inputPeakLabel_;
    QLabel* outputPeakLabel_;
    QPushButton* resetPeaksButton_;
    
    // Effects - Gate
    QCheckBox* gateBypass_;
    QSlider* gateThreshold_;
    QLabel* gateThresholdLabel_;
    
    // Effects - Drive
    QCheckBox* driveBypass_;
    QSlider* driveAmount_;
    QComboBox* driveType_;
    QLabel* driveAmountLabel_;
    
    // Effects - EQ
    QCheckBox* eqBypass_;
    QSlider* lowGain_;
    QSlider* lowFreq_;
    QSlider* midGain_;
    QSlider* midFreq_;
    QSlider* midQ_;
    QSlider* highGain_;
    QSlider* highFreq_;
    QLabel* lowGainLabel_;
    QLabel* lowFreqLabel_;
    QLabel* midGainLabel_;
    QLabel* midFreqLabel_;
    QLabel* midQLabel_;
    QLabel* highGainLabel_;
    QLabel* highFreqLabel_;
    
    // Effects - Compressor
    QCheckBox* compBypass_;
    QSlider* compThreshold_;
    QSlider* compRatio_;
    QLabel* compThresholdLabel_;
    QLabel* compRatioLabel_;
    
    // Effects - Pitch Shift
    QCheckBox* pitchBypass_;
    QPushButton* pitchDownButton_;
    QPushButton* pitchUpButton_;
    QLabel* pitchInfoLabel_;
    
    // Effects - Delay
    QCheckBox* delayBypass_;
    QSlider* delayTime_;
    QSlider* delayFeedback_;
    QSlider* delayMix_;
    QLabel* delayTimeLabel_;
    QLabel* delayFeedbackLabel_;
    QLabel* delayMixLabel_;
    
    // Effects - Reverb
    QCheckBox* reverbBypass_;
    QSlider* reverbSize_;
    QSlider* reverbDamping_;
    QSlider* reverbMix_;
    QLabel* reverbSizeLabel_;
    QLabel* reverbDampingLabel_;
    QLabel* reverbMixLabel_;
    
    // Looper
    QPushButton* looperRecordButton_;
    QPushButton* looperPlayButton_;
    QPushButton* looperOverdubButton_;
    QPushButton* looperClearButton_;
    QSlider* looperLevelSlider_;
    QLabel* looperLevelLabel_;
    QLabel* looperStatusLabel_;
    QProgressBar* looperPositionBar_;
    
    // Recorder
    QPushButton* recordStartButton_;
    QPushButton* recordStopButton_;
    QLineEdit* recordNameEdit_;
    QPushButton* downloadButton_;
    QLabel* recordStatusLabel_;
    QLabel* recordDurationLabel_;
    
    // Playback
    QListWidget* clipList_;
    QPushButton* playButton_;
    QPushButton* pauseButton_;
    QPushButton* stopPlayButton_;
    QPushButton* renameButton_;
    QPushButton* deleteButton_;
    QPushButton* revealButton_;
    QSlider* clipVolumeSlider_;
    QLabel* clipVolumeLabel_;
    QLabel* playbackPositionLabel_;
    QProgressBar* playbackProgressBar_;
    
    // Presets
    QListWidget* presetList_;
    QLineEdit* presetNameEdit_;
    QPushButton* savePresetButton_;
    QPushButton* loadPresetButton_;
    QPushButton* deletePresetButton_;
    
    // State
    bool engineRunning_;
    bool isRecording_;
    QString currentClipName_;
    int currentPitchMode_; // 0=off, 1=down, 2=up
};

#endif // MAINWINDOW_H