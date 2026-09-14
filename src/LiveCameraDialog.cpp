#include "LiveCameraDialog.h"

#include <camera/camera.h>
#include <camera/device_discovery.h>
#include <ins_realtime_stitcher.h>
#include <ins_stitcher.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QStyle>
#include <QThread>
#include <QVBoxLayout>

#include <cstring>
#include <memory>

namespace {

class StitchStreamDelegate final : public ins_camera::StreamDelegate {
public:
    explicit StitchStreamDelegate(std::shared_ptr<ins::RealTimeStitcher> stitcher)
        : stitcher_(std::move(stitcher)) {}

    void OnAudioData(const uint8_t *, size_t, int64_t) override {}
    void OnVideoData(const uint8_t *data, size_t size, int64_t timestamp, uint8_t type, int index) override
    { stitcher_->HandleVideoData(data, size, timestamp, type, index); }
    void OnGyroData(const std::vector<ins_camera::GyroData> &data) override
    {
        std::vector<ins::GyroData> converted(data.size());
        static_assert(sizeof(ins::GyroData) == sizeof(ins_camera::GyroData));
        std::memcpy(converted.data(), data.data(), data.size() * sizeof(ins::GyroData));
        stitcher_->HandleGyroData(converted);
    }
    void OnExposureData(const ins_camera::ExposureData &data) override
    {
        ins::ExposureData converted {};
        converted.timestamp = data.timestamp;
        converted.exposure_time = data.exposure_time;
        stitcher_->HandleExposureData(converted);
    }

private:
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
};

class LiveCameraController final : public QObject {
    Q_OBJECT
public slots:
    void start(int width, int height, int stitchType, int accessoryType, int delayMs, int bitrate,
               bool flowState, bool directionLock, bool defringe, bool deflicker, bool softDecode)
    {
        if (camera_) return;
        emit statusChanged(tr("Mencari kamera Insta360 melalui USB/Wi-Fi…"), false);

        static bool initialized = false;
        if (!initialized) {
            ins::SetLogLevel(ins::InsLogLevel::WARNING);
            ins::InitEnv();
#ifdef INSTA360_DEFAULT_MODEL_DIR
            ins::SetModelFileRootDir(std::string(INSTA360_DEFAULT_MODEL_DIR) + "/");
#endif
            initialized = true;
        }
        ins_camera::SetLogLevel(ins_camera::LogLevel::WARNING);

        ins_camera::DeviceDiscovery discovery;
        auto devices = discovery.GetAvailableDevices();
        if (devices.empty()) {
            emit statusChanged(tr("Tiada kamera ditemui. Sambungkan kamera dan pastikan ia tidak digunakan aplikasi lain."), true);
            return;
        }
        const QString identity = QString::fromStdString(devices.front().camera_name) + QStringLiteral(" · ")
            + QString::fromStdString(devices.front().serial_number) + QStringLiteral(" · FW ")
            + QString::fromStdString(devices.front().fw_version);
        camera_ = std::make_shared<ins_camera::Camera>(devices.front().info);
        if (!camera_->Open()) {
            discovery.FreeDeviceDescriptors(devices);
            camera_.reset();
            emit statusChanged(tr("Kamera ditemui tetapi gagal dibuka."), true);
            return;
        }
        discovery.FreeDeviceDescriptors(devices);

        auto preview = camera_->GetPreviewParam();
        ins::CameraInfo info;
        info.cameraName = preview.camera_name;
        info.decode_type = static_cast<ins::VideoDecodeType>(preview.encode_type);
        info.gyro_timestamp = preview.delay_timestamp;
        info.sweep_timestamp = preview.sweep_time;
        std::vector<std::string> calibration;
        for (size_t i = 0; i < preview.GetCalibrationCount(); ++i) calibration.push_back(preview.GetCalibration(i));
        info.SetCalibration(calibration, preview.GetCropSrcWidth(), preview.GetCropSrcHeight(),
            preview.GetCropDstWidth(), preview.GetCropDstHeight(), preview.GetCropOffsetX(), preview.GetCropOffsetY());

        stitcher_ = std::make_shared<ins::RealTimeStitcher>();
        stitcher_->SetCameraInfo(info);
        stitcher_->SetOutputSize(width, height);
        stitcher_->SetStitchType(static_cast<ins::STITCH_TYPE>(stitchType));
        stitcher_->SetCameraAccessoryType(static_cast<ins::CameraAccessoryType>(accessoryType));
        stitcher_->SetVideoDelayMs(delayMs);
        stitcher_->EnableFlowState(flowState);
        stitcher_->EnableDirectionLock(directionLock);
        stitcher_->EnableDefringe(defringe);
        stitcher_->EnableDeflicker(deflicker);
        stitcher_->SetSoftwareCodecUsage(false, softDecode);
        stitcher_->SetStitchStateCallback([this](int code, const char *message) {
            emit statusChanged(tr("MediaSDK %1: %2").arg(code).arg(QString::fromUtf8(message ? message : "")), true);
        });
        stitcher_->SetStitchRealTimeDataCallback([this](uint8_t *data[4], int linesize[4], int w, int h, int, int64_t) {
            if (data[0]) emit frameReady(QImage(data[0], w, h, linesize[0], QImage::Format_RGBA8888).copy());
        });
        delegate_ = std::make_shared<StitchStreamDelegate>(stitcher_);
        std::shared_ptr<ins_camera::StreamDelegate> baseDelegate = delegate_;
        camera_->SetStreamDelegate(baseDelegate);

        ins_camera::LiveStreamParam stream;
        stream.video_resolution = ins_camera::RES_1440_720P30;
        stream.lrv_video_resulution = ins_camera::RES_1440_720P30;
        stream.video_bitrate = static_cast<uint32_t>(bitrate);
        stream.enable_audio = false;
        stream.enable_gyro = true;
        stream.using_lrv = false;
        if (!camera_->StartLiveStreaming(stream)) {
            camera_->Close();
            camera_.reset(); stitcher_.reset(); delegate_.reset();
            emit statusChanged(tr("Gagal memulakan live stream kamera."), true);
            return;
        }
        stitcher_->StartStitch();
        emit statusChanged(identity + tr(" · Live"), false);
        emit runningChanged(true);
    }

    void stop()
    {
        if (!camera_) return;
        camera_->StopLiveStreaming();
        if (stitcher_) stitcher_->CancelStitch();
        camera_->Close();
        delegate_.reset(); stitcher_.reset(); camera_.reset();
        emit statusChanged(tr("Live preview dihentikan."), false);
        emit runningChanged(false);
    }

signals:
    void frameReady(const QImage &image);
    void statusChanged(const QString &message, bool error);
    void runningChanged(bool running);

private:
    std::shared_ptr<ins_camera::Camera> camera_;
    std::shared_ptr<ins::RealTimeStitcher> stitcher_;
    std::shared_ptr<StitchStreamDelegate> delegate_;
};

} // namespace

LiveCameraDialog::LiveCameraDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Insta360 Live Camera · RealTimeStitcher"));
    resize(1040, 720);
    auto *root = new QVBoxLayout(this);
    preview_ = new QLabel(QStringLiteral("Tekan Start Live untuk mencari kamera"));
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setMinimumSize(800, 400);
    preview_->setStyleSheet(QStringLiteral("background:#050608;border:1px solid #333;color:#8993a3"));
    root->addWidget(preview_, 1);

    auto *controls = new QFormLayout;
    resolution_ = new QComboBox;
    resolution_->addItem(QStringLiteral("960 × 480 (latency rendah)"), QSize(960, 480));
    resolution_->addItem(QStringLiteral("1280 × 640"), QSize(1280, 640));
    resolution_->addItem(QStringLiteral("1920 × 960"), QSize(1920, 960));
    stitchType_ = new QComboBox;
    stitchType_->addItem(QStringLiteral("Dynamic Stitch (disyorkan)"), static_cast<int>(ins::STITCH_TYPE::DYNAMICSTITCH));
    stitchType_->addItem(QStringLiteral("Template (pantas)"), static_cast<int>(ins::STITCH_TYPE::TEMPLATE));
    stitchType_->addItem(QStringLiteral("Optical Flow"), static_cast<int>(ins::STITCH_TYPE::OPTFLOW));
    stitchType_->addItem(QStringLiteral("AI Flow (berat)"), static_cast<int>(ins::STITCH_TYPE::AIFLOW));
    accessory_ = new QComboBox;
    accessory_->addItem(QStringLiteral("Auto detect"), -1);
    accessory_->addItem(QStringLiteral("Tiada aksesori"), 0);
    accessory_->addItem(QStringLiteral("Dive case - udara"), 7);
    accessory_->addItem(QStringLiteral("Dive case - bawah air"), 8);
    accessory_->addItem(QStringLiteral("Invisible dive case - udara"), 9);
    accessory_->addItem(QStringLiteral("Invisible dive case - bawah air"), 10);
    accessory_->addItem(QStringLiteral("Lens Guard A"), 11);
    accessory_->addItem(QStringLiteral("Lens Guard S"), 12);
    delay_ = new QSpinBox; delay_->setRange(0, 2000); delay_->setSuffix(QStringLiteral(" ms"));
    bitrate_ = new QSpinBox; bitrate_->setRange(1, 100); bitrate_->setValue(10); bitrate_->setSuffix(QStringLiteral(" Mbps"));
    flowState_ = new QCheckBox(QStringLiteral("FlowState")); flowState_->setChecked(true);
    directionLock_ = new QCheckBox(QStringLiteral("Direction lock"));
    defringe_ = new QCheckBox(QStringLiteral("Defringe"));
    deflicker_ = new QCheckBox(QStringLiteral("Deflicker"));
    softDecode_ = new QCheckBox(QStringLiteral("Software decode"));
    controls->addRow(QStringLiteral("Output"), resolution_);
    controls->addRow(QStringLiteral("Stitch"), stitchType_);
    controls->addRow(QStringLiteral("Accessory"), accessory_);
    controls->addRow(QStringLiteral("Video delay"), delay_);
    controls->addRow(QStringLiteral("Camera bitrate"), bitrate_);
    auto *features = new QHBoxLayout;
    features->addWidget(flowState_); features->addWidget(directionLock_); features->addWidget(defringe_);
    features->addWidget(deflicker_); features->addWidget(softDecode_); features->addStretch();
    controls->addRow(QStringLiteral("Features"), features);
    root->addLayout(controls);

    auto *buttons = new QHBoxLayout;
    status_ = new QLabel(QStringLiteral("Sedia")); status_->setWordWrap(true);
    start_ = new QPushButton(QStringLiteral("Start Live"));
    start_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    stop_ = new QPushButton(QStringLiteral("Stop")); stop_->setEnabled(false);
    stop_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    buttons->addWidget(status_, 1); buttons->addWidget(start_); buttons->addWidget(stop_);
    root->addLayout(buttons);

    workerThread_ = new QThread(this);
    auto *controller = new LiveCameraController;
    controller_ = controller;
    controller->moveToThread(workerThread_);
    connect(workerThread_, &QThread::finished, controller, &QObject::deleteLater);
    connect(this, &LiveCameraDialog::startRequested, controller, &LiveCameraController::start);
    connect(this, &LiveCameraDialog::stopRequested, controller, &LiveCameraController::stop);
    connect(controller, &LiveCameraController::frameReady, this, [this](const QImage &image) {
        preview_->setPixmap(QPixmap::fromImage(image).scaled(preview_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    });
    connect(controller, &LiveCameraController::statusChanged, this, [this](const QString &message, bool error) {
        status_->setText(message); status_->setStyleSheet(error ? QStringLiteral("color:#ff6b6b") : QString());
    });
    connect(controller, &LiveCameraController::runningChanged, this, [this](bool running) {
        start_->setEnabled(!running); stop_->setEnabled(running);
    });
    connect(start_, &QPushButton::clicked, this, [this]() {
        const QSize size = resolution_->currentData().toSize();
        emit startRequested(size.width(), size.height(), stitchType_->currentData().toInt(), accessory_->currentData().toInt(),
            delay_->value(), bitrate_->value() * 1000 * 1000, flowState_->isChecked(), directionLock_->isChecked(),
            defringe_->isChecked(), deflicker_->isChecked(), softDecode_->isChecked());
    });
    connect(stop_, &QPushButton::clicked, this, &LiveCameraDialog::stopRequested);
    workerThread_->start();
}

LiveCameraDialog::~LiveCameraDialog()
{
    if (controller_ && workerThread_->isRunning()) {
        QMetaObject::invokeMethod(controller_, "stop", Qt::BlockingQueuedConnection);
        workerThread_->quit();
        workerThread_->wait();
    }
}

#include "LiveCameraDialog.moc"
