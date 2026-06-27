#pragma once

#include "AudioInput.h"
#include "AudioOutput.h"
#include "ModemController.h"
#include "WaterfallWidget.h"
#include "hftext_app_settings.h"
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
class QProgressBar;
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
    void updateWaterfallMarkers();
    void applySelectedSpeedProfile();
    void restartReceiveIfActive();
    void applyDefaultSettings();
    void saveLog();
    void saveFieldEvidence();

private:
    void appendLog(const QString& text);
    void appendReceivedLine(const QString& text);
    void writeLogHeader(QTextStream& stream, const char* title) const;
    void writeFieldSummaryCsv(QTextStream& stream, const QString& wavPath, std::size_t sampleCount, int sampleRate) const;
    void writeAcceptedRxFramesCsv(QTextStream& stream) const;
    void resetRxDiagnostic(const QString& state);
    void resetRxFrameProgress();
    void updateRxFrameProgressFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void updateRxDiagnosticFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void setRxDiagnosticText(const QString& state);
    void resetRxSessionCounters();
    void updateRxSessionFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events);
    void setRxSessionText();
    void rememberAcceptedRx(const hftext::DecodeResult& result, const hftext::ModemConfig& config);
    bool hasRecentAcceptedRx() const;
    void setTransmitButtonTransmitting(bool transmitting);
    void setReceiveControlsRecording(bool recording);
    QString selectedSpeedProfileKey() const;
    QString speedProfileDescription(const QString& profileKey) const;
    bool writeDefaultModemConfigFile() const;
    void loadModemConfigFile();
    hftext::ModemConfig configForSpeedProfile(const QString& profileKey, int sampleRate) const;
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
    struct AcceptedRxFrame {
        QString acceptedAtIso;
        double elapsedSeconds = 0.0;
        QString text;
        hftext::ModemConfig config;
        QString qualityText = "--";
        int length = -1;
        int offsetSamples = 0;
        int offsetsTried = 0;
    };
    std::thread rxWorker_;
    std::mutex rxMutex_;
    std::condition_variable rxCondition_;
    std::deque<float> rxPendingSamples_;
    std::size_t rxMaxPendingSamples_ = 0;
    std::size_t rxMaxWorkerChunkSamples_ = 0;
    std::atomic<std::size_t> rxPendingSampleCount_{0};
    std::atomic<std::size_t> rxMaxObservedPendingSamples_{0};
    std::atomic<std::size_t> rxDroppedSamples_{0};
    std::mutex rxEvidenceMutex_;
    std::deque<float> rxEvidenceSamples_;
    int rxEvidenceSampleRate_ = 48000;
    std::atomic<bool> waterfallUpdatePending_{false};
    bool rxWorkerStop_ = false;
    QString lastWavPath_;
    QString lastRxWavPath_;
    QString modemConfigPath_;
    QString modemConfigWarning_;
    hftext::AppModemProfiles modemProfiles_;
    QLineEdit* callsignEdit_ = nullptr;
    QPlainTextEdit* messageEdit_ = nullptr;
    QComboBox* speedProfileCombo_ = nullptr;
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
    QPushButton* transmitButton_ = nullptr;
    QPushButton* startReceiveButton_ = nullptr;
    QPushButton* stopReceiveButton_ = nullptr;
    QPushButton* saveLogButton_ = nullptr;
    QPushButton* clearLogButton_ = nullptr;
    QPushButton* saveEvidenceButton_ = nullptr;
    QPushButton* defaultSettingsButton_ = nullptr;
    QTimer* rxLevelTimer_ = nullptr;
    QTimer* txProgressTimer_ = nullptr;
    QString lastRxPhysicalLengthText_ = "--";
    QString lastRxQualityText_ = "--";
    QString lastRxRejectText_ = "--";
    bool hasLastAcceptedRx_ = false;
    QString lastAcceptedRxQualityText_ = "--";
    int lastAcceptedRxLength_ = -1;
    int lastAcceptedRxOffsetSamples_ = 0;
    int lastAcceptedRxOffsetsTried_ = 0;
    std::vector<AcceptedRxFrame> acceptedRxFrames_;
    int rxSessionSyncCount_ = 0;
    int rxSessionLengthCount_ = 0;
    int rxSessionRejectedCount_ = 0;
    int rxSessionAcceptedCount_ = 0;
    std::int64_t rxSessionStartedAtMsecs_ = 0;
    std::int64_t lastAcceptedRxAtMsecs_ = 0;
    std::int64_t rxProgressSyncSample_ = -1;
    int rxDisplayedFrameProgressPermille_ = 0;
};
