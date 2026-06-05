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
#include <QSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
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

void validateTonesBelowNyquist(const hftext::ModemConfig& config) {
    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    if (config.frequency0Hz >= nyquistHz || config.frequency1Hz >= nyquistHz) {
        throw std::invalid_argument("tons devem ficar abaixo de metade do sample rate");
    }
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
        reasons.push_back("CRC falhou");
    }
    if (!event.payloadValid) {
        reasons.push_back("payload invalido");
    }
    if (event.decodedLength >= 0 && event.payloadLength >= 0 && event.decodedLength != event.payloadLength) {
        reasons.push_back(
            QString("LENGTH %1/%2")
                .arg(event.decodedLength)
                .arg(event.payloadLength)
        );
    }
    if (reasons.isEmpty()) {
        return "candidato rejeitado";
    }
    return reasons.join("; ");
}

QString clippingAdvice(std::size_t clippedSamples, std::size_t sampleCount) {
    if (clippedSamples == 0 || sampleCount == 0) {
        return {};
    }

    const double percent = clippingPercent(clippedSamples, sampleCount);
    if (percent < 0.01) {
        return "RX: picos isolados de clipping detectados; provavelmente ruido impulsivo.";
    }
    if (percent < 0.5) {
        return "RX: clipping ocasional detectado; observar se coincide com ruido do canal.";
    }
    return "RX: clipping frequente detectado; reduzir ganho/volume de entrada se possivel.";
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
    const QString phase = QString("fase %1 amostras").arg(event.phaseOffsetSamples);

    switch (event.type) {
    case hftext::StreamingReceiverEventType::SyncFound:
        return QString("RX: START_SYNC encontrado (%1, bit %2, erros %3)")
            .arg(phase)
            .arg(event.syncBitIndex)
            .arg(event.syncMismatches);
    case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
        return QString("RX: PHYS_LENGTH=%1 simbolos (%2 bits robustos esperados, %3)")
            .arg(event.payloadLength)
            .arg(event.bitsExpected)
            .arg(phase);
    case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        return QString("RX: PHYS_LENGTH invalido (%1, bit %2)")
            .arg(phase)
            .arg(event.syncBitIndex);
    case hftext::StreamingReceiverEventType::FrameWaiting: {
        const double progress = event.bitsExpected <= 0
            ? 0.0
            : 100.0 * static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
        return QString("RX: acumulando ROBUST_FRAME %1/%2 bits (%3%, %4)")
            .arg(event.bitsAvailable)
            .arg(event.bitsExpected)
            .arg(progress, 0, 'f', 0)
            .arg(phase);
    }
    case hftext::StreamingReceiverEventType::FrameRejected:
        return QString("RX: quadro rejeitado (CRC %1, payload %2, LENGTH %3/%4, conf %5, %6)")
            .arg(event.crcOk ? "ok" : "falhou")
            .arg(event.payloadValid ? "ok" : "falhou")
            .arg(event.decodedLength)
            .arg(event.payloadLength)
            .arg(formatConfidence(event.confidence))
            .arg(phase);
    case hftext::StreamingReceiverEventType::FrameDecoded:
        return QString("RX: quadro valido (%1 simbolos, conf %2, latencia %3 s, %4)")
            .arg(event.payloadLength)
            .arg(formatConfidence(event.confidence))
            .arg(event.latencySeconds, 0, 'f', 2)
            .arg(phase);
    }

    return "RX: evento desconhecido";
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

bool isNormalRxStatusLine(const QString& line) {
    return line.startsWith("RX: sync forte") || line.startsWith("RX: frame ");
}

std::vector<QString> filterRepeatedNormalRxStatus(
    std::vector<QString> lines,
    std::vector<QString>& emittedStatusLines
) {
    std::vector<QString> filtered;
    filtered.reserve(lines.size());

    for (const auto& line : lines) {
        if (!isNormalRxStatusLine(line)) {
            filtered.push_back(line);
            continue;
        }

        if (std::find(emittedStatusLines.begin(), emittedStatusLines.end(), line) != emittedStatusLines.end()) {
            continue;
        }

        emittedStatusLines.push_back(line);
        filtered.push_back(line);
    }

    return filtered;
}

int rxFrameProgressPermille(const std::vector<hftext::StreamingReceiverEvent>& events) {
    int bestProgress = -1;
    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::FrameDecoded:
            return 1000;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            bestProgress = std::max(bestProgress, 0);
            break;
        case hftext::StreamingReceiverEventType::FrameWaiting:
            if (event.bitsExpected > 0) {
                const double ratio = static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
                bestProgress = std::max(bestProgress, static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 1000.0));
            }
            break;
        case hftext::StreamingReceiverEventType::SyncFound:
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        case hftext::StreamingReceiverEventType::FrameRejected:
            break;
        }
    }

    return bestProgress;
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
            ++rejectedCount;
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
            QString("RX: sync forte, PHYS_LENGTH=%1 simbolos (%2 bits, erros %3)")
                .arg(bestLength->payloadLength)
                .arg(bestLength->bitsExpected)
                .arg(bestLength->syncMismatches)
        );
    } else if (bestSync != nullptr) {
        lines.push_back(
            QString("RX: sync forte detectado (erros %1)")
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
        lines.push_back(QString("RX: %1 candidato(s) rejeitado(s) por CRC/payload.").arg(rejectedCount));
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
    auto* operationForm = new QFormLayout();
    auto* configPage = new QWidget(this);
    auto* configLayout = new QVBoxLayout(configPage);
    auto* configForm = new QFormLayout();

    callsignEdit_ = new QLineEdit(this);
    callsignEdit_->setText("pu5lrk");
    operationForm->addRow("Indicativo", callsignEdit_);

    messageEdit_ = new QPlainTextEdit(this);
    messageEdit_->setPlaceholderText("Mensagem");
    messageEdit_->setMinimumHeight(90);
    operationForm->addRow("Mensagem", messageEdit_);

    txEstimateLabel_ = new QLabel(this);
    txEstimateLabel_->setWordWrap(true);
    operationForm->addRow("Estimativa TX", txEstimateLabel_);

    sampleRateSpin_ = new QSpinBox(this);
    sampleRateSpin_->setRange(8000, 192000);
    sampleRateSpin_->setSingleStep(1000);
    sampleRateSpin_->setSuffix(" Hz");
    sampleRateSpin_->setValue(controller_.config().sampleRate);
    configForm->addRow("Sample rate TX/WAV", sampleRateSpin_);

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
    configForm->addRow("Duracao do simbolo", symbolDurationSpin_);

    frequency0Spin_ = new QDoubleSpinBox(this);
    frequency0Spin_->setRange(20.0, 20000.0);
    frequency0Spin_->setSingleStep(50.0);
    frequency0Spin_->setDecimals(1);
    frequency0Spin_->setSuffix(" Hz");
    frequency0Spin_->setValue(controller_.config().frequency0Hz);
    configForm->addRow("Tom 0", frequency0Spin_);

    frequency1Spin_ = new QDoubleSpinBox(this);
    frequency1Spin_->setRange(20.0, 20000.0);
    frequency1Spin_->setSingleStep(50.0);
    frequency1Spin_->setDecimals(1);
    frequency1Spin_->setSuffix(" Hz");
    frequency1Spin_->setValue(controller_.config().frequency1Hz);
    configForm->addRow("Tom 1", frequency1Spin_);

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
    configForm->addRow("Preambulo", preambleBitsSpin_);

    outputDeviceCombo_ = new QComboBox(this);
    configForm->addRow("Saida de audio", outputDeviceCombo_);
    populateOutputDevices();

    inputDeviceCombo_ = new QComboBox(this);
    configForm->addRow("Entrada de audio", inputDeviceCombo_);
    populateInputDevices();

    detailedRxLogCheck_ = new QCheckBox("Log RX detalhado", this);
    detailedRxLogCheck_->setChecked(false);
    configForm->addRow("Diagnostico", detailedRxLogCheck_);

    rxLevelBar_ = new QProgressBar(this);
    rxLevelBar_->setRange(0, 100);
    rxLevelBar_->setValue(0);
    rxLevelBar_->setTextVisible(false);
    operationForm->addRow("Nivel RX", rxLevelBar_);

    txProgressBar_ = new QProgressBar(this);
    txProgressBar_->setRange(0, 1000);
    txProgressBar_->setValue(0);
    txProgressBar_->setFormat("%p%");
    operationForm->addRow("Progresso TX", txProgressBar_);

    rxFrameProgressBar_ = new QProgressBar(this);
    rxFrameProgressBar_->setRange(0, 1000);
    rxFrameProgressBar_->setValue(0);
    rxFrameProgressBar_->setFormat("%p%");
    operationForm->addRow("Progresso RX", rxFrameProgressBar_);

    rxQualityBar_ = new QProgressBar(this);
    rxQualityBar_->setRange(0, 1000);
    rxQualityBar_->setValue(0);
    rxQualityBar_->setFormat("%p%");
    operationForm->addRow("Qualidade RX", rxQualityBar_);

    rxDiagnosticLabel_ = new QLabel(this);
    rxDiagnosticLabel_->setWordWrap(true);
    operationForm->addRow("Estado RX", rxDiagnosticLabel_);
    resetRxDiagnostic("Parado");

    rxSessionLabel_ = new QLabel(this);
    rxSessionLabel_->setWordWrap(true);
    operationForm->addRow("Sessao RX", rxSessionLabel_);
    resetRxSessionCounters();

    operationLayout->addLayout(operationForm);
    configLayout->addLayout(configForm);

    auto* logHeader = new QHBoxLayout();
    logHeader->addWidget(new QLabel("Log", this));
    logHeader->addStretch(1);
    saveLogButton_ = new QPushButton("Salvar Log", this);
    clearLogButton_ = new QPushButton("Limpar Log", this);
    saveEvidenceButton_ = new QPushButton("Salvar Evidencia RX", this);
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

    operationLayout->addWidget(new QLabel("Waterfall RX", this));
    waterfallWidget_ = new WaterfallWidget(this);
    operationLayout->addWidget(waterfallWidget_);

    auto* buttons = new QHBoxLayout();
    generateButton_ = new QPushButton("Gerar WAV", this);
    transmitButton_ = new QPushButton("Transmitir WAV", this);
    stopTransmitButton_ = new QPushButton("Parar TX", this);
    startReceiveButton_ = new QPushButton("Receber", this);
    stopReceiveButton_ = new QPushButton("Parar RX", this);
    decodeButton_ = new QPushButton("Decodificar WAV", this);
    buttons->addWidget(generateButton_);
    buttons->addWidget(transmitButton_);
    buttons->addWidget(stopTransmitButton_);
    buttons->addWidget(startReceiveButton_);
    buttons->addWidget(stopReceiveButton_);
    buttons->addWidget(decodeButton_);
    buttons->addStretch(1);
    operationLayout->addLayout(buttons);

    auto* receivedHeader = new QHBoxLayout();
    receivedHeader->addWidget(new QLabel("Texto recebido", this));
    receivedHeader->addStretch(1);
    clearReceivedButton_ = new QPushButton("Limpar RX", this);
    receivedHeader->addWidget(clearReceivedButton_);
    operationLayout->addLayout(receivedHeader);

    receivedEdit_ = new QPlainTextEdit(this);
    receivedEdit_->setReadOnly(true);
    receivedEdit_->setMinimumHeight(80);
    operationLayout->addWidget(receivedEdit_);

    tabs->addTab(operationPage, "Operacao");
    tabs->addTab(configPage, "Configuracao");
    root->addWidget(tabs);

    setCentralWidget(central);
    resize(720, 520);
    loadSettings();

    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateWav);
    connect(transmitButton_, &QPushButton::clicked, this, &MainWindow::transmitWav);
    connect(stopTransmitButton_, &QPushButton::clicked, this, &MainWindow::stopTransmit);
    connect(startReceiveButton_, &QPushButton::clicked, this, &MainWindow::startReceive);
    connect(stopReceiveButton_, &QPushButton::clicked, this, &MainWindow::stopReceive);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::decodeWav);
    connect(clearReceivedButton_, &QPushButton::clicked, this, &MainWindow::clearReceivedText);
    connect(saveLogButton_, &QPushButton::clicked, this, &MainWindow::saveLog);
    connect(clearLogButton_, &QPushButton::clicked, this, &MainWindow::clearLog);
    connect(saveEvidenceButton_, &QPushButton::clicked, this, &MainWindow::saveFieldEvidence);
    connect(callsignEdit_, &QLineEdit::textChanged, this, &MainWindow::updateTxEstimate);
    connect(messageEdit_, &QPlainTextEdit::textChanged, this, &MainWindow::sanitizeTxMessage);
    connect(symbolDurationSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::updateTxEstimate);
    connect(preambleBitsSpin_, qOverload<int>(&QSpinBox::valueChanged), this, &MainWindow::updateTxEstimate);

    rxLevelTimer_ = new QTimer(this);
    rxLevelTimer_->setInterval(100);
    connect(rxLevelTimer_, &QTimer::timeout, this, &MainWindow::updateRxLevel);

    txProgressTimer_ = new QTimer(this);
    txProgressTimer_->setInterval(100);
    connect(txProgressTimer_, &QTimer::timeout, this, &MainWindow::updateTxProgress);
    updateTxEstimate();
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
        "Salvar WAV",
        QString(),
        "WAV (*.wav)"
    );
    if (outputPath.isEmpty()) {
        return;
    }

    try {
        controller_.setConfig(readConfig());
        const std::string callsign = toStdString(callsignEdit_->text().trimmed());
        const std::string message = toStdString(messageEdit_->toPlainText());
        controller_.generateWav(callsign, message, toStdString(outputPath));
        lastWavPath_ = outputPath;
        appendLog("WAV gerado: " + outputPath);
        appendLog("Modo TX: robust");
        appendLog("Payload TX: " + QString::fromStdString(controller_.buildPayload(callsign, message)));
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao gerar WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::decodeWav() {
    const QString inputPath = QFileDialog::getOpenFileName(
        this,
        "Abrir WAV",
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
            "WAV duracao: " + QString::number(stats.durationSeconds(), 'f', 2)
            + " s, sample rate: " + QString::number(stats.sampleRate)
            + " Hz, pico: " + QString::number(stats.peak * 100.0F, 'f', 1)
            + "%, clipping aprox.: " + formatClippingSummary(stats.clippedSamples, stats.sampleCount)
        );
        const QString advice = clippingAdvice(stats.clippedSamples, stats.sampleCount);
        if (!advice.isEmpty()) {
            appendLog(advice);
        }
        const auto result = controller_.decodeWav(toStdString(inputPath));
        showDecodeResult(result);
        if (result.crcOk && result.payloadValid) {
            appendLog("RX texto: " + QString::fromStdString(result.text));
        }
        appendLog("WAV decodificado: " + inputPath);
        appendLog("Modo RX: robust");
        appendLog(
            "RX offset: " + QString::number(result.startOffset)
            + " amostras, tentativas: " + QString::number(result.offsetsTried)
            + ", confianca: " + formatConfidence(result.confidence)
        );
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao decodificar WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::transmitWav() {
    QString wavPath = lastWavPath_;
    if (wavPath.isEmpty()) {
        wavPath = QFileDialog::getOpenFileName(
            this,
            "Transmitir WAV",
            QString(),
            "WAV (*.wav)"
        );
    }
    if (wavPath.isEmpty()) {
        return;
    }

    try {
        const unsigned int deviceId = outputDeviceCombo_->currentData().toUInt();
        audioOutput_.playWavAsync(toStdString(wavPath), deviceId);
        lastWavPath_ = wavPath;
        txProgressBar_->setValue(0);
        txProgressTimer_->start();
        appendLog("TX iniciado: " + wavPath);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao transmitir WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopTransmit() {
    audioOutput_.stop();
    txProgressTimer_->stop();
    txProgressBar_->setValue(0);
    appendLog("TX interrompido.");
}

void MainWindow::startReceive() {
    try {
        const auto rxConfig = readRxConfig();
        stopRxWorker();
        startRxWorker(rxConfig, detailedRxLogCheck_->isChecked());
        clearRxEvidenceSamples(rxConfig.sampleRate);
        waterfallWidget_->clear();
        waterfallUpdatePending_.store(false);
        rxFrameProgressBar_->setValue(0);
        rxQualityBar_->setValue(0);
        resetRxDiagnostic("Escutando");
        resetRxSessionCounters();
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
        appendLog(
            QStringLiteral("RX streaming iniciado")
            + " (captura " + QString::number(rxConfig.sampleRate) + " Hz)"
        );
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao iniciar RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopReceive() {
    if (!audioInput_.isRecording()) {
        appendLog("RX nao iniciado.");
        return;
    }

    try {
        controller_.setConfig(readRxConfig());
        const auto stats = audioInput_.stopAndSave({});
        audioInput_.setSamplesCallback({});
        waterfallUpdatePending_.store(false);
        stopRxWorker(false);
        rxLevelTimer_->stop();
        rxLevelBar_->setValue(0);
        rxFrameProgressBar_->setValue(0);
        resetRxDiagnostic("Parado");
        const std::string error = audioInput_.lastError();
        if (!error.empty()) {
            QMessageBox::warning(this, "HFText", QString::fromStdString(error));
            appendLog("Erro no RX: " + QString::fromStdString(error));
            return;
        }
        appendLog("RX streaming encerrado.");
        appendLog(
            "RX duracao: " + QString::number(stats.durationSeconds(), 'f', 2)
            + " s, sample rate: " + QString::number(stats.sampleRate)
            + " Hz, pico: " + QString::number(stats.peak * 100.0F, 'f', 1)
            + "%, clipping aprox.: " + formatClippingSummary(stats.clippedSamples, stats.sampleCount)
        );
        const QString advice = clippingAdvice(stats.clippedSamples, stats.sampleCount);
        if (!advice.isEmpty()) {
            appendLog(advice);
        }
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao parar RX: " + QString::fromUtf8(exc.what()));
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
}

void MainWindow::updateTxProgress() {
    const double duration = audioOutput_.durationSeconds();
    if (duration <= 0.0) {
        txProgressBar_->setValue(0);
        return;
    }

    const double position = audioOutput_.positionSeconds();
    const int value = static_cast<int>(std::clamp(position / duration, 0.0, 1.0) * 1000.0);
    txProgressBar_->setValue(value);

    if (!audioOutput_.isPlaying() && position >= duration) {
        txProgressTimer_->stop();
        txProgressBar_->setValue(1000);
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
                QString("0/%1 simbolos | robust | TX: --")
                    .arg(estimate.maxPayloadSymbols)
            );
            return;
        }

        QString text = QString("%1/%2 simbolos | %3 | %4 bits | TX: %5 s")
            .arg(estimate.payloadSymbols)
            .arg(estimate.maxPayloadSymbols)
            .arg("robust")
            .arg(estimate.transmissionBits)
            .arg(estimate.durationSeconds, 0, 'f', 2);

        if (estimate.payloadTooLong) {
            text += " | excede o limite";
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

void MainWindow::saveLog() {
    const QString defaultName = "HFText-log-"
        + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")
        + ".txt";
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Salvar Log",
        defaultName,
        "Texto (*.txt)"
    );
    if (outputPath.isEmpty()) {
        return;
    }

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "HFText", "Nao foi possivel salvar o log.");
        appendLog("Erro ao salvar log: " + outputPath);
        return;
    }

    QTextStream stream(&file);
    writeLogHeader(stream, "HFText log");
    stream << '\n';
    stream << "--- Log ---\n";
    stream << logEdit_->toPlainText() << '\n';
    appendLog("Log salvo: " + outputPath);
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
        QMessageBox::information(this, "HFText", "Nao ha audio RX recente para salvar.");
        appendLog("Evidencia RX nao salva: sem audio recente.");
        return;
    }

    const QString outputDir = QFileDialog::getExistingDirectory(
        this,
        "Salvar Evidencia RX"
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
        appendLog("Erro ao salvar WAV de evidencia RX: " + QString::fromUtf8(exc.what()));
        return;
    }

    QFile file(logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "HFText", "Nao foi possivel salvar o log de evidencia.");
        appendLog("Erro ao salvar log de evidencia: " + logPath);
        return;
    }

    QTextStream stream(&file);
    writeLogHeader(stream, "HFText evidencia RX");
    stream << "WAV RX recente: " << wavPath << '\n';
    stream << "Janela RX recente: ate " << kRxEvidenceSeconds << " s\n";
    stream << "Amostras RX salvas: " << static_cast<qulonglong>(samples.size()) << '\n';
    stream << "Duracao RX salva: "
           << QString::number(static_cast<double>(samples.size()) / static_cast<double>(sampleRate), 'f', 2)
           << " s\n\n";
    stream << "--- Texto recebido ---\n";
    stream << receivedEdit_->toPlainText() << "\n\n";
    stream << "--- Log ---\n";
    stream << logEdit_->toPlainText() << '\n';

    lastRxWavPath_ = wavPath;
    appendLog("Evidencia RX salva: " + wavPath + " | " + logPath);
}

void MainWindow::appendLog(const QString& text) {
    const QString timestamp = QDateTime::currentDateTime().toString("[HH:mm:ss] ");
    logEdit_->appendPlainText(timestamp + text);
}

void MainWindow::writeLogHeader(QTextStream& stream, const char* title) const {
    stream << title << '\n';
    stream << "Gerado em: " << QDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    stream << "Indicativo: " << callsignEdit_->text().trimmed() << '\n';
    stream << "Sample rate TX/WAV: " << sampleRateSpin_->value() << " Hz\n";
    stream << "Sample rate RX: " << rxSampleRateSpin_->value() << " Hz\n";
    stream << "Duracao do simbolo: " << QString::number(symbolDurationSpin_->value(), 'f', 3) << " s\n";
    stream << "Tom 0: " << QString::number(frequency0Spin_->value(), 'f', 1) << " Hz\n";
    stream << "Tom 1: " << QString::number(frequency1Spin_->value(), 'f', 1) << " Hz\n";
    stream << "Amplitude: " << QString::number(amplitudeSpin_->value(), 'f', 2) << '\n';
    stream << "Preambulo: " << preambleBitsSpin_->value() << " bits\n";
    stream << "Saida de audio: " << outputDeviceCombo_->currentText() << '\n';
    stream << "Entrada de audio: " << inputDeviceCombo_->currentText() << '\n';
    stream << "Log RX detalhado: " << (detailedRxLogCheck_->isChecked() ? "sim" : "nao") << '\n';
    if (rxDiagnosticLabel_ != nullptr) {
        stream << "Estado RX atual: " << rxDiagnosticLabel_->text() << '\n';
    }
    if (rxSessionLabel_ != nullptr) {
        stream << "Sessao RX atual: " << rxSessionLabel_->text() << '\n';
    }
}

void MainWindow::resetRxDiagnostic(const QString& state) {
    lastRxPhysicalLengthText_ = "--";
    lastRxQualityText_ = "--";
    lastRxRejectText_ = "--";
    setRxDiagnosticText(state);
}

void MainWindow::updateRxDiagnosticFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    QString state;
    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::SyncFound:
            state = QString("Sync detectado (%1 erro(s))").arg(event.syncMismatches);
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            lastRxPhysicalLengthText_ = QString("%1 simbolos, %2 bits")
                .arg(event.payloadLength)
                .arg(event.bitsExpected);
            state = "PHYS_LENGTH recuperado";
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
            lastRxRejectText_ = "PHYS_LENGTH invalido";
            state = "Tamanho fisico invalido";
            break;
        case hftext::StreamingReceiverEventType::FrameWaiting: {
            lastRxPhysicalLengthText_ = QString("%1 simbolos, %2 bits")
                .arg(event.payloadLength)
                .arg(event.bitsExpected);
            const double progress = event.bitsExpected <= 0
                ? 0.0
                : 100.0 * static_cast<double>(event.bitsAvailable) / static_cast<double>(event.bitsExpected);
            state = QString("Recebendo frame %1%").arg(progress, 0, 'f', 0);
            break;
        }
        case hftext::StreamingReceiverEventType::FrameRejected:
            lastRxPhysicalLengthText_ = QString("%1 simbolos, %2 bits")
                .arg(event.payloadLength)
                .arg(event.bitsExpected);
            lastRxQualityText_ = formatConfidence(event.confidence);
            lastRxRejectText_ = frameRejectionReason(event);
            state = "Candidato rejeitado";
            break;
        case hftext::StreamingReceiverEventType::FrameDecoded:
            lastRxPhysicalLengthText_ = QString("%1 simbolos, %2 bits")
                .arg(event.payloadLength)
                .arg(event.bitsExpected);
            lastRxQualityText_ = formatConfidence(event.confidence);
            lastRxRejectText_ = "--";
            state = "Quadro valido";
            break;
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
        QString("%1 | PHYS_LENGTH: %2 | Qualidade: %3 | Ultimo: %4")
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
    setRxSessionText();
}

void MainWindow::updateRxSessionFromEvents(const std::vector<hftext::StreamingReceiverEvent>& events) {
    for (const auto& event : events) {
        switch (event.type) {
        case hftext::StreamingReceiverEventType::SyncFound:
            ++rxSessionSyncCount_;
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthRecovered:
            ++rxSessionLengthCount_;
            break;
        case hftext::StreamingReceiverEventType::FrameRejected:
            ++rxSessionRejectedCount_;
            break;
        case hftext::StreamingReceiverEventType::FrameDecoded:
            ++rxSessionAcceptedCount_;
            break;
        case hftext::StreamingReceiverEventType::PhysicalLengthInvalid:
        case hftext::StreamingReceiverEventType::FrameWaiting:
            break;
        }
    }
    setRxSessionText();
}

void MainWindow::setRxSessionText() {
    if (rxSessionLabel_ == nullptr) {
        return;
    }
    rxSessionLabel_->setText(
        QString("aceitos %1 | rejeitados %2 | PHYS_LENGTH %3 | sync %4")
            .arg(rxSessionAcceptedCount_)
            .arg(rxSessionRejectedCount_)
            .arg(rxSessionLengthCount_)
            .arg(rxSessionSyncCount_)
    );
}

void MainWindow::loadSettings() {
    QSettings settings("HFText", "HFText");

    restoreGeometry(settings.value("windowGeometry").toByteArray());
    callsignEdit_->setText(settings.value("callsign", callsignEdit_->text()).toString());
    sampleRateSpin_->setValue(settings.value("sampleRate", sampleRateSpin_->value()).toInt());
    rxSampleRateSpin_->setValue(settings.value("rxSampleRate", rxSampleRateSpin_->value()).toInt());
    symbolDurationSpin_->setValue(settings.value("symbolDuration", symbolDurationSpin_->value()).toDouble());
    frequency0Spin_->setValue(settings.value("frequency0", frequency0Spin_->value()).toDouble());
    frequency1Spin_->setValue(settings.value("frequency1", frequency1Spin_->value()).toDouble());
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
    settings.setValue("sampleRate", sampleRateSpin_->value());
    settings.setValue("rxSampleRate", rxSampleRateSpin_->value());
    settings.setValue("symbolDuration", symbolDurationSpin_->value());
    settings.setValue("frequency0", frequency0Spin_->value());
    settings.setValue("frequency1", frequency1Spin_->value());
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
        inputDeviceCombo_->addItem("Nenhum dispositivo encontrado", 0U);
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
        outputDeviceCombo_->addItem("Nenhum dispositivo encontrado", 0U);
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
            const int frameProgress = rxFrameProgressPermille(events);
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
                    frameProgress,
                    frameQuality
                ]() {
                    if (frameProgress >= 0) {
                        rxFrameProgressBar_->setValue(frameProgress);
                    }
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
                [this, result]() {
                    rxFrameProgressBar_->setValue(1000);
                    showDecodeResult(result);
                    if (result.crcOk && result.payloadValid) {
                        appendLog("RX texto: " + QString::fromStdString(result.text));
                    }
                    lastRxQualityText_ = formatConfidence(result.confidence);
                    lastRxRejectText_ = "--";
                    setRxDiagnosticText("Mensagem aceita");
                    appendLog(
                        "RX: mensagem aceita, offset " + QString::number(result.startOffset)
                        + " amostras, fases " + QString::number(result.offsetsTried)
                        + ", confianca: " + formatConfidence(result.confidence)
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
    config.frequency1Hz = static_cast<float>(frequency1Spin_->value());
    config.amplitude = static_cast<float>(amplitudeSpin_->value());
    config.preambleBits = preambleBitsSpin_->value();
    validateTonesBelowNyquist(config);
    if (config.frequency0Hz == config.frequency1Hz) {
        throw std::invalid_argument("tom 0 e tom 1 devem ser diferentes");
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
        receivedEdit_->appendPlainText(QStringLiteral("Quadro nao detectado: ") + QString::fromStdString(result.error));
        return;
    }
    if (!result.crcOk) {
        receivedEdit_->appendPlainText("Quadro detectado, mas CRC invalido.");
        return;
    }
    if (!result.payloadValid) {
        receivedEdit_->appendPlainText("Quadro detectado, CRC valido, mas payload invalido.");
        return;
    }
    receivedEdit_->appendPlainText(QString::fromStdString(result.text));
}
