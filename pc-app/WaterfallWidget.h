#pragma once

#include <QImage>
#include <QWidget>

#include <vector>

class WaterfallWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaterfallWidget(QWidget* parent = nullptr);

    void addSamples(const std::vector<float>& samples, int sampleRate);
    void clear();
    void setMarkerFrequencies(double frequency0Hz, double frequency1Hz);
    void setMarkerFrequencies(const std::vector<double>& frequenciesHz);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect dataRect() const;
    void ensureImage();
    int frequencyToX(double frequencyHz, const QRect& targetRect) const;
    QRgb colorForLevel(double level, double inputPeak) const;

    QImage image_;
    std::vector<double> markerFrequenciesHz_ = {1200.0, 1600.0};
};
