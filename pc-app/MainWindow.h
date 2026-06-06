#pragma once

#include "AudioInput.h"
#include "AudioOutput.h"
#include "ModemController.h"
#include "WaterfallWidget.h"
#include "hftext_streaming_receiver.h"

#include <QMainWindow>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <QString>
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
class QTextStream;
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
    void clearLog();
    void updateRxLevel();
    void updateTxProgress();
    void sanitizeTxMessage();
    void updateTxEstimate();
    void saveLog();
    void saveFieldEvidence();

private:
    void appendLog(const QString& text);
    void writeLogHeader(QTextStream& stream, const char* title) const;
    void resetRxDiagnostic(const QString& state);
    void resetRxFrameProgress();
    void updateRxFrameProgressFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void updateRxDiagnosticFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void setRxDiagnosticText(const QString& state);
    void resetRxSessionCounters();
    void updateRxSessionFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void setRxSessionText();
    void loadSettings();
    void saveSettings() const;
    void populateInputDevices();
    void populateOutputDevices();
    hftext::ModemConfig readConfig() const;
    hftext::ModemConfig readRxConfig() const;
    void showDecodeResult(const hftext::DecodeResult& result);
    void startRxWorker(const hftext::ModemConfig& config, bool detailedRxLog);
    void stopRxWorker(bool drainPending = false);
    void enqueueRxSamples(const std::vector<float>& samples);
    void clearRxEvidenceSamples(int sampleRate);
    void appendRxEvidenceSamples(const std::vector<float>& samples, int sampleRate);
    void rxWorkerLoop(hftext::ModemConfig config, bool detailedRxLog);

    AudioInput audioInput_;
    AudioOutput audioOutput_;
    ModemController controller_;
    std::thread rxWorker_;
    std::mutex rxMutex_;
    std::condition_variable rxCondition_;
    std::vector<float> rxPendingSamples_;
    std::size_t rxMaxPendingSamples_ = 0;
    std::size_t rxMaxWorkerChunkSamples_ = 0;
    std::mutex rxEvidenceMutex_;
    std::deque<float> rxEvidenceSamples_;
    int rxEvidenceSampleRate_ = 48000;
    std::atomic<bool> waterfallUpdatePending_{false};
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
    QProgressBar* rxFrameProgressBar_ = nullptr;
    QProgressBar* rxQualityBar_ = nullptr;
    QLabel* rxDiagnosticLabel_ = nullptr;
    QLabel* rxSessionLabel_ = nullptr;
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
    QPushButton* saveLogButton_ = nullptr;
    QPushButton* clearLogButton_ = nullptr;
    QPushButton* saveEvidenceButton_ = nullptr;
    QTimer* rxLevelTimer_ = nullptr;
    QTimer* txProgressTimer_ = nullptr;
    QString lastRxPhysicalLengthText_ = "--";
    QString lastRxQualityText_ = "--";
    QString lastRxRejectText_ = "--";
    int rxSessionSyncCount_ = 0;
    int rxSessionLengthCount_ = 0;
    int rxSessionRejectedCount_ = 0;
    int rxSessionAcceptedCount_ = 0;
    std::int64_t rxSessionStartedAtMsecs_ = 0;
    std::int64_t rxProgressSyncSample_ = -1;
    int rxDisplayedFrameProgressPermille_ = 0;
};
