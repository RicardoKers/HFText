#pragma once

#include "AudioInput.h"
#include "AudioOutput.h"
#include "ModemController.h"

#include <QMainWindow>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QComboBox;
class QDoubleSpinBox;
class QProgressBar;
class QSpinBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void generateWav();
    void decodeWav();
    void transmitWav();
    void stopTransmit();
    void startReceive();
    void stopReceive();
    void updateRxLevel();

private:
    void appendLog(const QString& text);
    void populateInputDevices();
    void populateOutputDevices();
    hftext::ModemConfig readConfig() const;
    void showDecodeResult(const hftext::DecodeResult& result);

    AudioInput audioInput_;
    AudioOutput audioOutput_;
    ModemController controller_;
    QString lastWavPath_;
    QString lastRxWavPath_;
    QLineEdit* callsignEdit_ = nullptr;
    QPlainTextEdit* messageEdit_ = nullptr;
    QSpinBox* sampleRateSpin_ = nullptr;
    QDoubleSpinBox* symbolDurationSpin_ = nullptr;
    QDoubleSpinBox* frequency0Spin_ = nullptr;
    QDoubleSpinBox* frequency1Spin_ = nullptr;
    QDoubleSpinBox* amplitudeSpin_ = nullptr;
    QSpinBox* preambleBitsSpin_ = nullptr;
    QComboBox* inputDeviceCombo_ = nullptr;
    QComboBox* outputDeviceCombo_ = nullptr;
    QProgressBar* rxLevelBar_ = nullptr;
    QPlainTextEdit* receivedEdit_ = nullptr;
    QPlainTextEdit* logEdit_ = nullptr;
    QPushButton* generateButton_ = nullptr;
    QPushButton* decodeButton_ = nullptr;
    QPushButton* transmitButton_ = nullptr;
    QPushButton* stopTransmitButton_ = nullptr;
    QPushButton* startReceiveButton_ = nullptr;
    QPushButton* stopReceiveButton_ = nullptr;
    QTimer* rxLevelTimer_ = nullptr;
};
