#include "MainWindow.h"

#include "hftext_encoder.h"

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
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace {

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

    rxQualityBar_ = new QProgressBar(this);
    rxQualityBar_->setRange(0, 1000);
    rxQualityBar_->setValue(0);
    rxQualityBar_->setFormat("%p%");
    operationForm->addRow("Qualidade RX", rxQualityBar_);

    operationLayout->addLayout(operationForm);
    configLayout->addLayout(configForm);

    configLayout->addWidget(new QLabel("Log", this));
    logEdit_ = new QPlainTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(180);
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

    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateWav);
    connect(transmitButton_, &QPushButton::clicked, this, &MainWindow::transmitWav);
    connect(stopTransmitButton_, &QPushButton::clicked, this, &MainWindow::stopTransmit);
    connect(startReceiveButton_, &QPushButton::clicked, this, &MainWindow::startReceive);
    connect(stopReceiveButton_, &QPushButton::clicked, this, &MainWindow::stopReceive);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::decodeWav);
    connect(clearReceivedButton_, &QPushButton::clicked, this, &MainWindow::clearReceivedText);
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
            + "%, clipping aprox.: " + QString::number(static_cast<qulonglong>(stats.clippedSamples))
            + " samples"
        );
        const auto result = controller_.decodeWav(toStdString(inputPath));
        showDecodeResult(result);
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
        startRxWorker(rxConfig);
        waterfallWidget_->clear();
        rxQualityBar_->setValue(0);
        audioInput_.setSamplesCallback([this, sampleRate = rxConfig.sampleRate](const std::vector<float>& samples) {
            enqueueRxSamples(samples);
            auto chunk = samples;
            QMetaObject::invokeMethod(
                this,
                [this, chunk = std::move(chunk), sampleRate]() {
                    waterfallWidget_->addSamples(chunk, sampleRate);
                },
                Qt::QueuedConnection
            );
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
        stopRxWorker(true);
        rxLevelTimer_->stop();
        rxLevelBar_->setValue(0);
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
            + "%, clipping aprox.: " + QString::number(static_cast<qulonglong>(stats.clippedSamples))
            + " samples"
        );
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao parar RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::clearReceivedText() {
    receivedEdit_->clear();
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

void MainWindow::appendLog(const QString& text) {
    logEdit_->appendPlainText(text);
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

void MainWindow::startRxWorker(const hftext::ModemConfig& config) {
    {
        std::lock_guard<std::mutex> lock(rxMutex_);
        rxPendingSamples_.clear();
        rxWorkerStop_ = false;
    }
    rxWorker_ = std::thread(&MainWindow::rxWorkerLoop, this, config);
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
    }
    rxCondition_.notify_one();
}

void MainWindow::rxWorkerLoop(hftext::ModemConfig config) {
    hftext::StreamingReceiver receiver(config);

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

            chunk.swap(rxPendingSamples_);
        }

        const auto results = receiver.pushSamples(chunk);
        for (const auto& result : results) {
            QMetaObject::invokeMethod(
                this,
                [this, result]() {
                    showDecodeResult(result);
                    appendLog("RX streaming: quadro com CRC valido.");
                    appendLog(
                        "RX offset: " + QString::number(result.startOffset)
                        + " amostras, tentativas: " + QString::number(result.offsetsTried)
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
