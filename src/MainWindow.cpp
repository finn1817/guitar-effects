#include "MainWindow.h"
#include "AudioEngine.h"
#include "DSPChain.h"
#include "Looper.h"
#include "Recorder.h"
#include "ClipManager.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , engineRunning_(false)
    , isRecording_(false)
    , currentPitchMode_(0)
{
    audioEngine_ = std::make_unique<AudioEngine>();
    clipManager_ = std::make_unique<ClipManager>();
    
    mediaPlayer_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    mediaPlayer_->setAudioOutput(audioOutput_);
    
    setupUI();
    
    updateTimer_ = new QTimer(this);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::updateMeters);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::updateLooperStatus);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::updateRecorderStatus);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::updatePlaybackPosition);
    updateTimer_->start(33); // ~30 Hz
    
    connect(mediaPlayer_, &QMediaPlayer::positionChanged, this, &MainWindow::updatePlaybackPosition);
    
    setWindowTitle("Guitar Effects App v1.0");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    if (engineRunning_) {
        audioEngine_->stop();
    }
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Top section - Audio I/O and Meters
    QHBoxLayout* topLayout = new QHBoxLayout();
    createAudioIOPanel();
    createMetersPanel();
    topLayout->addWidget(makeCollapsible(findChild<QGroupBox*>("audioIOPanel")));
    topLayout->addWidget(makeCollapsible(findChild<QGroupBox*>("metersPanel")));
    mainLayout->addLayout(topLayout);
    
    // Middle section - Effects tabs
    createEffectsPanel();
    mainLayout->addWidget(findChild<QTabWidget*>("effectsTab"));
    
    // Bottom section - Looper, Recorder, Playback, Presets
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    
    QVBoxLayout* leftColumn = new QVBoxLayout();
    createLooperPanel();
    createRecorderPanel();
    leftColumn->addWidget(makeCollapsible(findChild<QGroupBox*>("looperPanel")));
    leftColumn->addWidget(makeCollapsible(findChild<QGroupBox*>("recorderPanel")));
    
    QVBoxLayout* rightColumn = new QVBoxLayout();
    createPlaybackPanel();
    createPresetsPanel();
    rightColumn->addWidget(makeCollapsible(findChild<QGroupBox*>("playbackPanel")));
    rightColumn->addWidget(makeCollapsible(findChild<QGroupBox*>("presetsPanel")));
    
    bottomLayout->addLayout(leftColumn);
    bottomLayout->addLayout(rightColumn);
    mainLayout->addLayout(bottomLayout);
}

QWidget* MainWindow::makeCollapsible(QGroupBox* box)
{
    if (!box) return nullptr;
    QWidget* container = new QWidget();
    QVBoxLayout* v = new QVBoxLayout(container);
    v->setContentsMargins(0,0,0,0);
    QToolButton* btn = new QToolButton(container);
    btn->setText(box->title());
    btn->setCheckable(true);
    btn->setChecked(true);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setArrowType(Qt::DownArrow);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setStyleSheet("QToolButton { font-weight: bold; background: #e6e6e6; padding:6px; border:1px solid #c0c0c0; border-radius:4px; }"
                      "QToolButton:pressed { background: #d0d0d0; }");
    box->setTitle("");
    v->addWidget(btn);
    v->addWidget(box);
    connect(btn, &QToolButton::toggled, this, [box, btn](bool on){
        box->setVisible(on);
        btn->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });
    return container;
}

void MainWindow::createAudioIOPanel()
{
    QGroupBox* panel = new QGroupBox("Audio I/O", this);
    panel->setObjectName("audioIOPanel");
    QGridLayout* layout = new QGridLayout(panel);
    
    // Device selection
    layout->addWidget(new QLabel("Input Device:"), 0, 0);
    inputDeviceCombo_ = new QComboBox();
    layout->addWidget(inputDeviceCombo_, 0, 1, 1, 2);
    
    layout->addWidget(new QLabel("Output Device:"), 1, 0);
    outputDeviceCombo_ = new QComboBox();
    layout->addWidget(outputDeviceCombo_, 1, 1, 1, 2);
    
    // Sample rate and buffer size
    layout->addWidget(new QLabel("Sample Rate:"), 2, 0);
    sampleRateSpin_ = new QSpinBox();
    sampleRateSpin_->setRange(44100, 192000);
    sampleRateSpin_->setValue(48000);
    sampleRateSpin_->setSingleStep(48000);
    layout->addWidget(sampleRateSpin_, 2, 1);
    
    layout->addWidget(new QLabel("Buffer Size:"), 2, 2);
    bufferSizeSpin_ = new QSpinBox();
    bufferSizeSpin_->setRange(32, 2048);
    bufferSizeSpin_->setValue(128);
    bufferSizeSpin_->setSingleStep(32);
    layout->addWidget(bufferSizeSpin_, 2, 3);
    
    // WASAPI exclusive mode (Windows only)
#ifdef Q_OS_WIN
    wasapiCheck_ = new QCheckBox("WASAPI Exclusive Mode");
    layout->addWidget(wasapiCheck_, 3, 0, 1, 2);
#else
    wasapiCheck_ = new QCheckBox("WASAPI Exclusive Mode");
    wasapiCheck_->setVisible(false);
#endif
    
    // Start/Stop buttons
    startButton_ = new QPushButton("Start Engine");
    stopButton_ = new QPushButton("Stop Engine");
    stopButton_->setEnabled(false);
    layout->addWidget(startButton_, 4, 0, 1, 2);
    layout->addWidget(stopButton_, 4, 2, 1, 2);
    
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::onStartEngine);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopEngine);
    
    // Gain controls
    layout->addWidget(new QLabel("Input Gain:"), 5, 0);
    inputGainSlider_ = new QSlider(Qt::Horizontal);
    inputGainSlider_->setRange(0, 200);
    inputGainSlider_->setValue(100);
    layout->addWidget(inputGainSlider_, 5, 1, 1, 2);
    inputGainLabel_ = new QLabel("0.0 dB");
    layout->addWidget(inputGainLabel_, 5, 3);
    
    layout->addWidget(new QLabel("Output Gain:"), 6, 0);
    outputGainSlider_ = new QSlider(Qt::Horizontal);
    outputGainSlider_->setRange(0, 200);
    outputGainSlider_->setValue(100);
    layout->addWidget(outputGainSlider_, 6, 1, 1, 2);
    outputGainLabel_ = new QLabel("0.0 dB");
    layout->addWidget(outputGainLabel_, 6, 3);
    
    connect(inputGainSlider_, &QSlider::valueChanged, this, &MainWindow::onInputGainChanged);
    connect(outputGainSlider_, &QSlider::valueChanged, this, &MainWindow::onOutputGainChanged);
    
    // Populate device lists
    auto inputDevices = audioEngine_->getInputDevices();
    for (const auto& dev : inputDevices) {
        inputDeviceCombo_->addItem(QString::fromStdString(dev.name), 
                                   QString::fromStdString(dev.id));
        if (dev.isDefault) {
            inputDeviceCombo_->setCurrentIndex(inputDeviceCombo_->count() - 1);
        }
    }
    
    auto outputDevices = audioEngine_->getOutputDevices();
    for (const auto& dev : outputDevices) {
        outputDeviceCombo_->addItem(QString::fromStdString(dev.name), 
                                    QString::fromStdString(dev.id));
        if (dev.isDefault) {
            outputDeviceCombo_->setCurrentIndex(outputDeviceCombo_->count() - 1);
        }
    }
}

void MainWindow::createMetersPanel()
{
    QGroupBox* panel = new QGroupBox("Meters", this);
    panel->setObjectName("metersPanel");
    QGridLayout* layout = new QGridLayout(panel);
    
    layout->addWidget(new QLabel("Input:"), 0, 0);
    inputMeter_ = new QProgressBar();
    inputMeter_->setRange(0, 100);
    inputMeter_->setTextVisible(false);
    inputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #00ff00; }");
    layout->addWidget(inputMeter_, 0, 1);
    
    inputPeakLabel_ = new QLabel("Peak: -∞ dB");
    layout->addWidget(inputPeakLabel_, 0, 2);
    
    layout->addWidget(new QLabel("Output:"), 1, 0);
    outputMeter_ = new QProgressBar();
    outputMeter_->setRange(0, 100);
    outputMeter_->setTextVisible(false);
    outputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #00ff00; }");
    layout->addWidget(outputMeter_, 1, 1);
    
    outputPeakLabel_ = new QLabel("Peak: -∞ dB");
    layout->addWidget(outputPeakLabel_, 1, 2);
    
    resetPeaksButton_ = new QPushButton("Reset Peaks");
    layout->addWidget(resetPeaksButton_, 2, 0, 1, 3);
    connect(resetPeaksButton_, &QPushButton::clicked, this, &MainWindow::onResetPeaks);
}

void MainWindow::createEffectsPanel()
{
    QTabWidget* effectsTab = new QTabWidget(this);
    effectsTab->setObjectName("effectsTab");
    
    // Gate Tab
    QWidget* gateWidget = new QWidget();
    QVBoxLayout* gateLayout = new QVBoxLayout(gateWidget);
    gateBypass_ = new QCheckBox("Bypass");
    gateBypass_->setChecked(true);
    gateLayout->addWidget(gateBypass_);
    
    QGridLayout* gateGrid = new QGridLayout();
    gateGrid->addWidget(new QLabel("Threshold:"), 0, 0);
    gateThreshold_ = new QSlider(Qt::Horizontal);
    gateThreshold_->setRange(-80, 0);
    gateThreshold_->setValue(-60);
    gateGrid->addWidget(gateThreshold_, 0, 1);
    gateThresholdLabel_ = new QLabel("-60 dB");
    gateGrid->addWidget(gateThresholdLabel_, 0, 2);
    gateLayout->addLayout(gateGrid);
    gateLayout->addStretch();
    
    connect(gateBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(gateThreshold_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(gateWidget, "Gate");
    
    // Drive Tab
    QWidget* driveWidget = new QWidget();
    QVBoxLayout* driveLayout = new QVBoxLayout(driveWidget);
    driveBypass_ = new QCheckBox("Bypass");
    driveBypass_->setChecked(true);
    driveLayout->addWidget(driveBypass_);
    
    QGridLayout* driveGrid = new QGridLayout();
    driveGrid->addWidget(new QLabel("Amount:"), 0, 0);
    driveAmount_ = new QSlider(Qt::Horizontal);
    driveAmount_->setRange(0, 100);
    driveAmount_->setValue(50);
    driveGrid->addWidget(driveAmount_, 0, 1);
    driveAmountLabel_ = new QLabel("50%");
    driveGrid->addWidget(driveAmountLabel_, 0, 2);
    
    driveGrid->addWidget(new QLabel("Type:"), 1, 0);
    driveType_ = new QComboBox();
    driveType_->addItem("Soft Clip");
    driveType_->addItem("Hard Clip");
    driveType_->addItem("Asymmetric");
    driveGrid->addWidget(driveType_, 1, 1, 1, 2);
    
    driveLayout->addLayout(driveGrid);
    driveLayout->addStretch();
    
    connect(driveBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(driveAmount_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(driveType_, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(driveWidget, "Drive");
    
    // EQ Tab
    QWidget* eqWidget = new QWidget();
    QVBoxLayout* eqLayout = new QVBoxLayout(eqWidget);
    eqBypass_ = new QCheckBox("Bypass");
    eqBypass_->setChecked(true);
    eqLayout->addWidget(eqBypass_);
    
    QGridLayout* eqGrid = new QGridLayout();
    
    // Low shelf
    eqGrid->addWidget(new QLabel("Low Gain:"), 0, 0);
    lowGain_ = new QSlider(Qt::Horizontal);
    lowGain_->setRange(-12, 12);
    lowGain_->setValue(0);
    eqGrid->addWidget(lowGain_, 0, 1);
    lowGainLabel_ = new QLabel("0 dB");
    eqGrid->addWidget(lowGainLabel_, 0, 2);
    
    eqGrid->addWidget(new QLabel("Low Freq:"), 1, 0);
    lowFreq_ = new QSlider(Qt::Horizontal);
    lowFreq_->setRange(20, 500);
    lowFreq_->setValue(100);
    eqGrid->addWidget(lowFreq_, 1, 1);
    lowFreqLabel_ = new QLabel("100 Hz");
    eqGrid->addWidget(lowFreqLabel_, 1, 2);
    
    // Mid peak
    eqGrid->addWidget(new QLabel("Mid Gain:"), 2, 0);
    midGain_ = new QSlider(Qt::Horizontal);
    midGain_->setRange(-12, 12);
    midGain_->setValue(0);
    eqGrid->addWidget(midGain_, 2, 1);
    midGainLabel_ = new QLabel("0 dB");
    eqGrid->addWidget(midGainLabel_, 2, 2);
    
    eqGrid->addWidget(new QLabel("Mid Freq:"), 3, 0);
    midFreq_ = new QSlider(Qt::Horizontal);
    midFreq_->setRange(200, 5000);
    midFreq_->setValue(1000);
    eqGrid->addWidget(midFreq_, 3, 1);
    midFreqLabel_ = new QLabel("1000 Hz");
    eqGrid->addWidget(midFreqLabel_, 3, 2);
    
    eqGrid->addWidget(new QLabel("Mid Q:"), 4, 0);
    midQ_ = new QSlider(Qt::Horizontal);
    midQ_->setRange(5, 50);
    midQ_->setValue(10);
    eqGrid->addWidget(midQ_, 4, 1);
    midQLabel_ = new QLabel("1.0");
    eqGrid->addWidget(midQLabel_, 4, 2);
    
    // High shelf
    eqGrid->addWidget(new QLabel("High Gain:"), 5, 0);
    highGain_ = new QSlider(Qt::Horizontal);
    highGain_->setRange(-12, 12);
    highGain_->setValue(0);
    eqGrid->addWidget(highGain_, 5, 1);
    highGainLabel_ = new QLabel("0 dB");
    eqGrid->addWidget(highGainLabel_, 5, 2);
    
    eqGrid->addWidget(new QLabel("High Freq:"), 6, 0);
    highFreq_ = new QSlider(Qt::Horizontal);
    highFreq_->setRange(2000, 16000);
    highFreq_->setValue(8000);
    eqGrid->addWidget(highFreq_, 6, 1);
    highFreqLabel_ = new QLabel("8000 Hz");
    eqGrid->addWidget(highFreqLabel_, 6, 2);
    
    eqLayout->addLayout(eqGrid);
    eqLayout->addStretch();
    
    connect(eqBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(lowGain_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(lowFreq_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(midGain_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(midFreq_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(midQ_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(highGain_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(highFreq_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(eqWidget, "EQ");
    
    // Compressor Tab
    QWidget* compWidget = new QWidget();
    QVBoxLayout* compLayout = new QVBoxLayout(compWidget);
    compBypass_ = new QCheckBox("Bypass");
    compBypass_->setChecked(true);
    compLayout->addWidget(compBypass_);
    
    QGridLayout* compGrid = new QGridLayout();
    compGrid->addWidget(new QLabel("Threshold:"), 0, 0);
    compThreshold_ = new QSlider(Qt::Horizontal);
    compThreshold_->setRange(-40, 0);
    compThreshold_->setValue(-20);
    compGrid->addWidget(compThreshold_, 0, 1);
    compThresholdLabel_ = new QLabel("-20 dB");
    compGrid->addWidget(compThresholdLabel_, 0, 2);
    
    compGrid->addWidget(new QLabel("Ratio:"), 1, 0);
    compRatio_ = new QSlider(Qt::Horizontal);
    compRatio_->setRange(10, 100);
    compRatio_->setValue(40);
    compGrid->addWidget(compRatio_, 1, 1);
    compRatioLabel_ = new QLabel("4.0:1");
    compGrid->addWidget(compRatioLabel_, 1, 2);
    
    compLayout->addLayout(compGrid);
    compLayout->addStretch();
    
    connect(compBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(compThreshold_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(compRatio_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(compWidget, "Compressor");
    
    // Pitch Shift Tab
    QWidget* pitchWidget = new QWidget();
    QVBoxLayout* pitchLayout = new QVBoxLayout(pitchWidget);
    
    pitchBypass_ = new QCheckBox("Bypass");
    pitchBypass_->setChecked(true);
    pitchLayout->addWidget(pitchBypass_);
    
    QHBoxLayout* pitchButtonLayout = new QHBoxLayout();
    pitchDownButton_ = new QPushButton("Half Step Down (-1)");
    pitchUpButton_ = new QPushButton("Half Step Up (+1)");
    pitchDownButton_->setCheckable(true);
    pitchUpButton_->setCheckable(true);
    pitchButtonLayout->addWidget(pitchDownButton_);
    pitchButtonLayout->addWidget(pitchUpButton_);
    pitchLayout->addLayout(pitchButtonLayout);
    
    pitchInfoLabel_ = new QLabel(
        "• Shifts your signal exactly one semitone up or down with minimal added latency; "
        "it's captured in recordings and loops.\n"
        "• Place time-based effects (delay/reverb) after pitch shift so their tails follow "
        "the shifted pitch naturally.\n"
        "• For tight feel, use smaller buffer sizes (e.g., 64–128 samples) in Audio I/O settings."
    );
    pitchInfoLabel_->setWordWrap(true);
    pitchInfoLabel_->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; border-radius: 5px; }");
    pitchLayout->addWidget(pitchInfoLabel_);
    
    pitchLayout->addStretch();
    
    connect(pitchBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(pitchDownButton_, &QPushButton::toggled, [this](bool checked) {
        if (checked) {
            pitchUpButton_->setChecked(false);
            currentPitchMode_ = 1;
            if (audioEngine_->getDSPChain()) {
                audioEngine_->getDSPChain()->getParams().pitchMode.store(1);
            }
        } else if (!pitchUpButton_->isChecked()) {
            currentPitchMode_ = 0;
            if (audioEngine_->getDSPChain()) {
                audioEngine_->getDSPChain()->getParams().pitchMode.store(0);
            }
        }
    });
    connect(pitchUpButton_, &QPushButton::toggled, [this](bool checked) {
        if (checked) {
            pitchDownButton_->setChecked(false);
            currentPitchMode_ = 2;
            if (audioEngine_->getDSPChain()) {
                audioEngine_->getDSPChain()->getParams().pitchMode.store(2);
            }
        } else if (!pitchDownButton_->isChecked()) {
            currentPitchMode_ = 0;
            if (audioEngine_->getDSPChain()) {
                audioEngine_->getDSPChain()->getParams().pitchMode.store(0);
            }
        }
    });
    
    effectsTab->addTab(pitchWidget, "Pitch Shift");
    
    // Delay Tab
    QWidget* delayWidget = new QWidget();
    QVBoxLayout* delayLayout = new QVBoxLayout(delayWidget);
    delayBypass_ = new QCheckBox("Bypass");
    delayBypass_->setChecked(true);
    delayLayout->addWidget(delayBypass_);
    
    QGridLayout* delayGrid = new QGridLayout();
    delayGrid->addWidget(new QLabel("Time:"), 0, 0);
    delayTime_ = new QSlider(Qt::Horizontal);
    delayTime_->setRange(10, 2000);
    delayTime_->setValue(250);
    delayGrid->addWidget(delayTime_, 0, 1);
    delayTimeLabel_ = new QLabel("250 ms");
    delayGrid->addWidget(delayTimeLabel_, 0, 2);
    
    delayGrid->addWidget(new QLabel("Feedback:"), 1, 0);
    delayFeedback_ = new QSlider(Qt::Horizontal);
    delayFeedback_->setRange(0, 95);
    delayFeedback_->setValue(30);
    delayGrid->addWidget(delayFeedback_, 1, 1);
    delayFeedbackLabel_ = new QLabel("30%");
    delayGrid->addWidget(delayFeedbackLabel_, 1, 2);
    
    delayGrid->addWidget(new QLabel("Mix:"), 2, 0);
    delayMix_ = new QSlider(Qt::Horizontal);
    delayMix_->setRange(0, 100);
    delayMix_->setValue(30);
    delayGrid->addWidget(delayMix_, 2, 1);
    delayMixLabel_ = new QLabel("30%");
    delayGrid->addWidget(delayMixLabel_, 2, 2);
    
    delayLayout->addLayout(delayGrid);
    delayLayout->addStretch();
    
    connect(delayBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(delayTime_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(delayFeedback_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(delayMix_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(delayWidget, "Delay");
    
    // Reverb Tab
    QWidget* reverbWidget = new QWidget();
    QVBoxLayout* reverbLayout = new QVBoxLayout(reverbWidget);
    reverbBypass_ = new QCheckBox("Bypass");
    reverbBypass_->setChecked(true);
    reverbLayout->addWidget(reverbBypass_);
    
    QGridLayout* reverbGrid = new QGridLayout();
    reverbGrid->addWidget(new QLabel("Size:"), 0, 0);
    reverbSize_ = new QSlider(Qt::Horizontal);
    reverbSize_->setRange(0, 100);
    reverbSize_->setValue(50);
    reverbGrid->addWidget(reverbSize_, 0, 1);
    reverbSizeLabel_ = new QLabel("50%");
    reverbGrid->addWidget(reverbSizeLabel_, 0, 2);
    
    reverbGrid->addWidget(new QLabel("Damping:"), 1, 0);
    reverbDamping_ = new QSlider(Qt::Horizontal);
    reverbDamping_->setRange(0, 100);
    reverbDamping_->setValue(50);
    reverbGrid->addWidget(reverbDamping_, 1, 1);
    reverbDampingLabel_ = new QLabel("50%");
    reverbGrid->addWidget(reverbDampingLabel_, 1, 2);
    
    reverbGrid->addWidget(new QLabel("Mix:"), 2, 0);
    reverbMix_ = new QSlider(Qt::Horizontal);
    reverbMix_->setRange(0, 100);
    reverbMix_->setValue(25);
    reverbGrid->addWidget(reverbMix_, 2, 1);
    reverbMixLabel_ = new QLabel("25%");
    reverbGrid->addWidget(reverbMixLabel_, 2, 2);
    
    reverbLayout->addLayout(reverbGrid);
    reverbLayout->addStretch();
    
    connect(reverbBypass_, &QCheckBox::toggled, this, &MainWindow::onEffectBypassChanged);
    connect(reverbSize_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(reverbDamping_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    connect(reverbMix_, &QSlider::valueChanged, this, &MainWindow::onEffectParameterChanged);
    
    effectsTab->addTab(reverbWidget, "Reverb");
}

void MainWindow::createLooperPanel()
{
    QGroupBox* panel = new QGroupBox("Looper", this);
    panel->setObjectName("looperPanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    looperRecordButton_ = new QPushButton("Record");
    looperPlayButton_ = new QPushButton("Play/Stop");
    looperOverdubButton_ = new QPushButton("Overdub");
    looperClearButton_ = new QPushButton("Clear");
    
    buttonLayout->addWidget(looperRecordButton_);
    buttonLayout->addWidget(looperPlayButton_);
    buttonLayout->addWidget(looperOverdubButton_);
    buttonLayout->addWidget(looperClearButton_);
    layout->addLayout(buttonLayout);
    
    QHBoxLayout* levelLayout = new QHBoxLayout();
    levelLayout->addWidget(new QLabel("Loop Level:"));
    looperLevelSlider_ = new QSlider(Qt::Horizontal);
    looperLevelSlider_->setRange(0, 200);
    looperLevelSlider_->setValue(100);
    levelLayout->addWidget(looperLevelSlider_);
    looperLevelLabel_ = new QLabel("100%");
    levelLayout->addWidget(looperLevelLabel_);
    layout->addLayout(levelLayout);
    
    looperStatusLabel_ = new QLabel("Status: Off");
    layout->addWidget(looperStatusLabel_);
    
    looperPositionBar_ = new QProgressBar();
    looperPositionBar_->setRange(0, 100);
    looperPositionBar_->setValue(0);
    layout->addWidget(looperPositionBar_);
    
    connect(looperRecordButton_, &QPushButton::clicked, this, &MainWindow::onLooperRecord);
    connect(looperPlayButton_, &QPushButton::clicked, this, &MainWindow::onLooperPlayStop);
    connect(looperOverdubButton_, &QPushButton::clicked, this, &MainWindow::onLooperOverdub);
    connect(looperClearButton_, &QPushButton::clicked, this, &MainWindow::onLooperClear);
    connect(looperLevelSlider_, &QSlider::valueChanged, this, &MainWindow::onLooperLevelChanged);
}

void MainWindow::createRecorderPanel()
{
    QGroupBox* panel = new QGroupBox("Recorder", this);
    panel->setObjectName("recorderPanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    recordStartButton_ = new QPushButton("Start Recording");
    recordStopButton_ = new QPushButton("Stop Recording");
    recordStopButton_->setEnabled(false);
    buttonLayout->addWidget(recordStartButton_);
    buttonLayout->addWidget(recordStopButton_);
    layout->addLayout(buttonLayout);
    
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Clip Name:"));
    recordNameEdit_ = new QLineEdit();
    recordNameEdit_->setPlaceholderText("Auto-generated");
    nameLayout->addWidget(recordNameEdit_);
    layout->addLayout(nameLayout);
    
    downloadButton_ = new QPushButton("Download/Save As");
    downloadButton_->setEnabled(false);
    layout->addWidget(downloadButton_);
    
    recordStatusLabel_ = new QLabel("Status: Ready");
    layout->addWidget(recordStatusLabel_);
    
    recordDurationLabel_ = new QLabel("Duration: 00:00");
    layout->addWidget(recordDurationLabel_);
    
    connect(recordStartButton_, &QPushButton::clicked, this, &MainWindow::onStartRecording);
    connect(recordStopButton_, &QPushButton::clicked, this, &MainWindow::onStopRecording);
    connect(downloadButton_, &QPushButton::clicked, this, &MainWindow::onDownloadRecording);
}

void MainWindow::createPlaybackPanel()
{
    QGroupBox* panel = new QGroupBox("Playback Studio", this);
    panel->setObjectName("playbackPanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    clipList_ = new QListWidget();
    layout->addWidget(clipList_);
    
    QHBoxLayout* transportLayout = new QHBoxLayout();
    playButton_ = new QPushButton("Play");
    pauseButton_ = new QPushButton("Pause");
    stopPlayButton_ = new QPushButton("Stop");
    transportLayout->addWidget(playButton_);
    transportLayout->addWidget(pauseButton_);
    transportLayout->addWidget(stopPlayButton_);
    layout->addLayout(transportLayout);
    
    playbackProgressBar_ = new QProgressBar();
    playbackProgressBar_->setRange(0, 100);
    layout->addWidget(playbackProgressBar_);
    
    playbackPositionLabel_ = new QLabel("00:00 / 00:00");
    layout->addWidget(playbackPositionLabel_);
    
    QHBoxLayout* volumeLayout = new QHBoxLayout();
    volumeLayout->addWidget(new QLabel("Volume:"));
    clipVolumeSlider_ = new QSlider(Qt::Horizontal);
    clipVolumeSlider_->setRange(0, 100);
    clipVolumeSlider_->setValue(100);
    volumeLayout->addWidget(clipVolumeSlider_);
    clipVolumeLabel_ = new QLabel("100%");
    volumeLayout->addWidget(clipVolumeLabel_);
    layout->addLayout(volumeLayout);
    
    QHBoxLayout* manageLayout = new QHBoxLayout();
    renameButton_ = new QPushButton("Rename");
    deleteButton_ = new QPushButton("Delete");
    revealButton_ = new QPushButton("Reveal in Explorer");
    manageLayout->addWidget(renameButton_);
    manageLayout->addWidget(deleteButton_);
    manageLayout->addWidget(revealButton_);
    layout->addLayout(manageLayout);
    
    connect(clipList_, &QListWidget::itemSelectionChanged, this, &MainWindow::onClipSelected);
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayClip);
    connect(pauseButton_, &QPushButton::clicked, this, &MainWindow::onPauseClip);
    connect(stopPlayButton_, &QPushButton::clicked, this, &MainWindow::onStopClip);
    connect(renameButton_, &QPushButton::clicked, this, &MainWindow::onRenameClip);
    connect(deleteButton_, &QPushButton::clicked, this, &MainWindow::onDeleteClip);
    connect(revealButton_, &QPushButton::clicked, this, &MainWindow::onRevealClip);
    connect(clipVolumeSlider_, &QSlider::valueChanged, this, &MainWindow::onClipVolumeChanged);
    
    // Populate clip list
    QStringList clips = clipManager_->getClipList();
    clipList_->addItems(clips);
}

void MainWindow::createPresetsPanel()
{
    QGroupBox* panel = new QGroupBox("Presets", this);
    panel->setObjectName("presetsPanel");
    QVBoxLayout* layout = new QVBoxLayout(panel);
    
    presetList_ = new QListWidget();
    layout->addWidget(presetList_);
    
    QHBoxLayout* nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("Preset Name:"));
    presetNameEdit_ = new QLineEdit();
    presetNameEdit_->setPlaceholderText("Enter preset name");
    nameLayout->addWidget(presetNameEdit_);
    layout->addLayout(nameLayout);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    savePresetButton_ = new QPushButton("Save");
    loadPresetButton_ = new QPushButton("Load");
    deletePresetButton_ = new QPushButton("Delete");
    buttonLayout->addWidget(savePresetButton_);
    buttonLayout->addWidget(loadPresetButton_);
    buttonLayout->addWidget(deletePresetButton_);
    layout->addLayout(buttonLayout);
    
    connect(savePresetButton_, &QPushButton::clicked, this, &MainWindow::onSavePreset);
    connect(loadPresetButton_, &QPushButton::clicked, this, &MainWindow::onLoadPreset);
    connect(deletePresetButton_, &QPushButton::clicked, this, &MainWindow::onDeletePreset);
    
    refreshPresetList();
}

// Slot implementations

void MainWindow::onStartEngine()
{
    std::string inputId = inputDeviceCombo_->currentData().toString().toStdString();
    std::string outputId = outputDeviceCombo_->currentData().toString().toStdString();
    int sampleRate = sampleRateSpin_->value();
    int bufferSize = bufferSizeSpin_->value();
    bool wasapi = wasapiCheck_->isChecked();
    
    if (audioEngine_->start(inputId, outputId, sampleRate, bufferSize, wasapi)) {
        engineRunning_ = true;
        startButton_->setEnabled(false);
        stopButton_->setEnabled(true);
        
        // Update effects to audio engine
        onEffectBypassChanged();
        onEffectParameterChanged();
        
        QMessageBox::information(this, "Success", "Audio engine started successfully!");
    } else {
        QMessageBox::critical(this, "Error", "Failed to start audio engine!");
    }
}

void MainWindow::onStopEngine()
{
    audioEngine_->stop();
    engineRunning_ = false;
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
}

void MainWindow::onInputGainChanged(int value)
{
    float gain = value / 100.0f;
    audioEngine_->setInputGain(gain);
    inputGainLabel_->setText(QString::number(linearTodB(gain), 'f', 1) + " dB");
}

void MainWindow::onOutputGainChanged(int value)
{
    float gain = value / 100.0f;
    audioEngine_->setOutputGain(gain);
    outputGainLabel_->setText(QString::number(linearTodB(gain), 'f', 1) + " dB");
}

void MainWindow::onResetPeaks()
{
    audioEngine_->resetPeaks();
}

void MainWindow::onEffectBypassChanged()
{
    if (!audioEngine_->getDSPChain()) return;
    
    auto& params = audioEngine_->getDSPChain()->getParams();
    params.gateBypass.store(gateBypass_->isChecked());
    params.driveBypass.store(driveBypass_->isChecked());
    params.eqBypass.store(eqBypass_->isChecked());
    params.compBypass.store(compBypass_->isChecked());
    params.pitchBypass.store(pitchBypass_->isChecked());
    params.delayBypass.store(delayBypass_->isChecked());
    params.reverbBypass.store(reverbBypass_->isChecked());
}

void MainWindow::onEffectParameterChanged()
{
    if (!audioEngine_->getDSPChain()) return;
    
    auto& params = audioEngine_->getDSPChain()->getParams();
    
    // Gate
    params.gateThreshold.store(gateThreshold_->value());
    gateThresholdLabel_->setText(QString::number(gateThreshold_->value()) + " dB");
    
    // Drive
    params.driveAmount.store(driveAmount_->value() / 100.0f);
    params.driveType.store(driveType_->currentIndex());
    driveAmountLabel_->setText(QString::number(driveAmount_->value()) + "%");
    
    // EQ
    params.lowGain.store(lowGain_->value());
    params.lowFreq.store(lowFreq_->value());
    params.midGain.store(midGain_->value());
    params.midFreq.store(midFreq_->value());
    params.midQ.store(midQ_->value() / 10.0f);
    params.highGain.store(highGain_->value());
    params.highFreq.store(highFreq_->value());
    
    lowGainLabel_->setText(QString::number(lowGain_->value()) + " dB");
    lowFreqLabel_->setText(QString::number(lowFreq_->value()) + " Hz");
    midGainLabel_->setText(QString::number(midGain_->value()) + " dB");
    midFreqLabel_->setText(QString::number(midFreq_->value()) + " Hz");
    midQLabel_->setText(QString::number(midQ_->value() / 10.0f, 'f', 1));
    highGainLabel_->setText(QString::number(highGain_->value()) + " dB");
    highFreqLabel_->setText(QString::number(highFreq_->value()) + " Hz");
    
    // Compressor
    params.compThreshold.store(compThreshold_->value());
    params.compRatio.store(compRatio_->value() / 10.0f);
    compThresholdLabel_->setText(QString::number(compThreshold_->value()) + " dB");
    compRatioLabel_->setText(QString::number(compRatio_->value() / 10.0f, 'f', 1) + ":1");
    
    // Delay
    params.delayTime.store(delayTime_->value() / 1000.0f);
    params.delayFeedback.store(delayFeedback_->value() / 100.0f);
    params.delayMix.store(delayMix_->value() / 100.0f);
    delayTimeLabel_->setText(QString::number(delayTime_->value()) + " ms");
    delayFeedbackLabel_->setText(QString::number(delayFeedback_->value()) + "%");
    delayMixLabel_->setText(QString::number(delayMix_->value()) + "%");
    
    // Reverb
    params.reverbSize.store(reverbSize_->value() / 100.0f);
    params.reverbDamping.store(reverbDamping_->value() / 100.0f);
    params.reverbMix.store(reverbMix_->value() / 100.0f);
    reverbSizeLabel_->setText(QString::number(reverbSize_->value()) + "%");
    reverbDampingLabel_->setText(QString::number(reverbDamping_->value()) + "%");
    reverbMixLabel_->setText(QString::number(reverbMix_->value()) + "%");
}

void MainWindow::onLooperRecord()
{
    if (!audioEngine_->getLooper()) return;
    
    auto state = audioEngine_->getLooper()->getState();
    if (state == LooperState::Off || state == LooperState::Playing) {
        audioEngine_->getLooper()->startRecording();
    } else if (state == LooperState::Recording) {
        audioEngine_->getLooper()->stopRecording();
    }
}

void MainWindow::onLooperPlayStop()
{
    if (!audioEngine_->getLooper()) return;
    
    auto state = audioEngine_->getLooper()->getState();
    if (state == LooperState::Off) {
        audioEngine_->getLooper()->startPlaying();
    } else {
        audioEngine_->getLooper()->stopPlaying();
    }
}

void MainWindow::onLooperOverdub()
{
    if (!audioEngine_->getLooper()) return;
    
    auto state = audioEngine_->getLooper()->getState();
    if (state == LooperState::Playing) {
        audioEngine_->getLooper()->startOverdub();
    } else if (state == LooperState::Overdubbing) {
        audioEngine_->getLooper()->stopOverdub();
    }
}

void MainWindow::onLooperClear()
{
    if (!audioEngine_->getLooper()) return;
    audioEngine_->getLooper()->clear();
}

void MainWindow::onLooperLevelChanged(int value)
{
    if (!audioEngine_->getLooper()) return;
    float level = value / 100.0f;
    audioEngine_->getLooper()->setLoopLevel(level);
    looperLevelLabel_->setText(QString::number(value) + "%");
}

void MainWindow::onStartRecording()
{
    if (!audioEngine_->getRecorder()) return;
    
    audioEngine_->getRecorder()->startRecording();
    isRecording_ = true;
    recordStartButton_->setEnabled(false);
    recordStopButton_->setEnabled(true);
    downloadButton_->setEnabled(false);
    recordStatusLabel_->setText("Status: Recording...");
}

void MainWindow::onStopRecording()
{
    if (!audioEngine_->getRecorder()) return;
    
    audioEngine_->getRecorder()->stopRecording();
    isRecording_ = false;
    recordStartButton_->setEnabled(true);
    recordStopButton_->setEnabled(false);
    
    if (audioEngine_->getRecorder()->hasRecordedAudio()) {
        downloadButton_->setEnabled(true);
        recordStatusLabel_->setText("Status: Ready to save");
        
        // Generate default name if empty
        if (recordNameEdit_->text().isEmpty()) {
            currentClipName_ = clipManager_->generateClipName();
            recordNameEdit_->setText(currentClipName_);
        } else {
            currentClipName_ = recordNameEdit_->text();
        }
    } else {
        recordStatusLabel_->setText("Status: No audio recorded");
    }
}

void MainWindow::onDownloadRecording()
{
    if (!audioEngine_->getRecorder()) return;
    
    QString clipName = recordNameEdit_->text();
    if (clipName.isEmpty()) {
        clipName = clipManager_->generateClipName();
        recordNameEdit_->setText(clipName);
    }
    
    QString filepath = clipManager_->getClipsDirectory() + "/" + clipName + ".wav";
    
    if (audioEngine_->getRecorder()->saveToFile(filepath.toStdString())) {
        QMessageBox::information(this, "Success", 
            QString("Recording saved as:\n%1").arg(clipName));
        
        // Refresh clip list
        clipList_->clear();
        clipList_->addItems(clipManager_->getClipList());
        
        // Clear recorder
        audioEngine_->getRecorder()->clearRecording();
        recordNameEdit_->clear();
        downloadButton_->setEnabled(false);
        recordStatusLabel_->setText("Status: Ready");
    } else {
        QMessageBox::critical(this, "Error", "Failed to save recording!");
    }
}

void MainWindow::onClipSelected()
{
    // Enable/disable buttons based on selection
    bool hasSelection = !clipList_->selectedItems().isEmpty();
    playButton_->setEnabled(hasSelection);
    pauseButton_->setEnabled(hasSelection);
    stopPlayButton_->setEnabled(hasSelection);
    renameButton_->setEnabled(hasSelection);
    deleteButton_->setEnabled(hasSelection);
    revealButton_->setEnabled(hasSelection);
}

void MainWindow::onPlayClip()
{
    if (clipList_->selectedItems().isEmpty()) return;
    
    QString clipName = clipList_->currentItem()->text();
    QString filepath = clipManager_->getClipsDirectory() + "/" + clipName + ".wav";
    
    mediaPlayer_->setSource(QUrl::fromLocalFile(filepath));
    mediaPlayer_->play();
}

void MainWindow::onPauseClip()
{
    mediaPlayer_->pause();
}

void MainWindow::onStopClip()
{
    mediaPlayer_->stop();
    playbackProgressBar_->setValue(0);
    playbackPositionLabel_->setText("00:00 / 00:00");
}

void MainWindow::onRenameClip()
{
    if (clipList_->selectedItems().isEmpty()) return;
    
    QString oldName = clipList_->currentItem()->text();
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Clip", 
        "Enter new name:", QLineEdit::Normal, oldName, &ok);
    
    if (ok && !newName.isEmpty() && newName != oldName) {
        if (clipManager_->renameClip(oldName, newName)) {
            clipList_->clear();
            clipList_->addItems(clipManager_->getClipList());
            QMessageBox::information(this, "Success", "Clip renamed successfully!");
        } else {
            QMessageBox::critical(this, "Error", "Failed to rename clip!");
        }
    }
}

void MainWindow::onDeleteClip()
{
    if (clipList_->selectedItems().isEmpty()) return;
    
    QString clipName = clipList_->currentItem()->text();
    
    auto reply = QMessageBox::question(this, "Delete Clip",
        QString("Are you sure you want to delete '%1'?").arg(clipName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (clipManager_->deleteClip(clipName)) {
            clipList_->clear();
            clipList_->addItems(clipManager_->getClipList());
            QMessageBox::information(this, "Success", "Clip deleted successfully!");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete clip!");
        }
    }
}

void MainWindow::onRevealClip()
{
    if (clipList_->selectedItems().isEmpty()) return;
    
    QString clipName = clipList_->currentItem()->text();
    clipManager_->revealInExplorer(clipName);
}

void MainWindow::onClipVolumeChanged(int value)
{
    audioOutput_->setVolume(value / 100.0f);
    clipVolumeLabel_->setText(QString::number(value) + "%");
}

void MainWindow::onSavePreset()
{
    QString presetName = presetNameEdit_->text();
    if (presetName.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please enter a preset name!");
        return;
    }
    
    savePresetToFile(presetName);
    refreshPresetList();
    QMessageBox::information(this, "Success", 
        QString("Preset '%1' saved successfully!").arg(presetName));
}

void MainWindow::onLoadPreset()
{
    if (presetList_->selectedItems().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a preset to load!");
        return;
    }
    
    QString presetName = presetList_->currentItem()->text();
    loadPresetFromFile(presetName);
    updateEffectsUI();
    QMessageBox::information(this, "Success", 
        QString("Preset '%1' loaded successfully!").arg(presetName));
}

void MainWindow::onDeletePreset()
{
    if (presetList_->selectedItems().isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a preset to delete!");
        return;
    }
    
    QString presetName = presetList_->currentItem()->text();
    
    auto reply = QMessageBox::question(this, "Delete Preset",
        QString("Are you sure you want to delete preset '%1'?").arg(presetName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        QString filepath = getPresetsDirectory() + "/" + presetName + ".json";
        QFile file(filepath);
        if (file.remove()) {
            refreshPresetList();
            QMessageBox::information(this, "Success", "Preset deleted successfully!");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete preset!");
        }
    }
}

void MainWindow::updateMeters()
{
    if (!engineRunning_) return;
    
    // Input meter
    float inputLevel = audioEngine_->getInputLevel();
    float inputPeak = audioEngine_->getInputPeak();
    int inputPercent = static_cast<int>(inputLevel * 100);
    inputMeter_->setValue(std::min(100, inputPercent));
    
    if (inputPeak > 0.0001f) {
        inputPeakLabel_->setText(QString("Peak: %1 dB").arg(linearTodB(inputPeak), 0, 'f', 1));
    } else {
        inputPeakLabel_->setText("Peak: -∞ dB");
    }
    
    // Output meter
    float outputLevel = audioEngine_->getOutputLevel();
    float outputPeak = audioEngine_->getOutputPeak();
    int outputPercent = static_cast<int>(outputLevel * 100);
    outputMeter_->setValue(std::min(100, outputPercent));
    
    if (outputPeak > 0.0001f) {
        outputPeakLabel_->setText(QString("Peak: %1 dB").arg(linearTodB(outputPeak), 0, 'f', 1));
    } else {
        outputPeakLabel_->setText("Peak: -∞ dB");
    }
    
    // Change color if clipping
    if (inputPeak >= 0.99f) {
        inputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #ff0000; }");
    } else {
        inputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #00ff00; }");
    }
    
    if (outputPeak >= 0.99f) {
        outputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #ff0000; }");
    } else {
        outputMeter_->setStyleSheet("QProgressBar::chunk { background-color: #00ff00; }");
    }
}

void MainWindow::updateLooperStatus()
{
    if (!audioEngine_->getLooper()) return;
    
    auto state = audioEngine_->getLooper()->getState();
    int loopLength = audioEngine_->getLooper()->getLoopLength();
    int position = audioEngine_->getLooper()->getCurrentPosition();
    
    QString statusText;
    switch (state) {
        case LooperState::Off:
            statusText = "Status: Off";
            looperPositionBar_->setValue(0);
            break;
        case LooperState::Recording:
            statusText = "Status: Recording...";
            if (loopLength > 0) {
                looperPositionBar_->setValue(static_cast<int>(100.0f * position / loopLength));
            }
            break;
        case LooperState::Playing:
            statusText = QString("Status: Playing (%1s)")
                .arg(loopLength / static_cast<float>(audioEngine_->getSampleRate()), 0, 'f', 1);
            if (loopLength > 0) {
                looperPositionBar_->setValue(static_cast<int>(100.0f * position / loopLength));
            }
            break;
        case LooperState::Overdubbing:
            statusText = QString("Status: Overdubbing (%1s)")
                .arg(loopLength / static_cast<float>(audioEngine_->getSampleRate()), 0, 'f', 1);
            if (loopLength > 0) {
                looperPositionBar_->setValue(static_cast<int>(100.0f * position / loopLength));
            }
            break;
    }
    
    looperStatusLabel_->setText(statusText);
}

void MainWindow::updateRecorderStatus()
{
    if (!audioEngine_->getRecorder()) return;
    
    if (isRecording_) {
        float duration = audioEngine_->getRecorder()->getRecordingDuration();
        recordDurationLabel_->setText(QString("Duration: %1").arg(formatTime(duration)));
    }
}

void MainWindow::updatePlaybackPosition()
{
    if (mediaPlayer_->playbackState() == QMediaPlayer::PlayingState) {
        qint64 position = mediaPlayer_->position();
        qint64 duration = mediaPlayer_->duration();
        
        if (duration > 0) {
            playbackProgressBar_->setValue(static_cast<int>(100.0f * position / duration));
            playbackPositionLabel_->setText(QString("%1 / %2")
                .arg(formatTime(position / 1000.0f))
                .arg(formatTime(duration / 1000.0f)));
        }
    }
}

void MainWindow::updateEffectsUI()
{
    if (!audioEngine_->getDSPChain()) return;
    
    auto& params = audioEngine_->getDSPChain()->getParams();
    
    // Update all UI elements from parameters
    gateBypass_->setChecked(params.gateBypass.load());
    gateThreshold_->setValue(params.gateThreshold.load());
    
    driveBypass_->setChecked(params.driveBypass.load());
    driveAmount_->setValue(params.driveAmount.load() * 100);
    driveType_->setCurrentIndex(params.driveType.load());
    
    eqBypass_->setChecked(params.eqBypass.load());
    lowGain_->setValue(params.lowGain.load());
    lowFreq_->setValue(params.lowFreq.load());
    midGain_->setValue(params.midGain.load());
    midFreq_->setValue(params.midFreq.load());
    midQ_->setValue(params.midQ.load() * 10);
    highGain_->setValue(params.highGain.load());
    highFreq_->setValue(params.highFreq.load());
    
    compBypass_->setChecked(params.compBypass.load());
    compThreshold_->setValue(params.compThreshold.load());
    compRatio_->setValue(params.compRatio.load() * 10);
    
    pitchBypass_->setChecked(params.pitchBypass.load());
    int pitchMode = params.pitchMode.load();
    pitchDownButton_->setChecked(pitchMode == 1);
    pitchUpButton_->setChecked(pitchMode == 2);
    
    delayBypass_->setChecked(params.delayBypass.load());
    delayTime_->setValue(params.delayTime.load() * 1000);
    delayFeedback_->setValue(params.delayFeedback.load() * 100);
    delayMix_->setValue(params.delayMix.load() * 100);
    
    reverbBypass_->setChecked(params.reverbBypass.load());
    reverbSize_->setValue(params.reverbSize.load() * 100);
    reverbDamping_->setValue(params.reverbDamping.load() * 100);
    reverbMix_->setValue(params.reverbMix.load() * 100);
    
    // Update looper
    looperLevelSlider_->setValue(audioEngine_->getLooper()->getLoopLevel() * 100);
    
    // Trigger label updates
    onEffectParameterChanged();
}

void MainWindow::savePresetToFile(const QString& name)
{
    if (!audioEngine_->getDSPChain()) return;
    
    auto& params = audioEngine_->getDSPChain()->getParams();
    
    QJsonObject json;
    
    // Gate
    json["gateBypass"] = params.gateBypass.load();
    json["gateThreshold"] = static_cast<double>(params.gateThreshold.load());
    
    // Drive
    json["driveBypass"] = params.driveBypass.load();
    json["driveAmount"] = static_cast<double>(params.driveAmount.load());
    json["driveType"] = params.driveType.load();
    
    // EQ
    json["eqBypass"] = params.eqBypass.load();
    json["lowGain"] = static_cast<double>(params.lowGain.load());
    json["lowFreq"] = static_cast<double>(params.lowFreq.load());
    json["midGain"] = static_cast<double>(params.midGain.load());
    json["midFreq"] = static_cast<double>(params.midFreq.load());
    json["midQ"] = static_cast<double>(params.midQ.load());
    json["highGain"] = static_cast<double>(params.highGain.load());
    json["highFreq"] = static_cast<double>(params.highFreq.load());
    
    // Compressor
    json["compBypass"] = params.compBypass.load();
    json["compThreshold"] = static_cast<double>(params.compThreshold.load());
    json["compRatio"] = static_cast<double>(params.compRatio.load());
    
    // Pitch
    json["pitchBypass"] = params.pitchBypass.load();
    json["pitchMode"] = params.pitchMode.load();
    
    // Delay
    json["delayBypass"] = params.delayBypass.load();
    json["delayTime"] = static_cast<double>(params.delayTime.load());
    json["delayFeedback"] = static_cast<double>(params.delayFeedback.load());
    json["delayMix"] = static_cast<double>(params.delayMix.load());
    
    // Reverb
    json["reverbBypass"] = params.reverbBypass.load();
    json["reverbSize"] = static_cast<double>(params.reverbSize.load());
    json["reverbDamping"] = static_cast<double>(params.reverbDamping.load());
    json["reverbMix"] = static_cast<double>(params.reverbMix.load());
    
    // Global
    json["inputGain"] = static_cast<double>(audioEngine_->getInputGain());
    json["outputGain"] = static_cast<double>(audioEngine_->getOutputGain());
    json["loopLevel"] = static_cast<double>(audioEngine_->getLooper()->getLoopLevel());
    
    QString filepath = getPresetsDirectory() + "/" + name + ".json";
    QFile file(filepath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(json);
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::loadPresetFromFile(const QString& name)
{
    QString filepath = getPresetsDirectory() + "/" + name + ".json";
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject json = doc.object();
    
    if (!audioEngine_->getDSPChain()) return;
    auto& params = audioEngine_->getDSPChain()->getParams();
    
    // Gate
    params.gateBypass.store(json["gateBypass"].toBool());
    params.gateThreshold.store(json["gateThreshold"].toDouble());
    
    // Drive
    params.driveBypass.store(json["driveBypass"].toBool());
    params.driveAmount.store(json["driveAmount"].toDouble());
    params.driveType.store(json["driveType"].toInt());
    
    // EQ
    params.eqBypass.store(json["eqBypass"].toBool());
    params.lowGain.store(json["lowGain"].toDouble());
    params.lowFreq.store(json["lowFreq"].toDouble());
    params.midGain.store(json["midGain"].toDouble());
    params.midFreq.store(json["midFreq"].toDouble());
    params.midQ.store(json["midQ"].toDouble());
    params.highGain.store(json["highGain"].toDouble());
    params.highFreq.store(json["highFreq"].toDouble());
    
    // Compressor
    params.compBypass.store(json["compBypass"].toBool());
    params.compThreshold.store(json["compThreshold"].toDouble());
    params.compRatio.store(json["compRatio"].toDouble());
    
    // Pitch
    params.pitchBypass.store(json["pitchBypass"].toBool());
    params.pitchMode.store(json["pitchMode"].toInt());
    
    // Delay
    params.delayBypass.store(json["delayBypass"].toBool());
    params.delayTime.store(json["delayTime"].toDouble());
    params.delayFeedback.store(json["delayFeedback"].toDouble());
    params.delayMix.store(json["delayMix"].toDouble());
    
    // Reverb
    params.reverbBypass.store(json["reverbBypass"].toBool());
    params.reverbSize.store(json["reverbSize"].toDouble());
    params.reverbDamping.store(json["reverbDamping"].toDouble());
    params.reverbMix.store(json["reverbMix"].toDouble());
    
    // Global
    audioEngine_->setInputGain(json["inputGain"].toDouble());
    audioEngine_->setOutputGain(json["outputGain"].toDouble());
    audioEngine_->getLooper()->setLoopLevel(json["loopLevel"].toDouble());
    
    inputGainSlider_->setValue(audioEngine_->getInputGain() * 100);
    outputGainSlider_->setValue(audioEngine_->getOutputGain() * 100);
    looperLevelSlider_->setValue(audioEngine_->getLooper()->getLoopLevel() * 100);
}

void MainWindow::refreshPresetList()
{
    presetList_->clear();
    
    QDir dir(getPresetsDirectory());
    QStringList filters;
    filters << "*.json";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const QFileInfo& file : files) {
        presetList_->addItem(file.baseName());
    }
}

QString MainWindow::getPresetsDirectory()
{
#ifdef Q_OS_WIN
    QString dir = QDir::homePath() + "/AppData/Roaming/GuitarEffectsApp/Presets";
#elif defined(Q_OS_MAC)
    QString dir = QDir::homePath() + "/Library/Application Support/GuitarEffectsApp/Presets";
#else
    QString dir = QDir::homePath() + "/.local/share/GuitarEffectsApp/Presets";
#endif
    
    QDir().mkpath(dir);
    return dir;
}

QString MainWindow::formatTime(float seconds)
{
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    return QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
}

float MainWindow::dBToLinear(float dB)
{
    return std::pow(10.0f, dB / 20.0f);
}

float MainWindow::linearTodB(float linear)
{
    if (linear < 0.00001f) return -100.0f;
    return 20.0f * std::log10(linear);
}