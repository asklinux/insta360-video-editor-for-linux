#pragma once

#include <QImage>
#include <QLabel>
#include <QPoint>

class VideoPreviewWidget : public QLabel {
    Q_OBJECT

public:
    explicit VideoPreviewWidget(QWidget *parent = nullptr);
    void setPanEnabled(bool enabled);
    void setImageFrame(const QImage &image);
    void setPanoramaFrame(const QImage &equirectangular, int yawDegrees, int pitchDegrees, int rollDegrees);
    void updatePanoramaView(int yawDegrees, int pitchDegrees, int rollDegrees);

signals:
    void panDragged(int deltaX, int deltaY);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void renderPanorama();

    bool panEnabled_ = false;
    bool dragging_ = false;
    QPoint lastPos_;
    bool panoramaMode_ = false;
    QImage sourceFrame_;
    int yawDegrees_ = 0;
    int pitchDegrees_ = 0;
    int rollDegrees_ = 0;
};
