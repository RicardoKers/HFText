#include "MainWindow.h"

#include "hftext_app_rx.h"
#include "hftext_audio_stats.h"
#include "hftext_encoder.h"
#include "hftext_version.h"
#include "wav_io.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int kMaxLogBlocks = 3000;
constexpr int kRxPendingSeconds = 120;
constexpr int kRxWorkerChunkMilliseconds = 1000;
constexpr int kMaxDetailedRxLogLinesPerBatch = 80;
constexpr int kRxEvidenceSeconds = 300;
constexpr int kMaxAcceptedRxSnapshots = 512;
constexpr int kAcceptedRxStateHoldSeconds = 60;
constexpr const char* kDefaultCallsign = "nocall";

QString modulationModeIniName(hftext::ModulationMode mode) {
    return QString::fromLatin1(hftext::modulationModeKey(mode));
}

hftext::ModulationMode modulationModeFromIniValue(const QString& value, hftext::ModulationMode fallback) {
    return hftext::modulationModeFromKey(value.toStdString(), fallback);
}

QString modulationModeName(hftext::ModulationMode mode) {
    return QString::fromStdString(hftext::modulationModeDisplayName(mode));
}

QString versionDisplayText() {
    return QString("%1 (%2, %3)")
        .arg(QString::fromLatin1(hftext::kVersionLabel))
        .arg(QString::fromLatin1(hftext::kReleaseTrack))
        .arg(QString::fromLatin1(hftext::kProtocolVersion));
}

QString audioPeakPercent(const std::vector<float>& samples) {
    const float peak = hftext::analyzeAudioSamples(samples).peak;
    return QString::number(peak * 100.0F, 'f', 1) + "%";
}

void updateAtomicMax(std::atomic<std::size_t>& target, std::size_t value) {
    std::size_t current = target.load(std::memory_order_relaxed);
    while (current < value
           && !target.compare_exchange_weak(
               current,
               value,
               std::memory_order_relaxed,
               std::memory_order_relaxed
           )) {
    }
}

QString sampleDurationText(std::size_t samples, int sampleRate) {
    if (sampleRate <= 0) {
        return "--";
    }
    return QString::number(static_cast<double>(samples) / static_cast<double>(sampleRate), 'f', 2) + " s";
}

std::string toStdString(const QString& text) {
    return text.toUtf8().toStdString();
}

QString sanitizePresentationText(const QString& text) {
    QString output;
    output.reserve(text.size());

    for (const QChar ch : text) {
        if (ch.unicode() <= 0x7F && hftext::isSupportedPresentationChar(static_cast<char>(ch.unicode()))) {
            output.append(ch);
        } else if (QStringLiteral("áÁéÉíÍóÓúÚãÃõÕçÇ").contains(ch)) {
            output.append(ch);
        } else {
            output.append('?');
        }
    }

    return output;
}

QString formatConfidence(float confidence) {
    return QString::number(std::clamp(confidence, 0.0F, 1.0F) * 100.0F, 'f', 1) + "%";
}

QString formatElapsedTime(std::int64_t elapsedMsecs) {
    const auto totalSeconds = static_cast<int>(std::max<std::int64_t>(0, elapsedMsecs) / 1000);
    const int hours = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString csvCell(QString value) {
    value.replace('"', "\"\"");
    return "\"" + value + "\"";
}

QString csvBool(bool value) {
    return value ? "1" : "0";
}

QString formatClippingSummary(std::size_t clippedSamples, std::size_t sampleCount) {
    return QString::number(static_cast<qulonglong>(clippedSamples))
        + " samples (" + QString::number(hftext::clippingPercent(clippedSamples, sampleCount), 'f', 4) + "%)";
}

QString frameRejectionReason(const hftext::StreamingReceiverEvent& event) {
    QStringList reasons;
    if (!event.crcOk) {
        reasons.push_back("CRC failed");
    }
    if (!event.payloadValid) {
        reasons.push_back("invalid payload");
    }
    if (event.decodedLength >= 0 && event.payloadLength >= 0 && event.decodedLength != event.payloadLength) {
        reasons.push_back(
            QString("LENGTH %1/%2")
                .arg(event.decodedLength)
                .arg(event.payloadLength)
        );
    }
    if (reasons.isEmpty()) {
        return "candidate rejected";
    }
    return reasons.join("; ");
}

QString clippingAdvice(std::size_t clippedSamples, std::size_t sampleCount) {
    if (clippedSamples == 0 || sampleCount == 0) {
        return {};
    }

    const double percent = hftext::clippingPercent(clippedSamples, sampleCount);
    if (percent < 0.01) {
        return "RX: isolated clipping peaks detected; probably impulse noise.";
    }
    if (percent < 0.5) {
        return "RX: occasional clipping detected; check whether it matches channel noise.";
    }
    return "RX: frequent clipping detected; reduce input gain/volume if possible.";
}

void selectComboText(QComboBox* combo, const QString& text) {
    if (combo == nullptr || text.isEmpty()) {
        return;
    }

    const int index = combo->findText(text);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

const hftext::StreamingReceiverEvent* selectedRxEvent(
    const std::vector<hftext::StreamingReceiverEvent>& events,
    int index
) {
    if (index < 0 || static_cast<std::size_t>(index) >= events.size()) {
        return nullptr;
    }
    return &events[static_cast<std::size_t>(index)];
}

QString formatStreamingEvent(const hftext::StreamingReceiverEvent& event) {
    const QString phase = QString("phase %1 samples").arg(event.phaseOffsetSamples);

    switch (event.type) {
    case hftext::StreamingReceiverEventType::SyncFound:
        return QString("RX: START_SYNC found (%1, bit %2, errors %3)")
            .arg(phase)
            .arg(event.syncBitIndex)
            .arg(event.syncMismatches);
    case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
        return QString("RX: PHYS_LENGTH=%1 symbols (%2 expected robust bits, %3)")
            .arg(event.payloadLength)
            .arg(event.bitsExpected)
            .arg(phase);
    case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        return QString("RX: invalid PHYS_LENGTH (%1, bit %2)")
            .arg(phase)
            .arg(event.syncBitIndex);
    case hftext::StreamingReceiverEventType::FrameWaiting: {
        const double progress = event.bitsExpected <= 0
            ? 0.0
            : 100.0 * static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
        return QString("RX: accumulating ROBUST_FRAME %1/%2 bits (%3%, %4)")
            .arg(event.bitsAvailable)
            .arg(event.bitsExpected)
            .arg(progress, 0, 'f', 0)
            .arg(phase);
    }
    case hftext::StreamingReceiverEventType::FrameRejected:
        return QString("RX: frame rejected (CRC %1, payload %2, LENGTH %3/%4, conf %5, %6)")
            .arg(event.crcOk ? "ok" : "failed")
            .arg(event.payloadValid ? "ok" : "failed")
            .arg(event.decodedLength)
            .arg(event.payloadLength)
            .arg(formatConfidence(event.confidence))
            .arg(phase);
    case hftext::StreamingReceiverEventType::FrameDecoded:
        return QString("RX: valid frame (%1 symbols, conf %2, latency %3 s, %4)")
            .arg(event.payloadLength)
            .arg(formatConfidence(event.confidence))
            .arg(event.latencySeconds, 0, 'f', 2)
            .arg(phase);
    }

    return "RX: unknown event";
}

QString normalRxStatusKey(const QString& line) {
    if (line.startsWith("RX: strong sync")) {
        return line;
    }
    if (line.startsWith("RX: frame ")) {
        const int prefixLength = QStringLiteral("RX: frame ").size();
        const int percentEnd = line.indexOf('%', prefixLength);
        if (percentEnd < 0) {
            return "RX: frame";
        }
        bool ok = false;
        const int percent = line.mid(prefixLength, percentEnd - prefixLength).toInt(&ok);
        if (ok) {
            return QString("RX: frame %1").arg(hftext::frameProgressLogMilestone(percent));
        }
        return "RX: frame";
    }
    if (line.contains("candidate(s) rejected")) {
        return line;
    }
    return {};
}

std::vector<QString> filterRepeatedNormalRxStatus(
    std::vector<QString> lines,
    std::vector<QString>& emittedStatusKeys
) {
    std::vector<QString> filtered;
    filtered.reserve(lines.size());

    for (const auto& line : lines) {
        const QString key = normalRxStatusKey(line);
        if (key.isEmpty()) {
            filtered.push_back(line);
            continue;
        }

        if (std::find(emittedStatusKeys.begin(), emittedStatusKeys.end(), key) != emittedStatusKeys.end()) {
            continue;
        }

        emittedStatusKeys.push_back(key);
        filtered.push_back(line);
    }

    return filtered;
}

std::vector<QString> formatStreamingEvents(
    const std::vector<hftext::StreamingReceiverEvent>& events,
    bool detailed,
    int maxDetailedLines = -1
) {
    std::vector<QString> lines;
    if (detailed) {
        const auto limit = maxDetailedLines < 0
            ? events.size()
            : std::min(events.size(), static_cast<std::size_t>(maxDetailedLines));
        lines.reserve(limit + (limit < events.size() ? 1 : 0));
        for (std::size_t index = 0; index < limit; ++index) {
            lines.push_back(formatStreamingEvent(events[index]));
        }
        if (limit < events.size()) {
            lines.push_back(
                QString("RX: %1 detailed event(s) omitted to keep the interface responsive.")
                    .arg(static_cast<qulonglong>(events.size() - limit))
            );
        }
        return lines;
    }

    const auto selection = hftext::selectRxEvents(events);
    const auto* bestDecoded = selectedRxEvent(events, selection.bestDecoded);
    const auto* bestLength = selectedRxEvent(events, selection.bestLength);
    const auto* bestWaiting = selectedRxEvent(events, selection.bestWaiting);
    const auto* bestSync = selectedRxEvent(events, selection.bestSync);
    const auto* bestRejected = selectedRxEvent(events, selection.bestRejected);

    if (bestDecoded != nullptr) {
        lines.push_back(formatStreamingEvent(*bestDecoded));
        return lines;
    }

    if (bestLength != nullptr) {
        lines.push_back(
            QString("RX: strong sync, PHYS_LENGTH=%1 symbols (%2 bits, errors %3)")
                .arg(bestLength->payloadLength)
                .arg(bestLength->bitsExpected)
                .arg(bestLength->syncMismatches)
        );
    } else if (bestSync != nullptr) {
        lines.push_back(
            QString("RX: strong sync detected (%1 errors)")
                .arg(bestSync->syncMismatches)
        );
    }

    if (bestWaiting != nullptr) {
        const int progress = hftext::frameProgressLogMilestone(hftext::frameProgressPercent(*bestWaiting));
        lines.push_back(
            QString("RX: frame %1% (%2/%3 bits)")
                .arg(progress)
                .arg(bestWaiting->bitsAvailable)
                .arg(bestWaiting->bitsExpected)
        );
    }

    if (selection.rejectedCount > 0) {
        lines.push_back(
            QString("RX: %1 candidate(s) rejected by CRC/payload (best %2, %3).")
                .arg(selection.rejectedCount)
                .arg(formatConfidence(bestRejected->confidence))
                .arg(frameRejectionReason(*bestRejected))
        );
    }

    return lines;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle(QString::fromLatin1(hftext::kVersionLabel));

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* tabs = new QTabWidget(this);
    auto* operationPage = new QWidget(this);
    auto* operationLayout = new QVBoxLayout(operationPage);
    auto* configScroll = new QScrollArea(this);
    auto* configPage = new QWidget(configScroll);
    auto* configLayout = new QVBoxLayout(configPage);
    auto* configForm = new QFormLayout();
    configScroll->setWidgetResizable(true);

    receivedEdit_ = new QPlainTextEdit(this);
    receivedEdit_->setReadOnly(true);
    receivedEdit_->setPlaceholderText("Received messages");
    receivedEdit_->setMinimumHeight(120);
    receivedEdit_->setContextMenuPolicy(Qt::CustomContextMenu);
    operationLayout->addWidget(receivedEdit_, 3);

    operationLayout->addWidget(new QLabel("RX waterfall", this));
    waterfallWidget_ = new WaterfallWidget(this);
    operationLayout->addWidget(waterfallWidget_, 2);

    auto* txStatusLayout = new QHBoxLayout();
    txStatusLayout->addWidget(new QLabel("Speed", this));
    speedProfileCombo_ = new QComboBox(this);
    speedProfileCombo_->addItem("Fast", "fast");
    speedProfileCombo_->addItem("Slow", "slow");
    txStatusLayout->addWidget(speedProfileCombo_);
    txEstimateLabel_ = new QLabel(this);
    txEstimateLabel_->setWordWrap(true);
    txStatusLayout->addWidget(txEstimateLabel_, 1);
    operationLayout->addLayout(txStatusLayout);

    txProgressBar_ = new QProgressBar(this);
    txProgressBar_->setRange(0, 1000);
    txProgressBar_->setValue(0);
    txProgressBar_->setTextVisible(false);
    txProgressBar_->setMaximumHeight(10);
    operationLayout->addWidget(txProgressBar_);

    auto* composerLayout = new QHBoxLayout();
    messageEdit_ = new QPlainTextEdit(this);
    messageEdit_->setPlaceholderText("Message");
    messageEdit_->setMinimumHeight(44);
    messageEdit_->setMaximumHeight(64);
    composerLayout->addWidget(messageEdit_, 1);

    transmitButton_ = new QPushButton(this);
    transmitButton_->setFixedSize(44, 44);
    transmitButton_->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    transmitButton_->setToolTip("Send");
    composerLayout->addWidget(transmitButton_);
    operationLayout->addLayout(composerLayout);

    callsignEdit_ = new QLineEdit(this);
    callsignEdit_->setText(kDefaultCallsign);
    configForm->addRow("Callsign", callsignEdit_);

    auto* versionLabel = new QLabel(versionDisplayText(), this);
    versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    versionLabel->setWordWrap(true);
    configForm->addRow("Version", versionLabel);

    outputDeviceCombo_ = new QComboBox(this);
    configForm->addRow("Audio output", outputDeviceCombo_);
    populateOutputDevices();

    inputDeviceCombo_ = new QComboBox(this);
    configForm->addRow("Audio input", inputDeviceCombo_);
    populateInputDevices();

    detailedRxLogCheck_ = new QCheckBox("Detailed RX log", this);
    detailedRxLogCheck_->setChecked(false);
    configForm->addRow("Diagnostics", detailedRxLogCheck_);

    rxLevelBar_ = new QProgressBar(this);
    rxLevelBar_->setRange(0, 100);
    rxLevelBar_->setValue(0);
    rxLevelBar_->setTextVisible(false);
    rxLevelBar_->setVisible(false);

    rxFrameProgressBar_ = new QProgressBar(this);
    rxFrameProgressBar_->setRange(0, 1000);
    rxFrameProgressBar_->setValue(0);
    rxFrameProgressBar_->setFormat("%p%");
    rxFrameProgressBar_->setVisible(false);

    rxQualityBar_ = new QProgressBar(this);
    rxQualityBar_->setRange(0, 1000);
    rxQualityBar_->setValue(0);
    rxQualityBar_->setFormat("%p%");
    rxQualityBar_->setVisible(false);

    rxDiagnosticLabel_ = new QLabel(this);
    rxDiagnosticLabel_->setWordWrap(true);
    rxDiagnosticLabel_->setVisible(false);
    resetRxDiagnostic("Stopped");

    rxSessionLabel_ = new QLabel(this);
    rxSessionLabel_->setWordWrap(true);
    rxSessionLabel_->setVisible(false);
    resetRxSessionCounters();

    configLayout->addLayout(configForm);

    auto* defaultButtons = new QHBoxLayout();
    defaultSettingsButton_ = new QPushButton("Load defaults", this);
    defaultButtons->addWidget(defaultSettingsButton_);
    defaultButtons->addStretch(1);
    configLayout->addLayout(defaultButtons);

    auto* rxButtons = new QHBoxLayout();
    startReceiveButton_ = new QPushButton("Start RX", this);
    stopReceiveButton_ = new QPushButton("Stop RX", this);
    rxButtons->addWidget(startReceiveButton_);
    rxButtons->addWidget(stopReceiveButton_);
    rxButtons->addStretch(1);
    configLayout->addLayout(rxButtons);

    auto* logHeader = new QHBoxLayout();
    logHeader->addWidget(new QLabel("Log", this));
    logHeader->addStretch(1);
    saveLogButton_ = new QPushButton("Save log", this);
    clearLogButton_ = new QPushButton("Clear log", this);
    saveEvidenceButton_ = new QPushButton("Save RX evidence", this);
    logHeader->addWidget(saveEvidenceButton_);
    logHeader->addWidget(saveLogButton_);
    logHeader->addWidget(clearLogButton_);
    configLayout->addLayout(logHeader);

    logEdit_ = new QPlainTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(180);
    logEdit_->document()->setMaximumBlockCount(kMaxLogBlocks);
    configLayout->addWidget(logEdit_);
    configLayout->addStretch(1);
    configScroll->setWidget(configPage);

    tabs->addTab(operationPage, "Operation");
    tabs->addTab(configScroll, "Settings");
    root->addWidget(tabs);

    setCentralWidget(central);
    resize(720, 520);
    loadModemConfigFile();
    loadSettings();
    applySelectedSpeedProfile();

    connect(transmitButton_, &QPushButton::clicked, this, &MainWindow::transmitWav);
    connect(startReceiveButton_, &QPushButton::clicked, this, &MainWindow::startReceive);
    connect(stopReceiveButton_, &QPushButton::clicked, this, &MainWindow::stopReceive);
    connect(saveLogButton_, &QPushButton::clicked, this, &MainWindow::saveLog);
    connect(clearLogButton_, &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(saveEvidenceButton_, &QPushButton::clicked, this, &MainWindow::saveFieldEvidence);
    connect(defaultSettingsButton_, &QPushButton::clicked, this, &MainWindow::applyDefaultSettings);
    connect(receivedEdit_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        QAction* clearAction = menu.addAction("Clear RX");
        QAction* selected = menu.exec(receivedEdit_->mapToGlobal(pos));
        if (selected == clearAction) {
            clearReceivedText();
        }
    });
    connect(callsignEdit_, &QLineEdit::textChanged, this, &MainWindow::updateTxEstimate);
    connect(messageEdit_, &QPlainTextEdit::textChanged, this, &MainWindow::sanitizeTxMessage);
    connect(speedProfileCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::applySelectedSpeedProfile);
    connect(inputDeviceCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::restartReceiveIfActive);
    connect(detailedRxLogCheck_, &QCheckBox::toggled, this, &MainWindow::restartReceiveIfActive);

    rxLevelTimer_ = new QTimer(this);
    rxLevelTimer_->setInterval(100);
    connect(rxLevelTimer_, &QTimer::timeout, this, &MainWindow::updateRxLevel);

    txProgressTimer_ = new QTimer(this);
    txProgressTimer_->setInterval(100);
    connect(txProgressTimer_, &QTimer::timeout, this, &MainWindow::updateTxProgress);
    updateTxEstimate();
    updateWaterfallMarkers();
    setReceiveControlsRecording(false);
    appendLog("Application: " + versionDisplayText());
    appendLog("Modem config: " + modemConfigPath_);
    if (!modemConfigWarning_.isEmpty()) {
        appendLog("Modem config warning: " + modemConfigWarning_);
    }
    appendLog("Speed profile: " + speedProfileDescription(selectedSpeedProfileKey()));
    QTimer::singleShot(0, this, [this]() {
        if (inputDeviceCombo_ != nullptr && inputDeviceCombo_->isEnabled() && inputDeviceCombo_->count() > 0) {
            startReceive();
        } else {
            appendLog("Automatic RX not started: no input device.");
        }
    });
}

MainWindow::~MainWindow() {
    saveSettings();
    stopRxWorker();
    audioInput_.setSamplesCallback({});
    (void)audioInput_.stopAndSave({});
    audioOutput_.stop();
}

void MainWindow::generateWav() {
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Save WAV",
        QString(),
        "WAV (*.wav)"
    );
    if (outputPath.isEmpty()) {
        return;
    }

    try {
        const auto config = readConfig();
        controller_.setConfig(config);
        const std::string callsign = toStdString(callsignEdit_->text().trimmed());
        const std::string message = toStdString(messageEdit_->toPlainText());
        controller_.generateWav(callsign, message, toStdString(outputPath));
        lastWavPath_ = outputPath;
        appendLog("WAV generated: " + outputPath);
        appendLog("TX mode: " + modulationModeName(config.modulationMode));
        appendLog("Payload TX: " + QString::fromStdString(controller_.buildPayload(callsign, message)));
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while generating WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::decodeWav() {
    const QString inputPath = QFileDialog::getOpenFileName(
        this,
        "Open WAV",
        QString(),
        "WAV (*.wav)"
    );
    if (inputPath.isEmpty()) {
        return;
    }

    try {
        controller_.setConfig(readConfig());
        const auto stats = controller_.analyzeWav(toStdString(inputPath));
        appendLog(
            "WAV duration: " + QString::number(stats.durationSeconds(), 'f', 2)
            + " s, sample rate: " + QString::number(stats.sampleRate)
            + " Hz, peak: " + QString::number(stats.peak * 100.0F, 'f', 1)
            + "%, approx. clipping: " + formatClippingSummary(stats.clippedSamples, stats.sampleCount)
        );
        const QString advice = clippingAdvice(stats.clippedSamples, stats.sampleCount);
        if (!advice.isEmpty()) {
            appendLog(advice);
        }
        const auto result = controller_.decodeWav(toStdString(inputPath));
        showDecodeResult(result);
        if (result.crcOk && result.payloadValid) {
            appendLog("RX text: " + QString::fromStdString(result.text));
        }
        appendLog("WAV decoded: " + inputPath);
        appendLog("RX mode: " + modulationModeName(readConfig().modulationMode));
        appendLog(
            "RX offset: " + QString::number(result.startOffset)
            + " samples, attempts: " + QString::number(result.offsetsTried)
            + ", confidence: " + formatConfidence(result.confidence)
        );
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while decoding WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::transmitWav() {
    if (audioOutput_.isPlaying()) {
        stopTransmit();
        return;
    }

    try {
        const auto config = readConfig();
        controller_.setConfig(config);
        const std::string callsign = toStdString(callsignEdit_->text().trimmed());
        const std::string message = toStdString(messageEdit_->toPlainText());
        const auto estimate = controller_.estimateTransmission(callsign, message);
        if (estimate.messageEmpty) {
            throw std::invalid_argument("message is empty");
        }
        if (estimate.payloadTooLong) {
            throw std::invalid_argument("message exceeds 127 symbols");
        }

        auto audio = controller_.generateAudio(callsign, message);
        const QString txPeak = audioPeakPercent(audio);
        const unsigned int deviceId = outputDeviceCombo_->currentData().toUInt();
        audioOutput_.playSamplesAsync(std::move(audio), config.sampleRate, deviceId);
        txProgressBar_->setValue(0);
        setTransmitButtonTransmitting(true);
        txProgressTimer_->start();
        appendLog("TX started.");
        appendLog("TX mode: " + modulationModeName(config.modulationMode));
        appendLog("Generated TX peak: " + txPeak);
        appendLog("Payload TX: " + QString::fromStdString(estimate.payload));
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while transmitting: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopTransmit() {
    const bool wasPlaying = audioOutput_.isPlaying();
    audioOutput_.stop();
    txProgressTimer_->stop();
    txProgressBar_->setValue(0);
    setTransmitButtonTransmitting(false);
    if (wasPlaying) {
        appendLog("TX interrupted.");
    }
}

void MainWindow::startReceive() {
    if (audioInput_.isRecording()) {
        restartReceiveIfActive();
        return;
    }

    try {
        const auto rxConfig = readRxConfig();
        stopRxWorker();
        startRxWorker(rxConfig, detailedRxLogCheck_->isChecked());
        clearRxEvidenceSamples(rxConfig.sampleRate);
        waterfallWidget_->clear();
        waterfallUpdatePending_.store(false);
        resetRxFrameProgress();
        rxQualityBar_->setValue(0);
        resetRxDiagnostic("Listening");
        resetRxSessionCounters();
        rxSessionStartedAtMsecs_ = QDateTime::currentMSecsSinceEpoch();
        setRxSessionText();
        audioInput_.setSamplesCallback([this, sampleRate = rxConfig.sampleRate](const std::vector<float>& samples) {
            appendRxEvidenceSamples(samples, sampleRate);
            enqueueRxSamples(samples);
            if (!waterfallUpdatePending_.exchange(true)) {
                auto chunk = samples;
                QMetaObject::invokeMethod(
                    this,
                    [this, chunk = std::move(chunk), sampleRate]() {
                        waterfallUpdatePending_.store(false);
                        waterfallWidget_->addSamples(chunk, sampleRate);
                    },
                    Qt::QueuedConnection
                );
            }
        });
        const unsigned int deviceId = inputDeviceCombo_->currentData().toUInt();
        audioInput_.start(deviceId, rxConfig.sampleRate);
        lastRxWavPath_.clear();
        rxLevelTimer_->start();
        setReceiveControlsRecording(true);
        appendLog(
            QStringLiteral("RX streaming started")
            + " (capture " + QString::number(rxConfig.sampleRate) + " Hz)"
        );
    } catch (const std::exception& exc) {
        setReceiveControlsRecording(false);
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while starting RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopReceive() {
    if (!audioInput_.isRecording()) {
        setReceiveControlsRecording(false);
        appendLog("RX is not running.");
        return;
    }

    try {
        const auto stats = audioInput_.stopAndSave({});
        audioInput_.setSamplesCallback({});
        waterfallUpdatePending_.store(false);
        stopRxWorker(false);
        rxLevelTimer_->stop();
        rxLevelBar_->setValue(0);
        setReceiveControlsRecording(false);
        resetRxFrameProgress();
        resetRxDiagnostic("Stopped");
        const std::string error = audioInput_.lastError();
        if (!error.empty()) {
            QMessageBox::warning(this, "HFText", QString::fromStdString(error));
            appendLog("RX error: " + QString::fromStdString(error));
            return;
        }
        appendLog("RX streaming stopped.");
        appendLog("RX summary: " + rxSessionLabel_->text());
        const auto droppedSamples = rxDroppedSamples_.load(std::memory_order_relaxed);
        const auto maxPendingSamples = rxMaxObservedPendingSamples_.load(std::memory_order_relaxed);
        appendLog(
            "RX worker backlog: max pending " + sampleDurationText(maxPendingSamples, stats.sampleRate)
            + ", dropped " + sampleDurationText(droppedSamples, stats.sampleRate)
            + " (" + QString::number(static_cast<qulonglong>(droppedSamples)) + " samples)"
        );
        appendLog(
            "RX duration: " + QString::number(stats.durationSeconds(), 'f', 2)
            + " s, sample rate: " + QString::number(stats.sampleRate)
            + " Hz, peak: " + QString::number(stats.peak * 100.0F, 'f', 1)
            + "%, approx. clipping: " + formatClippingSummary(stats.clippedSamples, stats.sampleCount)
        );
        const QString advice = clippingAdvice(stats.clippedSamples, stats.sampleCount);
        if (!advice.isEmpty()) {
            appendLog(advice);
        }
    } catch (const std::exception& exc) {
        setReceiveControlsRecording(audioInput_.isRecording());
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while stopping RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::clearReceivedText() {
    receivedEdit_->clear();
}

void MainWindow::clearLog() {
    logEdit_->clear();
}

void MainWindow::updateRxLevel() {
    const int level = static_cast<int>(std::clamp(audioInput_.level(), 0.0F, 1.0F) * 100.0F);
    rxLevelBar_->setValue(level);
    if (audioInput_.isRecording()) {
        setRxSessionText();
    }
}

void MainWindow::updateTxProgress() {
    const double duration = audioOutput_.durationSeconds();
    if (duration <= 0.0) {
        txProgressBar_->setValue(0);
        setTransmitButtonTransmitting(false);
        return;
    }

    const double position = audioOutput_.positionSeconds();
    const int value = static_cast<int>(std::clamp(position / duration, 0.0, 1.0) * 1000.0);
    txProgressBar_->setValue(value);

    if (!audioOutput_.isPlaying() && position >= duration) {
        txProgressTimer_->stop();
        txProgressBar_->setValue(1000);
        setTransmitButtonTransmitting(false);
        appendLog("TX completed.");
    }
}

void MainWindow::sanitizeTxMessage() {
    const QString current = messageEdit_->toPlainText();
    const QString sanitized = sanitizePresentationText(current);
    if (sanitized != current) {
        QTextCursor cursor = messageEdit_->textCursor();
        const int position = cursor.position();
        messageEdit_->blockSignals(true);
        messageEdit_->setPlainText(sanitized);
        cursor = messageEdit_->textCursor();
        cursor.setPosition((std::min)(position, static_cast<int>(sanitized.size())));
        messageEdit_->setTextCursor(cursor);
        messageEdit_->blockSignals(false);
    }
    updateTxEstimate();
}

void MainWindow::updateTxEstimate() {
    try {
        controller_.setConfig(readConfig());
        const auto estimate = controller_.estimateTransmission(
            toStdString(callsignEdit_->text().trimmed()),
            toStdString(messageEdit_->toPlainText())
        );

        if (estimate.messageEmpty) {
            txEstimateLabel_->setStyleSheet({});
            txEstimateLabel_->setText(
                QString("0/%1 symbols | TX: --")
                    .arg(estimate.maxPayloadSymbols)
            );
            return;
        }

        QString text = QString("%1/%2 symbols | TX: %3 s")
            .arg(estimate.payloadSymbols)
            .arg(estimate.maxPayloadSymbols)
            .arg(estimate.durationSeconds, 0, 'f', 2);

        if (estimate.payloadTooLong) {
            text += " | over limit";
            txEstimateLabel_->setStyleSheet("color: #b00020;");
        } else {
            txEstimateLabel_->setStyleSheet({});
        }
        txEstimateLabel_->setText(text);
    } catch (const std::exception& exc) {
        txEstimateLabel_->setStyleSheet("color: #b00020;");
        txEstimateLabel_->setText(QString::fromUtf8(exc.what()));
    }
}

void MainWindow::updateWaterfallMarkers() {
    if (waterfallWidget_ == nullptr) {
        return;
    }

    hftext::ModemConfig config;
    try {
        config = readConfig();
    } catch (const std::exception&) {
        return;
    }
    const auto toneFrequencies = hftext::modulationToneFrequenciesHz(config);
    std::vector<double> frequencies;
    frequencies.reserve(toneFrequencies.size());
    for (const float frequency : toneFrequencies) {
        frequencies.push_back(frequency);
    }
    waterfallWidget_->setMarkerFrequencies(frequencies);
}

void MainWindow::applySelectedSpeedProfile() {
    const bool wasRecording = audioInput_.isRecording();
    updateTxEstimate();
    updateWaterfallMarkers();
    if (wasRecording) {
        appendLog("RX restarted to apply speed profile: " + speedProfileDescription(selectedSpeedProfileKey()));
        restartReceiveIfActive();
    }
}

void MainWindow::restartReceiveIfActive() {
    if (!audioInput_.isRecording()) {
        return;
    }

    appendLog("RX restarted to apply settings.");
    stopReceive();
    if (inputDeviceCombo_ != nullptr && inputDeviceCombo_->isEnabled() && inputDeviceCombo_->count() > 0) {
        startReceive();
    }
}

void MainWindow::applyDefaultSettings() {
    const bool wasRecording = audioInput_.isRecording();

    if (!writeDefaultModemConfigFile()) {
        QMessageBox::warning(this, "HFText", "Could not write hftext.ini.");
        appendLog("Error while writing modem defaults: " + modemConfigPath_);
        return;
    }

    loadModemConfigFile();
    callsignEdit_->setText(kDefaultCallsign);
    detailedRxLogCheck_->setChecked(false);
    updateTxEstimate();
    updateWaterfallMarkers();
    appendLog("Default modem configuration written: " + modemConfigPath_);
    if (wasRecording) {
        restartReceiveIfActive();
    }
}

void MainWindow::saveLog() {
    const QString defaultName = "HFText-log-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")
        + ".txt";
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Save log",
        defaultName,
        "Text (*.txt)"
    );
    if (outputPath.isEmpty()) {
        return;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "HFText", "Could not save the log.");
        appendLog("Error while saving log: " + outputPath);
        return;
    }

    QTextStream stream(&file);
    writeLogHeader(stream, "HFText log");
    stream << '\n';
    stream << "--- Log ---\n";
    stream << logEdit_->toPlainText() << '\n';
    appendLog("Log saved: " + outputPath);
}

void MainWindow::saveFieldEvidence() {
    std::vector<float> samples;
    int sampleRate = 48000;
    {
        std::lock_guard<std::mutex> lock(rxEvidenceMutex_);
        samples.assign(rxEvidenceSamples_.begin(), rxEvidenceSamples_.end());
        sampleRate = rxEvidenceSampleRate_;
    }

    if (samples.empty()) {
        QMessageBox::information(this, "HFText", "There is no recent RX audio to save.");
        appendLog("RX evidence not saved: no recent audio.");
        return;
    }

    const QString outputDir = QFileDialog::getExistingDirectory(
        this,
        "Save RX evidence"
    );
    if (outputDir.isEmpty()) {
        return;
    }

    const QString stem = "HFText-rx-evidence-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const QDir dir(outputDir);
    const QString wavPath = dir.filePath(stem + ".wav");
    const QString logPath = dir.filePath(stem + ".txt");

    try {
        hftext::tools::writeMonoPcm16Wav(toStdString(wavPath), samples, sampleRate);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Error while saving RX evidence WAV: " + QString::fromUtf8(exc.what()));
        return;
    }

    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "HFText", "Could not save the evidence log.");
        appendLog("Error while saving RX evidence log: " + logPath);
        return;
    }

    QTextStream stream(&file);
    writeLogHeader(stream, "HFText RX evidence");
    stream << "Recent RX WAV: " << wavPath << '\n';
    stream << "Recent RX window: up to " << kRxEvidenceSeconds << " s\n";
    stream << "Saved RX samples: " << static_cast<qulonglong>(samples.size()) << '\n';
    stream << "Saved RX duration: "
           << QString::number(static_cast<double>(samples.size()) / static_cast<double>(sampleRate), 'f', 2)
           << " s\n\n";
    writeFieldSummaryCsv(stream, wavPath, samples.size(), sampleRate);
    stream << '\n';
    writeAcceptedRxFramesCsv(stream);
    stream << '\n';
    stream << "--- Received Text ---\n";
    stream << receivedEdit_->toPlainText() << "\n\n";
    stream << "--- Log ---\n";
    stream << logEdit_->toPlainText() << '\n';

    lastRxWavPath_ = wavPath;
    appendLog("RX evidence saved: " + wavPath + " | " + logPath);
}

void MainWindow::appendLog(const QString& text) {
    const QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss] ");
    logEdit_->appendPlainText(timestamp + text);
}

void MainWindow::appendReceivedLine(const QString& text) {
    const QString timestamp = QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss] ");
    receivedEdit_->appendPlainText(timestamp + text);
}

void MainWindow::writeLogHeader(QTextStream& stream, const char* title) const {
    const auto txConfig = readConfig();
    const auto rxConfig = readRxConfig();

    stream << title << '\n';
    stream << "Generated at: " << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    stream << "HFText version: " << hftext::kVersion << '\n';
    stream << "Release track: " << hftext::kReleaseTrack << '\n';
    stream << "Protocol: " << hftext::kProtocolVersion << '\n';
    stream << "Callsign: " << callsignEdit_->text().trimmed() << '\n';
    stream << "Speed profile: " << speedProfileDescription(selectedSpeedProfileKey()) << '\n';
    stream << "Modem config file: " << modemConfigPath_ << '\n';
    stream << "Modulation: " << modulationModeName(txConfig.modulationMode) << '\n';
    stream << "Sample rate TX/WAV: " << txConfig.sampleRate << " Hz\n";
    stream << "Sample rate RX: " << rxConfig.sampleRate << " Hz\n";
    stream << "Symbol duration: " << QString::number(txConfig.symbolDurationSec, 'f', 3) << " s\n";
    stream << "Base frequency: " << QString::number(txConfig.frequency0Hz, 'f', 1) << " Hz\n";
    stream << "Tone spacing: " << QString::number(hftext::modulationToneSpacingHz(txConfig), 'f', 1) << " Hz\n";
    stream << "Derived second tone: " << QString::number(txConfig.frequency1Hz, 'f', 1) << " Hz\n";
    stream << "Amplitude: " << QString::number(txConfig.amplitude, 'f', 2) << '\n';
    stream << "Preamble: " << txConfig.preambleBits << " bits\n";
    stream << "Audio output: " << outputDeviceCombo_->currentText() << '\n';
    stream << "Audio input: " << inputDeviceCombo_->currentText() << '\n';
    stream << "Detailed RX log: " << (detailedRxLogCheck_->isChecked() ? "yes" : "no") << '\n';
    if (rxDiagnosticLabel_ != nullptr) {
        stream << "Current RX state: " << rxDiagnosticLabel_->text() << '\n';
    }
    if (rxSessionLabel_ != nullptr) {
        stream << "Current RX session: " << rxSessionLabel_->text() << '\n';
    }
    const auto currentPendingSamples = rxPendingSampleCount_.load(std::memory_order_relaxed);
    const auto maxPendingSamples = rxMaxObservedPendingSamples_.load(std::memory_order_relaxed);
    const auto droppedSamples = rxDroppedSamples_.load(std::memory_order_relaxed);
    stream << "RX worker pending: current " << sampleDurationText(currentPendingSamples, rxConfig.sampleRate)
           << ", peak " << sampleDurationText(maxPendingSamples, rxConfig.sampleRate)
           << ", dropped " << sampleDurationText(droppedSamples, rxConfig.sampleRate)
           << " (" << static_cast<qulonglong>(droppedSamples) << " samples)\n";
    stream << "Stored accepted frames: " << static_cast<qulonglong>(acceptedRxFrames_.size()) << '\n';
}

void MainWindow::writeFieldSummaryCsv(
    QTextStream& stream,
    const QString& wavPath,
    std::size_t sampleCount,
    int sampleRate
) const {
    const auto txConfig = readConfig();
    const auto rxConfig = readRxConfig();
    const auto elapsedMsecs = rxSessionStartedAtMsecs_ <= 0
        ? 0
        : QDateTime::currentMSecsSinceEpoch() - rxSessionStartedAtMsecs_;
    const double elapsedSeconds = static_cast<double>(elapsedMsecs) / 1000.0;
    const double savedSeconds = sampleRate <= 0
        ? 0.0
        : static_cast<double>(sampleCount) / static_cast<double>(sampleRate);
    const QString receivedText = receivedEdit_ == nullptr ? QString() : receivedEdit_->toPlainText().trimmed();
    const int receivedLines = receivedText.isEmpty()
        ? 0
        : receivedText.split('\n', Qt::SkipEmptyParts).size();

    stream << "--- Summary CSV ---\n";
    stream
        << "generated_at,hftext_version,release_track,protocol,callsign,modulation,"
        << "speed_profile,modem_config_file,symbol_duration_s,tx_sample_rate_hz,rx_sample_rate_hz,"
        << "f0_hz,f1_hz,tone_spacing_hz,amplitude,preamble_bits,detailed_log,"
        << "rx_elapsed_s,rx_accepted,accepted_frames,rx_rejected_strong,rx_phys_length,rx_sync,"
        << "rx_quality,last_phys_length,last_reject,received_lines,received_text,"
        << "accepted_length,accepted_offset_samples,accepted_offsets_tried,"
        << "rx_pending_current_s,rx_pending_peak_s,rx_pending_dropped_s,rx_pending_dropped_samples,"
        << "saved_audio_s,saved_samples,wav_path\n";
    const QString summaryQuality = hasLastAcceptedRx_ ? lastAcceptedRxQualityText_ : lastRxQualityText_;
    const QString summaryPhysicalLength = hasLastAcceptedRx_
        ? QString("%1 symbols").arg(lastAcceptedRxLength_)
        : lastRxPhysicalLengthText_;
    stream
        << csvCell(QDateTime::currentDateTime().toString(Qt::ISODate)) << ','
        << csvCell(QString::fromLatin1(hftext::kVersion)) << ','
        << csvCell(QString::fromLatin1(hftext::kReleaseTrack)) << ','
        << csvCell(QString::fromLatin1(hftext::kProtocolVersion)) << ','
        << csvCell(callsignEdit_->text().trimmed()) << ','
        << csvCell(modulationModeName(txConfig.modulationMode)) << ','
        << csvCell(selectedSpeedProfileKey()) << ','
        << csvCell(modemConfigPath_) << ','
        << QString::number(txConfig.symbolDurationSec, 'f', 3) << ','
        << txConfig.sampleRate << ','
        << rxConfig.sampleRate << ','
        << QString::number(txConfig.frequency0Hz, 'f', 1) << ','
        << QString::number(txConfig.frequency1Hz, 'f', 1) << ','
        << QString::number(hftext::modulationToneSpacingHz(txConfig), 'f', 1) << ','
        << QString::number(txConfig.amplitude, 'f', 2) << ','
        << txConfig.preambleBits << ','
        << csvBool(detailedRxLogCheck_->isChecked()) << ','
        << QString::number(elapsedSeconds, 'f', 2) << ','
        << rxSessionAcceptedCount_ << ','
        << static_cast<qulonglong>(acceptedRxFrames_.size()) << ','
        << rxSessionRejectedCount_ << ','
        << rxSessionLengthCount_ << ','
        << rxSessionSyncCount_ << ','
        << csvCell(summaryQuality) << ','
        << csvCell(summaryPhysicalLength) << ','
        << csvCell(lastRxRejectText_) << ','
        << receivedLines << ','
        << csvCell(receivedText) << ','
        << lastAcceptedRxLength_ << ','
        << lastAcceptedRxOffsetSamples_ << ','
        << lastAcceptedRxOffsetsTried_ << ','
        << QString::number(
            static_cast<double>(rxPendingSampleCount_.load(std::memory_order_relaxed))
                / static_cast<double>((std::max)(1, rxConfig.sampleRate)),
            'f',
            2
        ) << ','
        << QString::number(
            static_cast<double>(rxMaxObservedPendingSamples_.load(std::memory_order_relaxed))
                / static_cast<double>((std::max)(1, rxConfig.sampleRate)),
            'f',
            2
        ) << ','
        << QString::number(
            static_cast<double>(rxDroppedSamples_.load(std::memory_order_relaxed))
                / static_cast<double>((std::max)(1, rxConfig.sampleRate)),
            'f',
            2
        ) << ','
        << static_cast<qulonglong>(rxDroppedSamples_.load(std::memory_order_relaxed)) << ','
        << QString::number(savedSeconds, 'f', 2) << ','
        << static_cast<qulonglong>(sampleCount) << ','
        << csvCell(wavPath) << '\n';
}

void MainWindow::writeAcceptedRxFramesCsv(QTextStream& stream) const {
    stream << "--- Accepted Frames CSV ---\n";
    stream
        << "accepted_at,rx_elapsed_s,modulation,symbol_duration_s,sample_rate_hz,"
        << "f0_hz,f1_hz,tone_spacing_hz,amplitude,preamble_bits,length,quality,"
        << "offset_samples,offsets_tried,text\n";

    for (const auto& frame : acceptedRxFrames_) {
        stream
            << csvCell(frame.acceptedAtIso) << ','
            << QString::number(frame.elapsedSeconds, 'f', 2) << ','
            << csvCell(modulationModeName(frame.config.modulationMode)) << ','
            << QString::number(frame.config.symbolDurationSec, 'f', 3) << ','
            << frame.config.sampleRate << ','
            << QString::number(frame.config.frequency0Hz, 'f', 1) << ','
            << QString::number(frame.config.frequency1Hz, 'f', 1) << ','
            << QString::number(hftext::modulationToneSpacingHz(frame.config), 'f', 1) << ','
            << QString::number(frame.config.amplitude, 'f', 2) << ','
            << frame.config.preambleBits << ','
            << frame.length << ','
            << csvCell(frame.qualityText) << ','
            << frame.offsetSamples << ','
            << frame.offsetsTried << ','
            << csvCell(frame.text) << '\n';
    }
}

void MainWindow::resetRxDiagnostic(const QString& state) {
    lastRxPhysicalLengthText_ = "--";
    lastRxQualityText_ = "--";
    lastRxRejectText_ = "--";
    setRxDiagnosticText(state);
}

void MainWindow::resetRxFrameProgress() {
    rxProgressSyncSample_ = -1;
    rxDisplayedFrameProgressPermille_ = 0;
    if (rxFrameProgressBar_ != nullptr) {
        rxFrameProgressBar_->setValue(0);
    }
}

void MainWindow::updateRxFrameProgressFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    const auto selection = hftext::selectRxEvents(events);
    const auto* bestDecoded = selectedRxEvent(events, selection.bestDecoded);
    const auto* bestWaiting = selectedRxEvent(events, selection.bestWaiting);
    const auto* bestLength = selectedRxEvent(events, selection.bestLength);
    const auto* bestRejected = selectedRxEvent(events, selection.bestRejected);

    if (bestDecoded != nullptr) {
        rxProgressSyncSample_ = -1;
        rxDisplayedFrameProgressPermille_ = 1000;
        rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
        return;
    }

    if (bestWaiting != nullptr) {
        if (rxProgressSyncSample_ != bestWaiting->syncSample) {
            rxProgressSyncSample_ = bestWaiting->syncSample;
            rxDisplayedFrameProgressPermille_ = 0;
        }
        const int progress = hftext::frameProgressPermille(*bestWaiting);
        rxDisplayedFrameProgressPermille_ = std::max(rxDisplayedFrameProgressPermille_, progress);
        rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
        return;
    }

    if (bestLength != nullptr) {
        if (rxProgressSyncSample_ != bestLength->syncSample) {
            rxProgressSyncSample_ = bestLength->syncSample;
            rxDisplayedFrameProgressPermille_ = 0;
            rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
        }
        return;
    }

    if (bestRejected != nullptr) {
        rxProgressSyncSample_ = -1;
        rxDisplayedFrameProgressPermille_ = 0;
        rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
    }
}

void MainWindow::updateRxDiagnosticFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    const auto selection = hftext::selectRxEvents(events);
    const auto* bestDecoded = selectedRxEvent(events, selection.bestDecoded);
    const auto* bestWaiting = selectedRxEvent(events, selection.bestWaiting);
    const auto* bestLength = selectedRxEvent(events, selection.bestLength);
    const auto* bestSync = selectedRxEvent(events, selection.bestSync);
    const auto* bestRejected = selectedRxEvent(events, selection.bestRejected);

    if (bestRejected != nullptr) {
        lastRxRejectText_ = frameRejectionReason(*bestRejected);
        lastRxQualityText_ = formatConfidence(bestRejected->confidence);
    }

    QString state;
    const hftext::StreamingReceiverEvent* displayEvent = nullptr;
    if (bestDecoded != nullptr) {
        displayEvent = bestDecoded;
        lastRxRejectText_ = "--";
        state = "Valid frame";
    } else if (bestWaiting != nullptr) {
        displayEvent = bestWaiting;
        state = QString("Receiving frame %1%").arg(hftext::frameProgressPercent(*bestWaiting));
    } else if (bestLength != nullptr) {
        displayEvent = bestLength;
        state = "PHYS_LENGTH recovered";
    } else if (bestSync != nullptr) {
        displayEvent = bestSync;
        state = QString("Sync detected (%1 error(s))").arg(bestSync->syncMismatches);
    } else if (bestRejected != nullptr) {
        displayEvent = bestRejected;
        state = "Candidate rejected";
    } else if (selection.hasInvalidLength) {
        if (hasRecentAcceptedRx()) {
            return;
        }
        lastRxRejectText_ = "invalid PHYS_LENGTH";
        state = "Invalid physical length";
    }

    if (displayEvent != nullptr && displayEvent->payloadLength >= 0) {
        lastRxPhysicalLengthText_ = QString("%1 symbols, %2 bits")
            .arg(displayEvent->payloadLength)
            .arg(displayEvent->bitsExpected);
        if (displayEvent->type == hftext::StreamingReceiverEventType::FrameDecoded
            || displayEvent->type == hftext::StreamingReceiverEventType::FrameRejected) {
            lastRxQualityText_ = formatConfidence(displayEvent->confidence);
        }
    }

    if (!state.isEmpty()) {
        setRxDiagnosticText(state);
    }
}

void MainWindow::setRxDiagnosticText(const QString& state) {
    if (rxDiagnosticLabel_ == nullptr) {
        return;
    }
    rxDiagnosticLabel_->setText(
        QString("%1 | PHYS_LENGTH: %2 | Quality: %3 | Last: %4")
            .arg(state)
            .arg(lastRxPhysicalLengthText_)
            .arg(lastRxQualityText_)
            .arg(lastRxRejectText_)
    );
}

void MainWindow::resetRxSessionCounters() {
    rxSessionSyncCount_ = 0;
    rxSessionLengthCount_ = 0;
    rxSessionRejectedCount_ = 0;
    rxSessionAcceptedCount_ = 0;
    rxSessionStartedAtMsecs_ = 0;
    hasLastAcceptedRx_ = false;
    lastAcceptedRxQualityText_ = "--";
    lastAcceptedRxLength_ = -1;
    lastAcceptedRxOffsetSamples_ = 0;
    lastAcceptedRxOffsetsTried_ = 0;
    lastAcceptedRxAtMsecs_ = 0;
    acceptedRxFrames_.clear();
    setRxSessionText();
}

void MainWindow::updateRxSessionFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    const auto counts = hftext::rxSessionEventCounts(events);
    rxSessionSyncCount_ += counts.sync;
    rxSessionLengthCount_ += counts.length;
    rxSessionRejectedCount_ += counts.rejected;
    setRxSessionText();
}

void MainWindow::setRxSessionText() {
    if (rxSessionLabel_ == nullptr) {
        return;
    }
    const auto elapsed = rxSessionStartedAtMsecs_ <= 0
        ? 0
        : QDateTime::currentMSecsSinceEpoch() - rxSessionStartedAtMsecs_;
    rxSessionLabel_->setText(
        QString("%1 | accepted %2 | rejected %3 | PHYS_LENGTH %4 | sync %5")
            .arg(formatElapsedTime(elapsed))
            .arg(rxSessionAcceptedCount_)
            .arg(rxSessionRejectedCount_)
            .arg(rxSessionLengthCount_)
            .arg(rxSessionSyncCount_)
    );
}

void MainWindow::rememberAcceptedRx(const hftext::DecodeResult& result, const hftext::ModemConfig& config) {
    if (!(result.frameDetected && result.crcOk && result.payloadValid)) {
        return;
    }

    const auto nowMsecs = QDateTime::currentMSecsSinceEpoch();
    hasLastAcceptedRx_ = true;
    lastAcceptedRxQualityText_ = formatConfidence(result.confidence);
    lastAcceptedRxLength_ = result.length;
    lastAcceptedRxOffsetSamples_ = result.startOffset;
    lastAcceptedRxOffsetsTried_ = result.offsetsTried;
    lastAcceptedRxAtMsecs_ = nowMsecs;
    lastRxPhysicalLengthText_ = QString("%1 symbols").arg(result.length);
    lastRxQualityText_ = "CRC OK, " + lastAcceptedRxQualityText_;
    lastRxRejectText_ = "--";

    const auto elapsedMsecs = rxSessionStartedAtMsecs_ <= 0
        ? 0
        : nowMsecs - rxSessionStartedAtMsecs_;

    AcceptedRxFrame frame;
    frame.acceptedAtIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    frame.elapsedSeconds = static_cast<double>(elapsedMsecs) / 1000.0;
    frame.text = QString::fromStdString(result.text);
    frame.config = config;
    frame.qualityText = lastAcceptedRxQualityText_;
    frame.length = result.length;
    frame.offsetSamples = result.startOffset;
    frame.offsetsTried = result.offsetsTried;
    acceptedRxFrames_.push_back(frame);
    if (acceptedRxFrames_.size() > kMaxAcceptedRxSnapshots) {
        acceptedRxFrames_.erase(acceptedRxFrames_.begin());
    }
}

bool MainWindow::hasRecentAcceptedRx() const {
    if (lastAcceptedRxAtMsecs_ <= 0) {
        return false;
    }

    const auto elapsed = QDateTime::currentMSecsSinceEpoch() - lastAcceptedRxAtMsecs_;
    return elapsed >= 0 && elapsed <= kAcceptedRxStateHoldSeconds * 1000;
}

void MainWindow::setTransmitButtonTransmitting(bool transmitting) {
    if (transmitButton_ == nullptr) {
        return;
    }

    transmitButton_->setIcon(style()->standardIcon(transmitting ? QStyle::SP_MediaStop : QStyle::SP_ArrowForward));
    transmitButton_->setToolTip(transmitting ? "Stop TX" : "Send");
}

void MainWindow::setReceiveControlsRecording(bool recording) {
    if (startReceiveButton_ != nullptr) {
        const bool hasInputDevice = inputDeviceCombo_ != nullptr && inputDeviceCombo_->isEnabled();
        startReceiveButton_->setEnabled(!recording && hasInputDevice);
    }
    if (stopReceiveButton_ != nullptr) {
        stopReceiveButton_->setEnabled(recording);
    }
}

QString MainWindow::selectedSpeedProfileKey() const {
    if (speedProfileCombo_ == nullptr) {
        return "slow";
    }
    const QString key = speedProfileCombo_->currentData().toString().trimmed().toLower();
    return key == "fast" ? "fast" : "slow";
}

QString MainWindow::speedProfileDescription(const QString& profileKey) const {
    return QString::fromStdString(
        hftext::speedProfileDisplayName(
            modemProfiles_,
            hftext::speedProfileFromKey(profileKey.toStdString())
        )
    );
}

bool MainWindow::writeDefaultModemConfigFile() const {
    QFile file(modemConfigPath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    const auto defaults = hftext::defaultAppModemProfiles();
    QTextStream stream(&file);
    stream
        << "; HFText modem configuration.\n"
        << "; Edit this file for debug or field experiments, then restart HFText or press Load defaults to recreate it.\n"
        << "; Supported modulation values: 2fsk, 4fsk, 8fsk.\n\n"
        << "[common]\n"
        << "tx_sample_rate_hz=" << defaults.txSampleRate << '\n'
        << "rx_sample_rate_hz=" << defaults.rxSampleRate << '\n'
        << "base_frequency_hz=" << QString::number(defaults.baseFrequencyHz, 'f', 1) << '\n'
        << "tone_spacing_hz=" << QString::number(defaults.toneSpacingHz, 'f', 1) << '\n'
        << "amplitude=" << QString::number(defaults.amplitude, 'f', 2) << '\n'
        << "preamble_bits=" << defaults.preambleBits << "\n\n"
        << "[slow]\n"
        << "modulation=" << modulationModeIniName(defaults.slow.modulationMode) << '\n'
        << "symbol_duration_s=" << QString::number(defaults.slow.symbolDurationSec, 'f', 3) << "\n\n"
        << "[fast]\n"
        << "modulation=" << modulationModeIniName(defaults.fast.modulationMode) << '\n'
        << "symbol_duration_s=" << QString::number(defaults.fast.symbolDurationSec, 'f', 3) << '\n';

    return true;
}

void MainWindow::loadModemConfigFile() {
    modemConfigPath_ = QDir(QCoreApplication::applicationDirPath()).filePath("hftext.ini");
    modemConfigWarning_.clear();

    if (!QFile::exists(modemConfigPath_) && !writeDefaultModemConfigFile()) {
        modemConfigWarning_ = "could not create hftext.ini; using built-in defaults";
    }

    modemProfiles_ = hftext::defaultAppModemProfiles();

    QSettings ini(modemConfigPath_, QSettings::IniFormat);
    modemProfiles_.txSampleRate = ini.value("common/tx_sample_rate_hz", modemProfiles_.txSampleRate).toInt();
    modemProfiles_.rxSampleRate = ini.value("common/rx_sample_rate_hz", modemProfiles_.rxSampleRate).toInt();
    modemProfiles_.baseFrequencyHz = ini.value("common/base_frequency_hz", modemProfiles_.baseFrequencyHz).toFloat();
    modemProfiles_.toneSpacingHz = ini.value("common/tone_spacing_hz", modemProfiles_.toneSpacingHz).toFloat();
    modemProfiles_.amplitude = ini.value("common/amplitude", modemProfiles_.amplitude).toFloat();
    modemProfiles_.preambleBits = ini.value("common/preamble_bits", modemProfiles_.preambleBits).toInt();
    modemProfiles_.slow.modulationMode = modulationModeFromIniValue(
        ini.value("slow/modulation", modulationModeIniName(modemProfiles_.slow.modulationMode)).toString(),
        modemProfiles_.slow.modulationMode
    );
    modemProfiles_.fast.modulationMode = modulationModeFromIniValue(
        ini.value("fast/modulation", modulationModeIniName(modemProfiles_.fast.modulationMode)).toString(),
        modemProfiles_.fast.modulationMode
    );
    modemProfiles_.slow.symbolDurationSec =
        ini.value("slow/symbol_duration_s", modemProfiles_.slow.symbolDurationSec).toFloat();
    modemProfiles_.fast.symbolDurationSec =
        ini.value("fast/symbol_duration_s", modemProfiles_.fast.symbolDurationSec).toFloat();
}

hftext::ModemConfig MainWindow::configForSpeedProfile(const QString& profileKey, int sampleRate) const {
    return hftext::modemConfigForProfile(
        modemProfiles_,
        hftext::speedProfileFromKey(profileKey.toStdString()),
        sampleRate
    );
}

void MainWindow::loadSettings() {
    QSettings settings("HFText", "HFText");

    restoreGeometry(settings.value("windowGeometry").toByteArray());
    callsignEdit_->setText(settings.value("callsign", callsignEdit_->text()).toString());
    const QString savedSpeed = settings.value("speedProfile", "slow").toString().toLower();
    const int speedIndex = speedProfileCombo_->findData(savedSpeed == "fast" ? "fast" : "slow");
    if (speedIndex >= 0) {
        speedProfileCombo_->setCurrentIndex(speedIndex);
    }
    detailedRxLogCheck_->setChecked(settings.value("detailedRxLog", detailedRxLogCheck_->isChecked()).toBool());
    selectComboText(outputDeviceCombo_, settings.value("outputDevice").toString());
    selectComboText(inputDeviceCombo_, settings.value("inputDevice").toString());
}

void MainWindow::saveSettings() const {
    QSettings settings("HFText", "HFText");

    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("callsign", callsignEdit_->text());
    settings.setValue("speedProfile", selectedSpeedProfileKey());
    settings.setValue("detailedRxLog", detailedRxLogCheck_->isChecked());
    settings.setValue("outputDevice", outputDeviceCombo_->currentText());
    settings.setValue("inputDevice", inputDeviceCombo_->currentText());
}

void MainWindow::populateInputDevices() {
    inputDeviceCombo_->clear();
    const auto devices = audioInput_.devices();
    if (devices.empty()) {
        inputDeviceCombo_->addItem("No device found", 0U);
        inputDeviceCombo_->setEnabled(false);
        return;
    }

    for (const auto& device : devices) {
        inputDeviceCombo_->addItem(QString::fromStdString(device.name), device.id);
    }
}

void MainWindow::populateOutputDevices() {
    outputDeviceCombo_->clear();
    const auto devices = audioOutput_.devices();
    if (devices.empty()) {
        outputDeviceCombo_->addItem("No device found", 0U);
        outputDeviceCombo_->setEnabled(false);
        return;
    }

    for (const auto& device : devices) {
        outputDeviceCombo_->addItem(QString::fromStdString(device.name), device.id);
    }
}

void MainWindow::startRxWorker(const hftext::ModemConfig& config, bool detailedRxLog) {
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        rxPendingSamples_.clear();
        rxMaxPendingSamples_ = static_cast<std::size_t>(
            std::max(config.sampleRate / 2, config.sampleRate * kRxPendingSeconds)
        );
        rxMaxWorkerChunkSamples_ = static_cast<std::size_t>(
            std::max(config.sampleRate / 10, config.sampleRate * kRxWorkerChunkMilliseconds / 1000)
        );
        rxPendingSampleCount_.store(0, std::memory_order_relaxed);
        rxMaxObservedPendingSamples_.store(0, std::memory_order_relaxed);
        rxDroppedSamples_.store(0, std::memory_order_relaxed);
        rxWorkerStop_ = false;
    }
    rxWorker_ = std::thread(&MainWindow::rxWorkerLoop, this, config, detailedRxLog);
}

void MainWindow::stopRxWorker(bool drainPending) {
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        rxWorkerStop_ = true;
        if (!drainPending) {
            rxPendingSamples_.clear();
            rxPendingSampleCount_.store(0, std::memory_order_relaxed);
        }
    }
    rxCondition_.notify_all();
    if (rxWorker_.joinable()) {
        rxWorker_.join();
    }
}

void MainWindow::enqueueRxSamples(const std::vector<float>& samples) {
    if (samples.empty()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        if (rxWorkerStop_) {
            return;
        }
        rxPendingSamples_.insert(rxPendingSamples_.end(), samples.begin(), samples.end());
        updateAtomicMax(rxMaxObservedPendingSamples_, rxPendingSamples_.size());
        if (rxMaxPendingSamples_ > 0 && rxPendingSamples_.size() > rxMaxPendingSamples_) {
            const auto excess = rxPendingSamples_.size() - rxMaxPendingSamples_;
            rxDroppedSamples_.fetch_add(excess, std::memory_order_relaxed);
            rxPendingSamples_.erase(
                rxPendingSamples_.begin(),
                rxPendingSamples_.begin() + static_cast<std::ptrdiff_t>(excess)
            );
        }
        rxPendingSampleCount_.store(rxPendingSamples_.size(), std::memory_order_relaxed);
    }
    rxCondition_.notify_one();
}

void MainWindow::clearRxEvidenceSamples(int sampleRate) {
    std::lock_guard<std::mutex> lock(rxEvidenceMutex_);
    rxEvidenceSamples_.clear();
    rxEvidenceSampleRate_ = sampleRate;
}

void MainWindow::appendRxEvidenceSamples(const std::vector<float>& samples, int sampleRate) {
    if (samples.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(rxEvidenceMutex_);
    if (sampleRate != rxEvidenceSampleRate_) {
        rxEvidenceSamples_.clear();
        rxEvidenceSampleRate_ = sampleRate;
    }

    rxEvidenceSamples_.insert(rxEvidenceSamples_.end(), samples.begin(), samples.end());
    const auto maxSamples = static_cast<std::size_t>(std::max(1, sampleRate * kRxEvidenceSeconds));
    if (rxEvidenceSamples_.size() > maxSamples) {
        const auto excess = rxEvidenceSamples_.size() - maxSamples;
        rxEvidenceSamples_.erase(
            rxEvidenceSamples_.begin(),
            rxEvidenceSamples_.begin() + static_cast<std::ptrdiff_t>(excess)
        );
    }
}

void MainWindow::rxWorkerLoop(hftext::ModemConfig config, bool detailedRxLog) {
    hftext::StreamingReceiver receiver(config);
    std::vector<QString> emittedNormalRxStatusLines;

    while (true) {
        std::vector<float> chunk;
        {
            std::unique_lock<std::mutex> lock(rxMutex_);
            rxCondition_.wait(lock, [this] {
                return rxWorkerStop_ || !rxPendingSamples_.empty();
            });

            if (rxWorkerStop_ && rxPendingSamples_.empty()) {
                break;
            }

            const auto maxChunk = rxMaxWorkerChunkSamples_ == 0
                ? rxPendingSamples_.size()
                : std::min(rxPendingSamples_.size(), rxMaxWorkerChunkSamples_);
            chunk.assign(rxPendingSamples_.begin(), rxPendingSamples_.begin() + static_cast<std::ptrdiff_t>(maxChunk));
            rxPendingSamples_.erase(
                rxPendingSamples_.begin(),
                rxPendingSamples_.begin() + static_cast<std::ptrdiff_t>(maxChunk)
            );
            rxPendingSampleCount_.store(rxPendingSamples_.size(), std::memory_order_relaxed);
        }

        const auto results = receiver.pushSamples(chunk);
        const auto events = receiver.takeEvents();
        if (!events.empty()) {
            const int frameQuality = hftext::rxQualityPermille(events);
            const bool hasTerminalCandidate = hftext::hasTerminalRxCandidate(events);
            auto lines = formatStreamingEvents(
                events,
                detailedRxLog,
                detailedRxLog ? kMaxDetailedRxLogLinesPerBatch : -1
            );
            if (!detailedRxLog) {
                lines = filterRepeatedNormalRxStatus(std::move(lines), emittedNormalRxStatusLines);
            }
            QMetaObject::invokeMethod(
                this,
                [
                    this,
                    events,
                    lines = std::move(lines),
                    frameQuality
                ]() {
                    updateRxFrameProgressFromEvents(events);
                    if (frameQuality >= 0) {
                        rxQualityBar_->setValue(frameQuality);
                    }
                    updateRxDiagnosticFromEvents(events);
                    updateRxSessionFromEvents(events);
                    for (const auto& line : lines) {
                        appendLog(line);
                    }
                },
                Qt::QueuedConnection
            );
            if (hasTerminalCandidate) {
                emittedNormalRxStatusLines.clear();
            }
        }
        if (!results.empty()) {
            emittedNormalRxStatusLines.clear();
        }
        for (const auto& result : results) {
            QMetaObject::invokeMethod(
                this,
                [this, result, config]() {
                    const bool resultAccepted = result.crcOk && result.payloadValid;
                    rxProgressSyncSample_ = -1;
                    rxDisplayedFrameProgressPermille_ = 1000;
                    rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
                    if (resultAccepted) {
                        rememberAcceptedRx(result, config);
                    }
                    showDecodeResult(result);
                    if (resultAccepted) {
                        ++rxSessionAcceptedCount_;
                        setRxSessionText();
                        appendLog("RX text: " + QString::fromStdString(result.text));
                    }
                    if (resultAccepted) {
                        lastRxQualityText_ = "CRC OK, " + formatConfidence(result.confidence);
                        lastRxRejectText_ = "--";
                        setRxDiagnosticText("Message accepted");
                    } else {
                        lastRxQualityText_ = formatConfidence(result.confidence);
                    }
                    appendLog(
                        QString(resultAccepted ? "RX: message accepted, offset " : "RX: decoded result rejected, offset ")
                        + QString::number(result.startOffset)
                        + " samples, phases " + QString::number(result.offsetsTried)
                        + ", confidence: " + formatConfidence(result.confidence)
                        + (resultAccepted ? ", CRC OK" : "")
                    );
                },
                Qt::QueuedConnection
            );
        }
    }
}

hftext::ModemConfig MainWindow::readConfig() const {
    return configForSpeedProfile(selectedSpeedProfileKey(), modemProfiles_.txSampleRate);
}

hftext::ModemConfig MainWindow::readRxConfig() const {
    return configForSpeedProfile(selectedSpeedProfileKey(), modemProfiles_.rxSampleRate);
}

void MainWindow::showDecodeResult(const hftext::DecodeResult& result) {
    rxQualityBar_->setValue(static_cast<int>(std::clamp(result.confidence, 0.0F, 1.0F) * 1000.0F));
    if (!result.frameDetected) {
        appendReceivedLine(QStringLiteral("Frame not detected: ") + QString::fromStdString(result.error));
        return;
    }
    if (!result.crcOk) {
        appendReceivedLine("Frame detected, but CRC is invalid.");
        return;
    }
    if (!result.payloadValid) {
        appendReceivedLine("Frame detected, CRC is valid, but payload is invalid.");
        return;
    }
    appendReceivedLine(QString::fromStdString(result.text));
}
