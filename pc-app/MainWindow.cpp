#include "MainWindow.h"

#include "hftext_encoder.h"
#include "wav_io.h"

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
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
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
#include <cmath>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr int kMaxLogBlocks = 3000;
constexpr int kRxPendingSeconds = 3;
constexpr int kRxWorkerChunkMilliseconds = 500;
constexpr int kMaxDetailedRxLogLinesPerBatch = 80;
constexpr int kRxEvidenceSeconds = 300;
constexpr int kMaxAcceptedRxSnapshots = 512;
constexpr int kDefaultSampleRate = 48000;
constexpr double kDefaultSymbolDurationSec = 0.5;
constexpr double kDefaultBaseFrequencyHz = 1200.0;
constexpr double kDefaultToneSpacingHz = 400.0;
constexpr double kDefaultAmplitude = 0.80;
constexpr int kDefaultPreambleBits = 64;

void validateTonesBelowNyquist(const hftext::ModemConfig& config) {
    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    if (hftext::highestModulationToneHz(config) >= nyquistHz) {
        throw std::invalid_argument("tones must stay below half the sample rate");
    }
}

hftext::ModulationMode modulationModeFromCombo(const QComboBox* combo) {
    if (combo == nullptr) {
        return hftext::ModulationMode::Fsk2;
    }
    if (combo->currentData().toInt() == 8) {
        return hftext::ModulationMode::Fsk8;
    }
    if (combo->currentData().toInt() == 4) {
        return hftext::ModulationMode::Fsk4;
    }
    return hftext::ModulationMode::Fsk2;
}

QString modulationModeName(hftext::ModulationMode mode) {
    switch (mode) {
    case hftext::ModulationMode::Fsk8:
        return QStringLiteral("8-FSK exp v0.3");
    case hftext::ModulationMode::Fsk4:
        return QStringLiteral("4-FSK exp v0.2");
    case hftext::ModulationMode::Fsk2:
    default:
        return QStringLiteral("2-FSK v0.1");
    }
}

QString audioPeakPercent(const std::vector<float>& samples) {
    float peak = 0.0F;
    for (const float sample : samples) {
        peak = (std::max)(peak, std::abs(sample));
    }
    return QString::number(peak * 100.0F, 'f', 1) + "%";
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

double clippingPercent(std::size_t clippedSamples, std::size_t sampleCount) {
    if (sampleCount == 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(clippedSamples) / static_cast<double>(sampleCount);
}

QString formatClippingSummary(std::size_t clippedSamples, std::size_t sampleCount) {
    return QString::number(static_cast<qulonglong>(clippedSamples))
        + " samples (" + QString::number(clippingPercent(clippedSamples, sampleCount), 'f', 4) + "%)";
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

    const double percent = clippingPercent(clippedSamples, sampleCount);
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

bool isStrongSyncEvent(const hftext::StreamingReceiverEvent& event) {
    return event.syncMismatches <= 4;
}

bool isBetterSyncEvent(
    const hftext::StreamingReceiverEvent& candidate,
    const hftext::StreamingReceiverEvent* current
) {
    if (current == nullptr) {
        return true;
    }
    if (candidate.syncMismatches != current->syncMismatches) {
        return candidate.syncMismatches < current->syncMismatches;
    }
    return candidate.confidence > current->confidence;
}

bool isBetterWaitingEvent(
    const hftext::StreamingReceiverEvent& candidate,
    const hftext::StreamingReceiverEvent* current
) {
    if (current == nullptr) {
        return true;
    }
    const double candidateProgress = candidate.bitsExpected <= 0
        ? 0.0
        : static_cast<double>(candidate.bitsAvailable) / static_cast<double>(candidate.bitsExpected);
    const double currentProgress = current->bitsExpected <= 0
        ? 0.0
        : static_cast<double>(current->bitsAvailable) / static_cast<double>(current->bitsExpected);
    if (candidateProgress != currentProgress) {
        return candidateProgress > currentProgress;
    }
    return candidate.syncMismatches < current->syncMismatches;
}

QString normalRxStatusKey(const QString& line) {
    if (line.startsWith("RX: strong sync")) {
        return line;
    }
    if (line.startsWith("RX: frame ")) {
        return line;
    }
    if (line.contains("strong candidate(s) rejected")) {
        return "RX: strong candidates rejected";
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

int rxQualityPermille(const std::vector<hftext::StreamingReceiverEvent>& events) {
    int bestQuality = -1;
    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::FrameDecoded:
        case hftext::StreamingReceiverEventType::FrameRejected:
            bestQuality = std::max(
                bestQuality,
                static_cast<int>(std::clamp(event.confidence, 0.0F, 1.0F) * 1000.0F)
            );
            break;
        case hftext::StreamingReceiverEventType::SyncFound:
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        case hftext::StreamingReceiverEventType::FrameWaiting:
            break;
        }
    }
    return bestQuality;
}

struct RxSessionEventCounts {
    int sync = 0;
    int length = 0;
    int rejected = 0;
};

RxSessionEventCounts rxSessionEventCounts(const std::vector<hftext::StreamingReceiverEvent>& events) {
    RxSessionEventCounts counts;
    const hftext::StreamingReceiverEvent* bestLength = nullptr;
    const hftext::StreamingReceiverEvent* bestSync = nullptr;
    const hftext::StreamingReceiverEvent* bestRejected = nullptr;

    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::FrameDecoded:
            break;
        case hftext::StreamingReceiverEventType::FrameRejected:
            if (isStrongSyncEvent(event) && (bestRejected == nullptr || event.confidence > bestRejected->confidence)) {
                bestRejected = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestLength)) {
                bestLength = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::SyncFound:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestSync)) {
                bestSync = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        case hftext::StreamingReceiverEventType::FrameWaiting:
            break;
        }
    }

    if (bestLength != nullptr) {
        counts.sync = 1;
        counts.length = 1;
    } else if (bestSync != nullptr) {
        counts.sync = 1;
    }
    if (bestRejected != nullptr) {
        counts.rejected = 1;
    }

    return counts;
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
                QString("RX: %1 evento(s) detalhado(s) omitido(s) para manter a interface responsiva.")
                    .arg(static_cast<qulonglong>(events.size() - limit))
            );
        }
        return lines;
    }

    const hftext::StreamingReceiverEvent* bestDecoded = nullptr;
    const hftext::StreamingReceiverEvent* bestLength = nullptr;
    const hftext::StreamingReceiverEvent* bestWaiting = nullptr;
    const hftext::StreamingReceiverEvent* bestSync = nullptr;
    int rejectedCount = 0;

    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::FrameDecoded:
            if (bestDecoded == nullptr || event.confidence > bestDecoded->confidence) {
                bestDecoded = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameRejected:
            if (isStrongSyncEvent(event)) {
                ++rejectedCount;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestLength)) {
                bestLength = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameWaiting:
            if (isStrongSyncEvent(event) && isBetterWaitingEvent(event, bestWaiting)) {
                bestWaiting = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::SyncFound:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestSync)) {
                bestSync = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
            break;
        }
    }

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
        const double progress = bestWaiting->bitsExpected <= 0
            ? 0.0
            : 100.0 * static_cast<double>(bestWaiting->bitsAvailable) / static_cast<double>(bestWaiting->bitsExpected);
        lines.push_back(
            QString("RX: frame %1% (%2/%3 bits)")
                .arg(progress, 0, 'f', 0)
                .arg(bestWaiting->bitsAvailable)
                .arg(bestWaiting->bitsExpected)
        );
    }

    if (rejectedCount > 0) {
        lines.push_back(QString("RX: %1 strong candidate(s) rejected by CRC/payload.").arg(rejectedCount));
    }

    return lines;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("HFText");

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* tabs = new QTabWidget(this);
    auto* operationPage = new QWidget(this);
    auto* operationLayout = new QVBoxLayout(operationPage);
    auto* configPage = new QWidget(this);
    auto* configLayout = new QVBoxLayout(configPage);
    auto* configForm = new QFormLayout();

    receivedEdit_ = new QPlainTextEdit(this);
    receivedEdit_->setReadOnly(true);
    receivedEdit_->setPlaceholderText("Received messages");
    receivedEdit_->setMinimumHeight(180);
    receivedEdit_->setContextMenuPolicy(Qt::CustomContextMenu);
    operationLayout->addWidget(receivedEdit_, 3);

    operationLayout->addWidget(new QLabel("RX waterfall", this));
    waterfallWidget_ = new WaterfallWidget(this);
    operationLayout->addWidget(waterfallWidget_, 2);

    txProgressBar_ = new QProgressBar(this);
    txProgressBar_->setRange(0, 1000);
    txProgressBar_->setValue(0);
    txProgressBar_->setTextVisible(false);
    txProgressBar_->setMaximumHeight(10);
    operationLayout->addWidget(txProgressBar_);

    txEstimateLabel_ = new QLabel(this);
    txEstimateLabel_->setWordWrap(true);
    operationLayout->addWidget(txEstimateLabel_);

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
    callsignEdit_->setText("pu5lrk");
    configForm->addRow("Callsign", callsignEdit_);

    modulationModeCombo_ = new QComboBox(this);
    modulationModeCombo_->addItem("2-FSK robust v0.1", 2);
    modulationModeCombo_->addItem("4-FSK experimental v0.2", 4);
    modulationModeCombo_->addItem("8-FSK experimental v0.3", 8);
    configForm->addRow("Modulation", modulationModeCombo_);

    sampleRateSpin_ = new QSpinBox(this);
    sampleRateSpin_->setRange(8000, 192000);
    sampleRateSpin_->setSingleStep(1000);
    sampleRateSpin_->setSuffix(" Hz");
    sampleRateSpin_->setValue(controller_.config().sampleRate);
    configForm->addRow("Sample rate TX", sampleRateSpin_);

    rxSampleRateSpin_ = new QSpinBox(this);
    rxSampleRateSpin_->setRange(8000, 192000);
    rxSampleRateSpin_->setSingleStep(1000);
    rxSampleRateSpin_->setSuffix(" Hz");
    rxSampleRateSpin_->setValue(48000);
    configForm->addRow("Sample rate RX", rxSampleRateSpin_);

    symbolDurationSpin_ = new QDoubleSpinBox(this);
    symbolDurationSpin_->setRange(0.005, 5.0);
    symbolDurationSpin_->setSingleStep(0.05);
    symbolDurationSpin_->setDecimals(3);
    symbolDurationSpin_->setSuffix(" s");
    symbolDurationSpin_->setValue(controller_.config().symbolDurationSec);
    configForm->addRow("Symbol duration", symbolDurationSpin_);

    frequency0Spin_ = new QDoubleSpinBox(this);
    frequency0Spin_->setRange(20.0, 20000.0);
    frequency0Spin_->setSingleStep(50.0);
    frequency0Spin_->setDecimals(1);
    frequency0Spin_->setSuffix(" Hz");
    frequency0Spin_->setValue(controller_.config().frequency0Hz);
    configForm->addRow("Base frequency", frequency0Spin_);

    frequency1Spin_ = new QDoubleSpinBox(this);
    frequency1Spin_->setRange(1.0, 5000.0);
    frequency1Spin_->setSingleStep(10.0);
    frequency1Spin_->setDecimals(1);
    frequency1Spin_->setSuffix(" Hz");
    frequency1Spin_->setValue(controller_.config().frequency1Hz - controller_.config().frequency0Hz);
    configForm->addRow("Tone spacing", frequency1Spin_);

    amplitudeSpin_ = new QDoubleSpinBox(this);
    amplitudeSpin_->setRange(0.0, 1.0);
    amplitudeSpin_->setSingleStep(0.05);
    amplitudeSpin_->setDecimals(2);
    amplitudeSpin_->setValue(controller_.config().amplitude);
    configForm->addRow("Amplitude", amplitudeSpin_);

    preambleBitsSpin_ = new QSpinBox(this);
    preambleBitsSpin_->setRange(0, 256);
    preambleBitsSpin_->setSingleStep(8);
    preambleBitsSpin_->setSuffix(" bits");
    preambleBitsSpin_->setValue(controller_.config().preambleBits);
    configForm->addRow("Preamble", preambleBitsSpin_);

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
    configForm->addRow("RX level", rxLevelBar_);

    rxFrameProgressBar_ = new QProgressBar(this);
    rxFrameProgressBar_->setRange(0, 1000);
    rxFrameProgressBar_->setValue(0);
    rxFrameProgressBar_->setFormat("%p%");
    configForm->addRow("RX progress", rxFrameProgressBar_);

    rxQualityBar_ = new QProgressBar(this);
    rxQualityBar_->setRange(0, 1000);
    rxQualityBar_->setValue(0);
    rxQualityBar_->setFormat("%p%");
    configForm->addRow("RX quality", rxQualityBar_);

    rxDiagnosticLabel_ = new QLabel(this);
    rxDiagnosticLabel_->setWordWrap(true);
    configForm->addRow("RX state", rxDiagnosticLabel_);
    resetRxDiagnostic("Stopped");

    rxSessionLabel_ = new QLabel(this);
    rxSessionLabel_->setWordWrap(true);
    configForm->addRow("RX session", rxSessionLabel_);
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

    auto* wavButtons = new QHBoxLayout();
    generateButton_ = new QPushButton("Generate WAV", this);
    decodeButton_ = new QPushButton("Decode WAV", this);
    wavButtons->addWidget(generateButton_);
    wavButtons->addWidget(decodeButton_);
    wavButtons->addStretch(1);
    configLayout->addLayout(wavButtons);

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

    tabs->addTab(operationPage, "Operation");
    tabs->addTab(configPage, "Settings");
    root->addWidget(tabs);

    setCentralWidget(central);
    resize(720, 520);
    loadSettings();

    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateWav);
    connect(transmitButton_, &QPushButton::clicked, this, &MainWindow::transmitWav);
    connect(startReceiveButton_, &QPushButton::clicked, this, &MainWindow::startReceive);
    connect(stopReceiveButton_, &QPushButton::clicked, this, &MainWindow::stopReceive);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::decodeWav);
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
    connect(modulationModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateTxEstimate);
    connect(modulationModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::updateWaterfallMarkers);
    connect(modulationModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::restartReceiveIfActive);
    connect(symbolDurationSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTxEstimate);
    connect(symbolDurationSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::restartReceiveIfActive);
    connect(frequency0Spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateWaterfallMarkers);
    connect(frequency0Spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::restartReceiveIfActive);
    connect(frequency1Spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateWaterfallMarkers);
    connect(frequency1Spin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::restartReceiveIfActive);
    connect(preambleBitsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::updateTxEstimate);
    connect(preambleBitsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::restartReceiveIfActive);
    connect(rxSampleRateSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::restartReceiveIfActive);
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
        appendLog("TX concluido.");
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
    if (waterfallWidget_ == nullptr || frequency0Spin_ == nullptr || frequency1Spin_ == nullptr) {
        return;
    }

    hftext::ModemConfig config;
    config.frequency0Hz = static_cast<float>(frequency0Spin_->value());
    config.frequency1Hz = static_cast<float>(frequency0Spin_->value() + frequency1Spin_->value());
    config.modulationMode = modulationModeFromCombo(modulationModeCombo_);
    std::vector<double> frequencies;
    frequencies.reserve(static_cast<std::size_t>(hftext::toneCount(config.modulationMode)));
    for (int tone = 0; tone < hftext::toneCount(config.modulationMode); ++tone) {
        frequencies.push_back(hftext::modulationToneFrequencyHz(config, tone));
    }
    waterfallWidget_->setMarkerFrequencies(frequencies);
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

    const QSignalBlocker modeBlocker(modulationModeCombo_);
    const QSignalBlocker sampleRateBlocker(sampleRateSpin_);
    const QSignalBlocker rxSampleRateBlocker(rxSampleRateSpin_);
    const QSignalBlocker symbolDurationBlocker(symbolDurationSpin_);
    const QSignalBlocker baseFrequencyBlocker(frequency0Spin_);
    const QSignalBlocker spacingBlocker(frequency1Spin_);
    const QSignalBlocker amplitudeBlocker(amplitudeSpin_);
    const QSignalBlocker preambleBlocker(preambleBitsSpin_);
    const QSignalBlocker detailedLogBlocker(detailedRxLogCheck_);

    const int modeIndex = modulationModeCombo_->findData(2);
    if (modeIndex >= 0) {
        modulationModeCombo_->setCurrentIndex(modeIndex);
    }
    sampleRateSpin_->setValue(kDefaultSampleRate);
    rxSampleRateSpin_->setValue(kDefaultSampleRate);
    symbolDurationSpin_->setValue(kDefaultSymbolDurationSec);
    frequency0Spin_->setValue(kDefaultBaseFrequencyHz);
    frequency1Spin_->setValue(kDefaultToneSpacingHz);
    amplitudeSpin_->setValue(kDefaultAmplitude);
    preambleBitsSpin_->setValue(kDefaultPreambleBits);
    detailedRxLogCheck_->setChecked(false);

    updateTxEstimate();
    updateWaterfallMarkers();
    appendLog("Default parameters loaded.");
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

void MainWindow::writeLogHeader(QTextStream& stream, const char* title) const {
    stream << title << '\n';
    stream << "Generated at: " << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    stream << "Callsign: " << callsignEdit_->text().trimmed() << '\n';
    stream << "Modulation: " << modulationModeName(modulationModeFromCombo(modulationModeCombo_)) << '\n';
    stream << "Sample rate TX/WAV: " << sampleRateSpin_->value() << " Hz\n";
    stream << "Sample rate RX: " << rxSampleRateSpin_->value() << " Hz\n";
    stream << "Symbol duration: " << QString::number(symbolDurationSpin_->value(), 'f', 3) << " s\n";
    stream << "Base frequency: " << QString::number(frequency0Spin_->value(), 'f', 1) << " Hz\n";
    stream << "Tone spacing: " << QString::number(frequency1Spin_->value(), 'f', 1) << " Hz\n";
    stream << "Derived second tone: "
           << QString::number(frequency0Spin_->value() + frequency1Spin_->value(), 'f', 1)
           << " Hz\n";
    stream << "Amplitude: " << QString::number(amplitudeSpin_->value(), 'f', 2) << '\n';
    stream << "Preamble: " << preambleBitsSpin_->value() << " bits\n";
    stream << "Audio output: " << outputDeviceCombo_->currentText() << '\n';
    stream << "Audio input: " << inputDeviceCombo_->currentText() << '\n';
    stream << "Detailed RX log: " << (detailedRxLogCheck_->isChecked() ? "yes" : "no") << '\n';
    if (rxDiagnosticLabel_ != nullptr) {
        stream << "Current RX state: " << rxDiagnosticLabel_->text() << '\n';
    }
    if (rxSessionLabel_ != nullptr) {
        stream << "Current RX session: " << rxSessionLabel_->text() << '\n';
    }
    stream << "Stored accepted frames: " << static_cast<qulonglong>(acceptedRxFrames_.size()) << '\n';
}

void MainWindow::writeFieldSummaryCsv(
    QTextStream& stream,
    const QString& wavPath,
    std::size_t sampleCount,
    int sampleRate
) const {
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
        << "generated_at,callsign,modulation,symbol_duration_s,tx_sample_rate_hz,rx_sample_rate_hz,"
        << "f0_hz,f1_hz,tone_spacing_hz,amplitude,preamble_bits,detailed_log,"
        << "rx_elapsed_s,rx_accepted,accepted_frames,rx_rejected_strong,rx_phys_length,rx_sync,"
        << "rx_quality,last_phys_length,last_reject,received_lines,received_text,"
        << "accepted_length,accepted_offset_samples,accepted_offsets_tried,"
        << "saved_audio_s,saved_samples,wav_path\n";
    const QString summaryQuality = hasLastAcceptedRx_ ? lastAcceptedRxQualityText_ : lastRxQualityText_;
    const QString summaryPhysicalLength = hasLastAcceptedRx_
        ? QString("%1 symbols").arg(lastAcceptedRxLength_)
        : lastRxPhysicalLengthText_;
    stream
        << csvCell(QDateTime::currentDateTime().toString(Qt::ISODate)) << ','
        << csvCell(callsignEdit_->text().trimmed()) << ','
        << csvCell(modulationModeName(modulationModeFromCombo(modulationModeCombo_))) << ','
        << QString::number(symbolDurationSpin_->value(), 'f', 3) << ','
        << sampleRateSpin_->value() << ','
        << rxSampleRateSpin_->value() << ','
        << QString::number(frequency0Spin_->value(), 'f', 1) << ','
        << QString::number(frequency0Spin_->value() + frequency1Spin_->value(), 'f', 1) << ','
        << QString::number(frequency1Spin_->value(), 'f', 1) << ','
        << QString::number(amplitudeSpin_->value(), 'f', 2) << ','
        << preambleBitsSpin_->value() << ','
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
    const hftext::StreamingReceiverEvent* bestDecoded = nullptr;
    const hftext::StreamingReceiverEvent* bestWaiting = nullptr;
    const hftext::StreamingReceiverEvent* bestLength = nullptr;
    const hftext::StreamingReceiverEvent* bestRejected = nullptr;

    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::FrameDecoded:
            if (bestDecoded == nullptr || event.confidence > bestDecoded->confidence) {
                bestDecoded = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameWaiting:
            if (isStrongSyncEvent(event) && isBetterWaitingEvent(event, bestWaiting)) {
                bestWaiting = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestLength)) {
                bestLength = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameRejected:
            if (isStrongSyncEvent(event)
                && (bestRejected == nullptr || event.confidence > bestRejected->confidence)) {
                bestRejected = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::SyncFound:
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
            break;
        }
    }

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
        const double ratio = bestWaiting->bitsExpected <= 0
            ? 0.0
            : static_cast<double>(bestWaiting->bitsAvailable) / static_cast<double>(bestWaiting->bitsExpected);
        const int progress = static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 1000.0);
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
    const hftext::StreamingReceiverEvent* bestDecoded = nullptr;
    const hftext::StreamingReceiverEvent* bestWaiting = nullptr;
    const hftext::StreamingReceiverEvent* bestLength = nullptr;
    const hftext::StreamingReceiverEvent* bestSync = nullptr;
    const hftext::StreamingReceiverEvent* bestRejected = nullptr;
    bool hasInvalidLength = false;

    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::SyncFound:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestSync)) {
                bestSync = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            if (isStrongSyncEvent(event) && isBetterSyncEvent(event, bestLength)) {
                bestLength = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
            hasInvalidLength = true;
            lastRxRejectText_ = "invalid PHYS_LENGTH";
            break;
        case hftext::StreamingReceiverEventType::FrameWaiting:
            if (isStrongSyncEvent(event) && isBetterWaitingEvent(event, bestWaiting)) {
                bestWaiting = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameRejected:
            if (isStrongSyncEvent(event)
                && (bestRejected == nullptr || event.confidence > bestRejected->confidence)) {
                bestRejected = &event;
            }
            break;
        case hftext::StreamingReceiverEventType::FrameDecoded:
            if (bestDecoded == nullptr || event.confidence > bestDecoded->confidence) {
                bestDecoded = &event;
            }
            break;
        }
    }

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
        const double progress = bestWaiting->bitsExpected <= 0
            ? 0.0
            : 100.0 * static_cast<double>(bestWaiting->bitsAvailable) / static_cast<double>(bestWaiting->bitsExpected);
        state = QString("Receiving frame %1%").arg(progress, 0, 'f', 0);
    } else if (bestLength != nullptr) {
        displayEvent = bestLength;
        state = "PHYS_LENGTH recovered";
    } else if (bestSync != nullptr) {
        displayEvent = bestSync;
        state = QString("Sync detected (%1 error(s))").arg(bestSync->syncMismatches);
    } else if (bestRejected != nullptr) {
        displayEvent = bestRejected;
        state = "Candidate rejected";
    } else if (hasInvalidLength) {
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
    acceptedRxFrames_.clear();
    setRxSessionText();
}

void MainWindow::updateRxSessionFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    const auto counts = rxSessionEventCounts(events);
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

    hasLastAcceptedRx_ = true;
    lastAcceptedRxQualityText_ = formatConfidence(result.confidence);
    lastAcceptedRxLength_ = result.length;
    lastAcceptedRxOffsetSamples_ = result.startOffset;
    lastAcceptedRxOffsetsTried_ = result.offsetsTried;

    const auto elapsedMsecs = rxSessionStartedAtMsecs_ <= 0
        ? 0
        : QDateTime::currentMSecsSinceEpoch() - rxSessionStartedAtMsecs_;

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

void MainWindow::loadSettings() {
    QSettings settings("HFText", "HFText");

    restoreGeometry(settings.value("windowGeometry").toByteArray());
    callsignEdit_->setText(settings.value("callsign", callsignEdit_->text()).toString());
    const int savedMode = settings.value("modulationMode", 2).toInt();
    const int modeIndex = modulationModeCombo_->findData(savedMode);
    if (modeIndex >= 0) {
        modulationModeCombo_->setCurrentIndex(modeIndex);
    }
    sampleRateSpin_->setValue(settings.value("sampleRate", sampleRateSpin_->value()).toInt());
    rxSampleRateSpin_->setValue(settings.value("rxSampleRate", rxSampleRateSpin_->value()).toInt());
    symbolDurationSpin_->setValue(settings.value("symbolDuration", symbolDurationSpin_->value()).toDouble());
    const double baseFrequency = settings.value("frequency0", frequency0Spin_->value()).toDouble();
    frequency0Spin_->setValue(baseFrequency);
    if (settings.contains("toneSpacing")) {
        frequency1Spin_->setValue(settings.value("toneSpacing", frequency1Spin_->value()).toDouble());
    } else {
        const double savedFrequency1 = settings.value("frequency1", baseFrequency + frequency1Spin_->value()).toDouble();
        frequency1Spin_->setValue(savedFrequency1 > baseFrequency ? savedFrequency1 - baseFrequency : savedFrequency1);
    }
    amplitudeSpin_->setValue(settings.value("amplitude", amplitudeSpin_->value()).toDouble());
    preambleBitsSpin_->setValue(settings.value("preambleBits", preambleBitsSpin_->value()).toInt());
    detailedRxLogCheck_->setChecked(settings.value("detailedRxLog", detailedRxLogCheck_->isChecked()).toBool());
    selectComboText(outputDeviceCombo_, settings.value("outputDevice").toString());
    selectComboText(inputDeviceCombo_, settings.value("inputDevice").toString());
}

void MainWindow::saveSettings() const {
    QSettings settings("HFText", "HFText");

    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("callsign", callsignEdit_->text());
    settings.setValue("modulationMode", modulationModeCombo_->currentData().toInt());
    settings.setValue("sampleRate", sampleRateSpin_->value());
    settings.setValue("rxSampleRate", rxSampleRateSpin_->value());
    settings.setValue("symbolDuration", symbolDurationSpin_->value());
    settings.setValue("frequency0", frequency0Spin_->value());
    settings.setValue("frequency1", frequency0Spin_->value() + frequency1Spin_->value());
    settings.setValue("toneSpacing", frequency1Spin_->value());
    settings.setValue("amplitude", amplitudeSpin_->value());
    settings.setValue("preambleBits", preambleBitsSpin_->value());
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
        if (rxMaxPendingSamples_ > 0 && rxPendingSamples_.size() > rxMaxPendingSamples_) {
            const auto excess = rxPendingSamples_.size() - rxMaxPendingSamples_;
            rxPendingSamples_.erase(
                rxPendingSamples_.begin(),
                rxPendingSamples_.begin() + static_cast<std::ptrdiff_t>(excess)
            );
        }
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
        }

        const auto results = receiver.pushSamples(chunk);
        const auto events = receiver.takeEvents();
        if (!events.empty()) {
            const int frameQuality = rxQualityPermille(events);
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
        }
        if (!results.empty()) {
            emittedNormalRxStatusLines.clear();
        }
        for (const auto& result : results) {
            QMetaObject::invokeMethod(
                this,
                [this, result, config]() {
                    rxProgressSyncSample_ = -1;
                    rxDisplayedFrameProgressPermille_ = 1000;
                    rxFrameProgressBar_->setValue(rxDisplayedFrameProgressPermille_);
                    if (result.crcOk && result.payloadValid) {
                        rememberAcceptedRx(result, config);
                    }
                    showDecodeResult(result);
                    if (result.crcOk && result.payloadValid) {
                        ++rxSessionAcceptedCount_;
                        setRxSessionText();
                        appendLog("RX text: " + QString::fromStdString(result.text));
                    }
                    lastRxQualityText_ = formatConfidence(result.confidence);
                    lastRxRejectText_ = "--";
                    setRxDiagnosticText("Message accepted");
                    appendLog(
                        "RX: message accepted, offset " + QString::number(result.startOffset)
                        + " samples, phases " + QString::number(result.offsetsTried)
                        + ", confidence: " + formatConfidence(result.confidence)
                    );
                },
                Qt::QueuedConnection
            );
        }
    }
}

hftext::ModemConfig MainWindow::readConfig() const {
    hftext::ModemConfig config;
    config.sampleRate = sampleRateSpin_->value();
    config.symbolDurationSec = static_cast<float>(symbolDurationSpin_->value());
    config.frequency0Hz = static_cast<float>(frequency0Spin_->value());
    config.frequency1Hz = static_cast<float>(frequency0Spin_->value() + frequency1Spin_->value());
    config.amplitude = static_cast<float>(amplitudeSpin_->value());
    config.preambleBits = preambleBitsSpin_->value();
    config.modulationMode = modulationModeFromCombo(modulationModeCombo_);
    validateTonesBelowNyquist(config);
    if (config.frequency0Hz == config.frequency1Hz) {
        throw std::invalid_argument("tone spacing must be positive");
    }
    if (hftext::toneCount(config.modulationMode) > 2 && hftext::modulationToneSpacingHz(config) <= 0.0F) {
        throw std::invalid_argument("MFSK requires positive tone spacing");
    }
    return config;
}

hftext::ModemConfig MainWindow::readRxConfig() const {
    hftext::ModemConfig config = readConfig();
    config.sampleRate = rxSampleRateSpin_->value();
    validateTonesBelowNyquist(config);
    return config;
}

void MainWindow::showDecodeResult(const hftext::DecodeResult& result) {
    rxQualityBar_->setValue(static_cast<int>(std::clamp(result.confidence, 0.0F, 1.0F) * 1000.0F));
    if (!result.frameDetected) {
        receivedEdit_->appendPlainText(QStringLiteral("Frame not detected: ") + QString::fromStdString(result.error));
        return;
    }
    if (!result.crcOk) {
        receivedEdit_->appendPlainText("Frame detected, but CRC is invalid.");
        return;
    }
    if (!result.payloadValid) {
        receivedEdit_->appendPlainText("Frame detected, CRC is valid, but payload is invalid.");
        return;
    }
    receivedEdit_->appendPlainText(QString::fromStdString(result.text));
}
