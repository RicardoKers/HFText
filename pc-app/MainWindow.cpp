#include "MainWindow.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>

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
