#include "VideoPreviewWidget.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QtMath>

namespace {

double clamp(double value, double minimum, double maximum)
{
    return qMax(minimum, qMin(maximum, value));
}

} // namespace

VideoPreviewWidget::VideoPreviewWidget(QWidget *parent)
    : QLabel(parent)
{
    setMouseTracking(true);
}

void VideoPreviewWidget::setPanEnabled(bool enabled)
{
    panEnabled_ = enabled;
    setCursor(enabled ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

void VideoPreviewWidget::setImageFrame(const QImage &image)
{
    panoramaMode_ = false;
    sourceFrame_ = image.convertToFormat(QImage::Format_RGB32);
    if (sourceFrame_.isNull()) {
        setText(QStringLiteral("Preview tidak tersedia"));
        return;
    }
    setPixmap(QPixmap::fromImage(sourceFrame_).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void VideoPreviewWidget::setPanoramaFrame(const QImage &equirectangular, int yawDegrees, int pitchDegrees, int rollDegrees)
{
    panoramaMode_ = true;
    sourceFrame_ = equirectangular.convertToFormat(QImage::Format_RGB32);
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = pitchDegrees;
    rollDegrees_ = rollDegrees;
    renderPanorama();
}

void VideoPreviewWidget::updatePanoramaView(int yawDegrees, int pitchDegrees, int rollDegrees)
{
    if (!panoramaMode_ || sourceFrame_.isNull()) {
        return;
    }
    yawDegrees_ = yawDegrees;
    pitchDegrees_ = pitchDegrees;
    rollDegrees_ = rollDegrees;
    renderPanorama();
}

void VideoPreviewWidget::resizeEvent(QResizeEvent *event)
{
    QLabel::resizeEvent(event);
    if (panoramaMode_) {
        renderPanorama();
    } else if (!sourceFrame_.isNull()) {
        setPixmap(QPixmap::fromImage(sourceFrame_).scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void VideoPreviewWidget::mousePressEvent(QMouseEvent *event)
{
    if (panEnabled_ && event->button() == Qt::LeftButton) {
        dragging_ = true;
        lastPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void VideoPreviewWidget::renderPanorama()
{
    if (sourceFrame_.isNull()) {
        setText(QStringLiteral("Preview panorama tidak tersedia"));
        return;
    }

    const int outW = qMax(320, width());
    const int outH = qMax(180, height());
    QImage output(outW, outH, QImage::Format_RGB32);

    const double yaw = qDegreesToRadians(static_cast<double>(yawDegrees_));
    const double pitch = qDegreesToRadians(static_cast<double>(pitchDegrees_));
    const double roll = qDegreesToRadians(static_cast<double>(rollDegrees_));
    const double fov = qDegreesToRadians(90.0);
    const double aspect = outW / static_cast<double>(outH);
    const double tanHalfFov = std::tan(fov / 2.0);

    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double cr = std::cos(roll);
    const double sr = std::sin(roll);

    for (int y = 0; y < outH; ++y) {
        QRgb *scan = reinterpret_cast<QRgb *>(output.scanLine(y));
        const double ndcY = (1.0 - 2.0 * ((y + 0.5) / outH)) * tanHalfFov;
        for (int x = 0; x < outW; ++x) {
            const double ndcX = (2.0 * ((x + 0.5) / outW) - 1.0) * aspect * tanHalfFov;

            double vx = ndcX;
            double vy = ndcY;
            double vz = 1.0;
            const double length = std::sqrt(vx * vx + vy * vy + vz * vz);
            vx /= length;
            vy /= length;
            vz /= length;

            const double rx = vx * cr - vy * sr;
            const double ry = vx * sr + vy * cr;
            const double rz = vz;

            const double px = rx;
            const double py = ry * cp - rz * sp;
            const double pz = ry * sp + rz * cp;

            const double wx = px * cy + pz * sy;
            const double wy = py;
            const double wz = -px * sy + pz * cy;

            const double lon = std::atan2(wx, wz);
            const double lat = std::asin(clamp(wy, -1.0, 1.0));
            int srcX = static_cast<int>((lon / (2.0 * M_PI) + 0.5) * sourceFrame_.width());
            int srcY = static_cast<int>((0.5 - lat / M_PI) * sourceFrame_.height());
            srcX = (srcX % sourceFrame_.width() + sourceFrame_.width()) % sourceFrame_.width();
            srcY = qBound(0, srcY, sourceFrame_.height() - 1);
            scan[x] = sourceFrame_.pixel(srcX, srcY);
        }
    }

    setPixmap(QPixmap::fromImage(output));
}

void VideoPreviewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (dragging_) {
        const QPoint delta = event->pos() - lastPos_;
        lastPos_ = event->pos();
        emit panDragged(delta.x(), delta.y());
        event->accept();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void VideoPreviewWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (dragging_ && event->button() == Qt::LeftButton) {
        dragging_ = false;
        setCursor(panEnabled_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}
