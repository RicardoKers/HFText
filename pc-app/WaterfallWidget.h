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

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QRect dataRect() const;
    void ensureImage();
    QRgb colorForLevel(double level) const;

    QImage image_;
};
