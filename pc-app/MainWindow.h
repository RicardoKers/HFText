#pragma once

#include "ModemController.h"

#include <QMainWindow>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QDoubleSpinBox;
class QSpinBox;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void generateWav();
    void decodeWav();

private:
    void appendLog(const QString& text);
    hftext::ModemConfig readConfig() const;
    void showDecodeResult(const hftext::DecodeResult& result);

    ModemController controller_;
    QLineEdit* callsignEdit_ = nullptr;
    QPlainTextEdit* messageEdit_ = nullptr;
    QSpinBox* sampleRateSpin_ = nullptr;
    QDoubleSpinBox* symbolDurationSpin_ = nullptr;
    QDoubleSpinBox* frequency0Spin_ = nullptr;
    QDoubleSpinBox* frequency1Spin_ = nullptr;
    QDoubleSpinBox* amplitudeSpin_ = nullptr;
    QPlainTextEdit* receivedEdit_ = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QPushButton* generateButton_ = nullptr;
    QPushButton* decodeButton_ = nullptr;
};
