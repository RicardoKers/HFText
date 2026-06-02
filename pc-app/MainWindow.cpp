#include "MainWindow.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

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
    callsignEdit_->setPlaceholderText("pu5lrk");
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

    root->addLayout(form);

    auto* buttons = new QHBoxLayout();
    generateButton_ = new QPushButton("Gerar WAV", this);
    decodeButton_ = new QPushButton("Decodificar WAV", this);
    buttons->addWidget(generateButton_);
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
    connect(decodeButton_, &QPushButton::clicked, this, &MainWindow::decodeWav);
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
        appendLog("WAV gerado: " + outputPath);
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

void MainWindow::appendLog(const QString& text) {
    logEdit_->appendPlainText(text);
}

hftext::ModemConfig MainWindow::readConfig() const {
    hftext::ModemConfig config;
    config.sampleRate = sampleRateSpin_->value();
    config.symbolDurationSec = static_cast<float>(symbolDurationSpin_->value());
    config.frequency0Hz = static_cast<float>(frequency0Spin_->value());
    config.frequency1Hz = static_cast<float>(frequency1Spin_->value());
    config.amplitude = static_cast<float>(amplitudeSpin_->value());
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
