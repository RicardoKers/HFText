#pragma once

#include "AudioInput.h"
#include "AudioOutput.h"
#include "ModemController.h"
#include "WaterfallWidget.h"
#include "hftext_streaming_receiver.h"

#include <QMainWindow>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class QLineEdit;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QProgressBar;
class QSpinBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void generateWav();
    void decodeWav();
    void transmitWav();
    void stopTransmit();
    void startReceive();
    void stopReceive();
    void clearReceivedText();
    void updateRxLevel();
    void updateTxProgress();
    void sanitizeTxMessage();
    void updateTxEstimate();

private:
    void appendLog(const QString& text);
    void populateInputDevices();
    void populateOutputDevices();
    hftext::ModemConfig readConfig() const;
    hftext::ModemConfig readRxConfig() const;
    void showDecodeResult(const hftext::DecodeResult& result);
    void startRxWorker(const hftext::ModemConfig& config, bool detailedRxLog);
    void stopRxWorker(bool drainPending = false);
    void enqueueRxSamples(const std::vector<float>& samples);
    void rxWorkerLoop(hftext::ModemConfig config, bool detailedRxLog);

    AudioInput audioInput_;
    AudioOutput audioOutput_;
    ModemController controller_;
    std::thread rxWorker_;
    std::mutex rxMutex_;
    std::condition_variable rxCondition_;
    std::vector<float> rxPendingSamples_;
    bool rxWorkerStop_ = false;
    QString lastWavPath_;
    QString lastRxWavPath_;
    QLineEdit* callsignEdit_ = nullptr;
    QPlainTextEdit* messageEdit_ = nullptr;
    QSpinBox* sampleRateSpin_ = nullptr;
    QSpinBox* rxSampleRateSpin_ = nullptr;
    QDoubleSpinBox* symbolDurationSpin_ = nullptr;
    QDoubleSpinBox* frequency0Spin_ = nullptr;
    QDoubleSpinBox* frequency1Spin_ = nullptr;
    QDoubleSpinBox* amplitudeSpin_ = nullptr;
    QSpinBox* preambleBitsSpin_ = nullptr;
    QComboBox* inputDeviceCombo_ = nullptr;
    QComboBox* outputDeviceCombo_ = nullptr;
    QLabel* txEstimateLabel_ = nullptr;
    QProgressBar* txProgressBar_ = nullptr;
    QProgressBar* rxLevelBar_ = nullptr;
    QProgressBar* rxQualityBar_ = nullptr;
    WaterfallWidget* waterfallWidget_ = nullptr;
    QCheckBox* detailedRxLogCheck_ = nullptr;
    QPlainTextEdit* receivedEdit_ = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QPushButton* generateButton_ = nullptr;
    QPushButton* decodeButton_ = nullptr;
    QPushButton* transmitButton_ = nullptr;
    QPushButton* stopTransmitButton_ = nullptr;
    QPushButton* startReceiveButton_ = nullptr;
    QPushButton* stopReceiveButton_ = nullptr;
    QPushButton* clearReceivedButton_ = nullptr;
    QTimer* rxLevelTimer_ = nullptr;
    QTimer* txProgressTimer_ = nullptr;
};
