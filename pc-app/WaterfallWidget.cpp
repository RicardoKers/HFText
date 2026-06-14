#include "WaterfallWidget.h"

#include <QPainter>
#include <QResizeEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kMinFrequencyHz = 300.0;
constexpr double kMaxFrequencyHz = 3000.0;
constexpr int kMaxAnalysisSamples = 2048;
constexpr int kAxisHeight = 24;
constexpr double kMinPower = 1.0e-8;
constexpr std::array<double, 10> kAxisFrequenciesHz = {
    300.0,
    600.0,
    900.0,
    1200.0,
    1500.0,
    1800.0,
    2100.0,
    2400.0,
    2700.0,
    3000.0,
};

double toneLevel(const std::vector<float>& samples, int sampleRate, double frequencyHz) {
    if (samples.empty() || sampleRate <= 0) {
        return 0.0;
    }

    const int count = (std::min)(static_cast<int>(samples.size()), kMaxAnalysisSamples);
    const int start = static_cast<int>(samples.size()) - count;
    const double phaseStep = 2.0 * kPi * frequencyHz / static_cast<double>(sampleRate);

    double iSum = 0.0;
    double qSum = 0.0;
    for (int index = 0; index < count; ++index) {
        const double phase = phaseStep * static_cast<double>(index);
        const double sample = static_cast<double>(samples[static_cast<std::size_t>(start + index)]);
        iSum += sample * std::cos(phase);
        qSum += sample * std::sin(phase);
    }

    const double magnitude = std::sqrt(iSum * iSum + qSum * qSum) / static_cast<double>(count);
    const double db = 20.0 * std::log10((std::max)(magnitude, kMinPower));
    return std::clamp((db + 60.0) / 60.0, 0.0, 1.0);
}

double peakLevel(const std::vector<float>& samples) {
    double peak = 0.0;
    for (float sample : samples) {
        peak = (std::max)(peak, std::abs(static_cast<double>(sample)));
    }
    return peak;
}

}  // namespace

WaterfallWidget::WaterfallWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(144);
}

void WaterfallWidget::addSamples(const std::vector<float>& samples, int sampleRate) {
    if (samples.empty() || width() <= 0 || height() <= 0) {
        return;
    }

    ensureImage();
    if (image_.isNull()) {
        return;
    }

    for (int y = 0; y < image_.height() - 1; ++y) {
        std::memcpy(image_.scanLine(y), image_.constScanLine(y + 1), static_cast<std::size_t>(image_.bytesPerLine()));
    }

    const double inputPeak = peakLevel(samples);
    auto* row = reinterpret_cast<QRgb*>(image_.scanLine(image_.height() - 1));
    for (int x = 0; x < image_.width(); ++x) {
        const double ratio = image_.width() <= 1 ? 0.0 : static_cast<double>(x) / static_cast<double>(image_.width() - 1);
        const double frequency = kMinFrequencyHz + ratio * (kMaxFrequencyHz - kMinFrequencyHz);
        row[x] = colorForLevel(toneLevel(samples, sampleRate, frequency), inputPeak);
    }

    update();
}

void WaterfallWidget::clear() {
    ensureImage();
    if (!image_.isNull()) {
        image_.fill(Qt::black);
        update();
    }
}

void WaterfallWidget::setMarkerFrequencies(double frequency0Hz, double frequency1Hz) {
    if (markerFrequency0Hz_ == frequency0Hz && markerFrequency1Hz_ == frequency1Hz) {
        return;
    }

    markerFrequency0Hz_ = frequency0Hz;
    markerFrequency1Hz_ = frequency1Hz;
    update();
}

void WaterfallWidget::paintEvent(QPaintEvent* event) {
    (void)event;
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
    const QRect waterfallRect = dataRect();
    if (!image_.isNull()) {
        painter.drawImage(waterfallRect, image_);
    }

    painter.save();
    painter.setClipRect(waterfallRect);
    QPen markerPen(QColor(255, 220, 0, 210), 2);
    painter.setPen(markerPen);
    for (const double frequency : {markerFrequency0Hz_, markerFrequency1Hz_}) {
        if (frequency < kMinFrequencyHz || frequency > kMaxFrequencyHz) {
            continue;
        }
        const int x = frequencyToX(frequency, waterfallRect);
        painter.drawLine(x, waterfallRect.top(), x, waterfallRect.bottom());
    }
    painter.restore();

    const QRect axisRect(0, waterfallRect.bottom() + 1, width(), kAxisHeight);
    painter.fillRect(axisRect, QColor(18, 18, 18));
    painter.setPen(QColor(190, 190, 190));
    painter.drawLine(axisRect.left(), axisRect.top(), axisRect.right(), axisRect.top());

    const QFontMetrics metrics(painter.font());
    for (double frequency : kAxisFrequenciesHz) {
        const double ratio = (frequency - kMinFrequencyHz) / (kMaxFrequencyHz - kMinFrequencyHz);
        const int x = static_cast<int>(std::round(ratio * static_cast<double>((std::max)(1, width() - 1))));
        painter.drawLine(x, axisRect.top(), x, axisRect.top() + 5);

        const QString label = frequency >= 1000.0
            ? QString::number(frequency / 1000.0, 'f', frequency == 3000.0 ? 0 : 1) + " kHz"
            : QString::number(static_cast<int>(frequency));
        const int labelWidth = metrics.horizontalAdvance(label);
        const int labelX = std::clamp(x - labelWidth / 2, 0, (std::max)(0, width() - labelWidth));
        painter.drawText(labelX, axisRect.top() + 18, label);
    }

    painter.setPen(QPen(QColor(255, 220, 0), 2));
    for (const double frequency : {markerFrequency0Hz_, markerFrequency1Hz_}) {
        if (frequency < kMinFrequencyHz || frequency > kMaxFrequencyHz) {
            continue;
        }
        const int x = frequencyToX(frequency, axisRect);
        painter.drawLine(x, axisRect.top(), x, axisRect.top() + 7);
    }
}

void WaterfallWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    ensureImage();
}

void WaterfallWidget::ensureImage() {
    const QRect rect = dataRect();
    const int imageWidth = (std::max)(1, rect.width());
    const int imageHeight = (std::max)(1, rect.height());
    if (image_.size() == QSize(imageWidth, imageHeight)) {
        return;
    }

    QImage next(imageWidth, imageHeight, QImage::Format_RGB32);
    next.fill(Qt::black);
    if (!image_.isNull()) {
        QPainter painter(&next);
        painter.drawImage(next.rect(), image_);
    }
    image_ = next;
}

QRect WaterfallWidget::dataRect() const {
    return QRect(0, 0, (std::max)(1, width()), (std::max)(1, height() - kAxisHeight));
}

int WaterfallWidget::frequencyToX(double frequencyHz, const QRect& targetRect) const {
    const double ratio = std::clamp(
        (frequencyHz - kMinFrequencyHz) / (kMaxFrequencyHz - kMinFrequencyHz),
        0.0,
        1.0
    );
    return targetRect.left() + static_cast<int>(
        std::round(ratio * static_cast<double>((std::max)(1, targetRect.width() - 1)))
    );
}

QRgb WaterfallWidget::colorForLevel(double level, double inputPeak) const {
    const double clamped = std::clamp(level, 0.0, 1.0);
    if (clamped <= 0.03) {
        return qRgb(0, 0, 18);
    }

    if (inputPeak >= 0.98 && clamped > 0.08) {
        const int green = static_cast<int>(std::clamp((1.0 - clamped) * 90.0, 20.0, 90.0));
        return qRgb(255, green, 0);
    }

    int red = static_cast<int>(std::clamp((clamped - 0.72) * 0.70, 0.0, 1.0) * 90.0);
    int green = static_cast<int>(std::clamp((clamped - 0.02) * 1.55, 0.0, 1.0) * 235.0);
    int blue = static_cast<int>(std::clamp(0.05 + clamped * 0.22, 0.0, 0.24) * 255.0);

    if (inputPeak >= 0.85 && clamped > 0.08) {
        red = (std::max)(red, 240);
        green = (std::max)(green, 190);
        blue = (std::min)(blue, 24);
    }

    return qRgb(red, green, blue);
}
