#include "MainWindow.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <exception>
#include <stdexcept>

namespace {

std::string toStdString(const QString& text) {
    return text.toUtf8().toStdString();
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("HFText");

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    auto* form = new QFormLayout();
    callsignEdit_ = new QLineEdit(this);
    callsignEdit_->setText("pu5lrk");
    form->addRow("Indicativo", callsignEdit_);

    messageEdit_ = new QPlainTextEdit(this);
    messageEdit_->setPlaceholderText("Mensagem");
    messageEdit_->setMinimumHeight(90);
    form->addRow("Mensagem", messageEdit_);

    sampleRateSpin_ = new QSpinBox(this);
    sampleRateSpin_->setRange(8000, 192000);
    sampleRateSpin_->setSingleStep(1000);
    sampleRateSpin_->setSuffix(" Hz");
    sampleRateSpin_->setValue(controller_.config().sampleRate);
    form->addRow("Sample rate", sampleRateSpin_);

    symbolDurationSpin_ = new QDoubleSpinBox(this);
    symbolDurationSpin_->setRange(0.005, 5.0);
    symbolDurationSpin_->setSingleStep(0.05);
    symbolDurationSpin_->setDecimals(3);
    symbolDurationSpin_->setSuffix(" s");
    symbolDurationSpin_->setValue(controller_.config().symbolDurationSec);
    form->addRow("Duracao do simbolo", symbolDurationSpin_);

    frequency0Spin_ = new QDoubleSpinBox(this);
    frequency0Spin_->setRange(20.0, 20000.0);
    frequency0Spin_->setSingleStep(50.0);
    frequency0Spin_->setDecimals(1);
    frequency0Spin_->setSuffix(" Hz");
    frequency0Spin_->setValue(controller_.config().frequency0Hz);
    form->addRow("Tom 0", frequency0Spin_);

    frequency1Spin_ = new QDoubleSpinBox(this);
    frequency1Spin_->setRange(20.0, 20000.0);
    frequency1Spin_->setSingleStep(50.0);
    frequency1Spin_->setDecimals(1);
    frequency1Spin_->setSuffix(" Hz");
    frequency1Spin_->setValue(controller_.config().frequency1Hz);
    form->addRow("Tom 1", frequency1Spin_);

    amplitudeSpin_ = new QDoubleSpinBox(this);
    amplitudeSpin_->setRange(0.0, 1.0);
    amplitudeSpin_->setSingleStep(0.05);
    amplitudeSpin_->setDecimals(2);
    amplitudeSpin_->setValue(controller_.config().amplitude);
    form->addRow("Amplitude", amplitudeSpin_);

    preambleBitsSpin_ = new QSpinBox(this);
    preambleBitsSpin_->setRange(0, 256);
    preambleBitsSpin_->setSingleStep(8);
    preambleBitsSpin_->setSuffix(" bits");
    preambleBitsSpin_->setValue(controller_.config().preambleBits);
    form->addRow("Preambulo", preambleBitsSpin_);

    outputDeviceCombo_ = new QComboBox(this);
    form->addRow("Saida de audio", outputDeviceCombo_);
    populateOutputDevices();

    inputDeviceCombo_ = new QComboBox(this);
    form->addRow("Entrada de audio", inputDeviceCombo_);
    populateInputDevices();

    rxLevelBar_ = new QProgressBar(this);
    rxLevelBar_->setRange(0, 100);
    rxLevelBar_->setValue(0);
    rxLevelBar_->setTextVisible(false);
    form->addRow("Nivel RX", rxLevelBar_);

    root->addLayout(form);

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
    root->addLayout(buttons);

    root->addWidget(new QLabel("Texto recebido", this));
    receivedEdit_ = new QPlainTextEdit(this);
    receivedEdit_->setReadOnly(true);
    receivedEdit_->setMinimumHeight(80);
    root->addWidget(receivedEdit_);

    root->addWidget(new QLabel("Log", this));
    logEdit_ = new QPlainTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMinimumHeight(100);
    root->addWidget(logEdit_);

    setCentralWidget(central);
    resize(720, 520);

    connect(generateButton_, &QPushButton::clicked, this, &MainWindow::generateWav);
    connect(transmitButton_, &QPushButton::clicked, this, &MainWindow::transmitWav);
    connect(stopTransmitButton_, &QPushButton::clicked, this, &MainWindow::stopTransmit);
    connect(startReceiveButton_, &QPushButton::clicked, this, &MainWindow::startReceive);
    connect(stopReceiveButton_, &QPushButton::clicked, this, &MainWindow::stopReceive);
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::decodeWav);

    rxLevelTimer_ = new QTimer(this);
    rxLevelTimer_->setInterval(100);
    connect(rxLevelTimer_, &QTimer::timeout, this, &MainWindow::updateRxLevel);
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
        const auto result = controller_.decodeWav(toStdString(inputPath));
        showDecodeResult(result);
        appendLog("WAV decodificado: " + inputPath);
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
        appendLog("TX iniciado: " + wavPath);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao transmitir WAV: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopTransmit() {
    audioOutput_.stop();
    appendLog("TX interrompido.");
}

void MainWindow::startReceive() {
    const QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Salvar RX WAV",
        QString(),
        "WAV (*.wav)"
    );
    if (outputPath.isEmpty()) {
        return;
    }

    try {
        const auto config = readConfig();
        const unsigned int deviceId = inputDeviceCombo_->currentData().toUInt();
        audioInput_.start(deviceId, config.sampleRate);
        lastRxWavPath_ = outputPath;
        rxLevelTimer_->start();
        appendLog("RX iniciado: " + outputPath);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao iniciar RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::stopReceive() {
    if (lastRxWavPath_.isEmpty() && !audioInput_.isRecording()) {
        appendLog("RX nao iniciado.");
        return;
    }

    try {
        audioInput_.stopAndSave(toStdString(lastRxWavPath_));
        rxLevelTimer_->stop();
        rxLevelBar_->setValue(0);
        const std::string error = audioInput_.lastError();
        if (!error.empty()) {
            QMessageBox::warning(this, "HFText", QString::fromStdString(error));
            appendLog("Erro no RX: " + QString::fromStdString(error));
            return;
        }
        appendLog("RX salvo: " + lastRxWavPath_);
    } catch (const std::exception& exc) {
        QMessageBox::warning(this, "HFText", QString::fromUtf8(exc.what()));
        appendLog("Erro ao parar RX: " + QString::fromUtf8(exc.what()));
    }
}

void MainWindow::updateRxLevel() {
    const int level = static_cast<int>(std::clamp(audioInput_.level(), 0.0F, 1.0F) * 100.0F);
    rxLevelBar_->setValue(level);
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

hftext::ModemConfig MainWindow::readConfig() const {
    hftext::ModemConfig config;
    config.sampleRate = sampleRateSpin_->value();
    config.symbolDurationSec = static_cast<float>(symbolDurationSpin_->value());
    config.frequency0Hz = static_cast<float>(frequency0Spin_->value());
    config.frequency1Hz = static_cast<float>(frequency1Spin_->value());
    config.amplitude = static_cast<float>(amplitudeSpin_->value());
    config.preambleBits = preambleBitsSpin_->value();
    const float nyquistHz = static_cast<float>(config.sampleRate) / 2.0F;
    if (config.frequency0Hz >= nyquistHz || config.frequency1Hz >= nyquistHz) {
        throw std::invalid_argument("tons devem ficar abaixo de metade do sample rate");
    }
    if (config.frequency0Hz == config.frequency1Hz) {
        throw std::invalid_argument("tom 0 e tom 1 devem ser diferentes");
    }
    return config;
}

void MainWindow::showDecodeResult(const hftext::DecodeResult& result) {
    if (!result.frameDetected) {
        receivedEdit_->setPlainText(QStringLiteral("Quadro nao detectado: ") + QString::fromStdString(result.error));
        return;
    }
    if (!result.crcOk) {
        receivedEdit_->setPlainText("Quadro detectado, mas CRC invalido.");
        return;
    }
    if (!result.payloadValid) {
        receivedEdit_->setPlainText("Quadro detectado, CRC valido, mas payload invalido.");
        return;
    }
    receivedEdit_->setPlainText(QString::fromStdString(result.text));
}
