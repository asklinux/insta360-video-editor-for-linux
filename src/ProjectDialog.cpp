#include "ProjectDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

ProjectDialog::ProjectDialog(QWidget *parent, const Project *existingProject)
    : QDialog(parent)
{
    editing_ = existingProject != nullptr;
    if (existingProject) {
        baseProject_ = *existingProject;
    }
    setWindowTitle(editing_ ? QStringLiteral("Tetapan Projek dan MediaSDK") : QStringLiteral("Cipta Projek Insta360"));
    setModal(true);
    resize(820, 760);

    auto *root = new QVBoxLayout(this);

    auto *projectBox = new QGroupBox(QStringLiteral("Projek"));
    auto *projectForm = new QFormLayout(projectBox);

    nameEdit_ = new QLineEdit(QStringLiteral("Projek Insta360"));
    directoryEdit_ = new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                                   + QStringLiteral("/Insta360EditorProject"));
    auto *dirRow = new QWidget;
    auto *dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->addWidget(directoryEdit_);
    auto *browseDirButton = new QPushButton(QStringLiteral("Pilih"));
    dirLayout->addWidget(browseDirButton);

    projectForm->addRow(QStringLiteral("Nama projek"), nameEdit_);
    projectForm->addRow(QStringLiteral("Lokasi simpan"), dirRow);
    root->addWidget(projectBox);

    auto *videoBox = new QGroupBox(QStringLiteral("Format Export"));
    auto *videoGrid = new QGridLayout(videoBox);

    formatCombo_ = new QComboBox;
    formatCombo_->addItem(QStringLiteral("360 Panorama / Equirectangular"), static_cast<int>(ProjectFormat::Panorama360));
    formatCombo_->addItem(QStringLiteral("Video Standard"), static_cast<int>(ProjectFormat::StandardVideo));

    resolutionCombo_ = new QComboBox;
    fpsCombo_ = new QComboBox;
    fpsCombo_->addItem(QStringLiteral("23.976 fps"), 24);
    fpsCombo_->addItem(QStringLiteral("24 fps"), 24);
    fpsCombo_->addItem(QStringLiteral("25 fps"), 25);
    fpsCombo_->addItem(QStringLiteral("29.97 fps"), 30);
    fpsCombo_->addItem(QStringLiteral("30 fps"), 30);
    fpsCombo_->addItem(QStringLiteral("50 fps"), 50);
    fpsCombo_->addItem(QStringLiteral("59.94 fps"), 60);
    fpsCombo_->addItem(QStringLiteral("60 fps"), 60);
    fpsCombo_->setCurrentIndex(4);

    bitrateSpin_ = new QSpinBox;
    bitrateSpin_->setRange(1, 400);
    bitrateSpin_->setValue(60);
    bitrateSpin_->setSuffix(QStringLiteral(" Mbps"));
    bitrateSlider_ = new QSlider(Qt::Horizontal);
    bitrateSlider_->setRange(1, 400);
    bitrateSlider_->setValue(60);
    bitrateSlider_->setPageStep(10);
    auto *bitrateRow = new QWidget;
    auto *bitrateLayout = new QHBoxLayout(bitrateRow);
    bitrateLayout->setContentsMargins(0, 0, 0, 0);
    bitrateLayout->setSpacing(8);
    bitrateLayout->addWidget(bitrateSlider_, 1);
    bitrateLayout->addWidget(bitrateSpin_);

    codecCombo_ = new QComboBox;
    codecCombo_->addItem(QStringLiteral("H.264"), QStringLiteral("h264"));
    codecCombo_->addItem(QStringLiteral("H.265 / HEVC"), QStringLiteral("h265"));

    videoGrid->addWidget(new QLabel(QStringLiteral("Jenis")), 0, 0);
    videoGrid->addWidget(formatCombo_, 0, 1, 1, 3);
    videoGrid->addWidget(new QLabel(QStringLiteral("Resolusi")), 1, 0);
    videoGrid->addWidget(resolutionCombo_, 1, 1, 1, 3);
    videoGrid->addWidget(new QLabel(QStringLiteral("FPS")), 2, 0);
    videoGrid->addWidget(fpsCombo_, 2, 1);
    videoGrid->addWidget(new QLabel(QStringLiteral("Bitrate Mbps")), 2, 2);
    videoGrid->addWidget(bitrateRow, 2, 3);
    videoGrid->addWidget(new QLabel(QStringLiteral("Codec")), 3, 0);
    videoGrid->addWidget(codecCombo_, 3, 1, 1, 3);
    root->addWidget(videoBox);

    auto *sdkBox = new QGroupBox(QStringLiteral("Insta360 Stitching"));
    auto *sdkGrid = new QGridLayout(sdkBox);

    stitchCombo_ = new QComboBox;
    stitchCombo_->addItem(QStringLiteral("Optical Flow"), static_cast<int>(StitchType::OptFlow));
    stitchCombo_->addItem(QStringLiteral("Dynamic Stitch"), static_cast<int>(StitchType::DynamicStitch));
    stitchCombo_->addItem(QStringLiteral("Template"), static_cast<int>(StitchType::Template));
    stitchCombo_->addItem(QStringLiteral("AI Flow"), static_cast<int>(StitchType::AiFlow));

    flowStateCheck_ = new QCheckBox(QStringLiteral("FlowState stabilization"));
    flowStateCheck_->setChecked(true);
    directionLockCheck_ = new QCheckBox(QStringLiteral("Direction lock"));
    cudaCheck_ = new QCheckBox(QStringLiteral("CUDA/GPU"));
    cudaCheck_->setChecked(true);
    softEncodeCheck_ = new QCheckBox(QStringLiteral("Software encode"));
    softDecodeCheck_ = new QCheckBox(QStringLiteral("Software decode"));
    tenBitCheck_ = new QCheckBox(QStringLiteral("Export 10-bit (H.265 automatik)"));
    imageProcessingCpuCheck_ = new QCheckBox(QStringLiteral("Render CPU (atasi ralat Vulkan)"));
    exportStabilizationDataCheck_ = new QCheckBox(QStringLiteral("Export data stabilisasi (.stab)"));

    cameraAccessoryCombo_ = new QComboBox;
    cameraAccessoryCombo_->addItem(QStringLiteral("Auto detect"), -1);
    cameraAccessoryCombo_->addItem(QStringLiteral("Tiada aksesori"), 0);
    const QStringList accessories {
        QStringLiteral("Waterproof case"), QStringLiteral("ONE R adhesive lens guard"),
        QStringLiteral("ONE R lens guard pro"), QStringLiteral("ONE X2 adhesive lens guard"),
        QStringLiteral("ONE X2 lens guard pro"), QStringLiteral("283 pano lens guard pro"),
        QStringLiteral("Dive case - udara"), QStringLiteral("Dive case - bawah air"),
        QStringLiteral("Invisible dive case - udara"), QStringLiteral("Invisible dive case - bawah air"),
        QStringLiteral("Lens Guard Grade A"), QStringLiteral("Lens Guard Grade S"),
        QStringLiteral("Lens Guard A/S auto"), QStringLiteral("X5 ND16"), QStringLiteral("X5 ND32"),
        QStringLiteral("X5 ND64"), QStringLiteral("ONE R 283 lens guard pro"),
        QStringLiteral("ONE R FPV lens guard"), QStringLiteral("X4 Air dive case - udara"),
        QStringLiteral("X4 Air dive case - bawah air")
    };
    for (int i = 0; i < accessories.size(); ++i) {
        cameraAccessoryCombo_->addItem(accessories[i], i + 1);
    }

    sdkLogLevelCombo_ = new QComboBox;
    sdkLogLevelCombo_->addItem(QStringLiteral("Error sahaja"), QStringLiteral("error"));
    sdkLogLevelCombo_->addItem(QStringLiteral("Warning"), QStringLiteral("warning"));
    sdkLogLevelCombo_->addItem(QStringLiteral("Info"), QStringLiteral("info"));
    sdkLogLevelCombo_->addItem(QStringLiteral("Verbose / debug"), QStringLiteral("verbose"));
    sdkLogLevelCombo_->addItem(QStringLiteral("Fatal sahaja"), QStringLiteral("fatal"));
    sdkLogPathEdit_ = new QLineEdit;
    sdkLogPathEdit_->setPlaceholderText(QStringLiteral("Kosong = log konsol sahaja"));

    aiModelEdit_ = new QLineEdit;
    auto *aiModelButton = new QPushButton(QStringLiteral("Pilih"));
    auto *modelRow = new QWidget;
    auto *modelLayout = new QHBoxLayout(modelRow);
    modelLayout->setContentsMargins(0, 0, 0, 0);
    modelLayout->addWidget(aiModelEdit_);
    modelLayout->addWidget(aiModelButton);

    sdkGrid->addWidget(new QLabel(QStringLiteral("Stitch type")), 0, 0);
    sdkGrid->addWidget(stitchCombo_, 0, 1, 1, 3);
    sdkGrid->addWidget(flowStateCheck_, 1, 0, 1, 2);
    sdkGrid->addWidget(directionLockCheck_, 1, 2, 1, 2);
    sdkGrid->addWidget(cudaCheck_, 2, 0);
    sdkGrid->addWidget(softEncodeCheck_, 2, 1);
    sdkGrid->addWidget(softDecodeCheck_, 2, 2);
    sdkGrid->addWidget(tenBitCheck_, 3, 0, 1, 2);
    sdkGrid->addWidget(imageProcessingCpuCheck_, 3, 2, 1, 2);
    sdkGrid->addWidget(exportStabilizationDataCheck_, 4, 0, 1, 2);
    sdkGrid->addWidget(new QLabel(QStringLiteral("Lens/accessory")), 5, 0);
    sdkGrid->addWidget(cameraAccessoryCombo_, 5, 1, 1, 3);
    sdkGrid->addWidget(new QLabel(QStringLiteral("SDK log level")), 6, 0);
    sdkGrid->addWidget(sdkLogLevelCombo_, 6, 1);
    sdkGrid->addWidget(new QLabel(QStringLiteral("Log folder")), 6, 2);
    sdkGrid->addWidget(sdkLogPathEdit_, 6, 3);
    sdkGrid->addWidget(new QLabel(QStringLiteral("Folder model (pilihan)")), 7, 0);
    sdkGrid->addWidget(modelRow, 7, 1, 1, 3);
    root->addWidget(sdkBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->button(QDialogButtonBox::Ok)->setText(editing_ ? QStringLiteral("Simpan Tetapan") : QStringLiteral("Create Project"));
    root->addWidget(buttons);

    connect(browseDirButton, &QPushButton::clicked, this, &ProjectDialog::chooseProjectDirectory);
    connect(aiModelButton, &QPushButton::clicked, this, &ProjectDialog::chooseAiModel);
    connect(bitrateSlider_, &QSlider::valueChanged, bitrateSpin_, &QSpinBox::setValue);
    connect(bitrateSpin_, QOverload<int>::of(&QSpinBox::valueChanged), bitrateSlider_, &QSlider::setValue);
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectDialog::updateResolutionForFormat);
    connect(resolutionCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProjectDialog::updateResolutionForFormat);
    connect(tenBitCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled) codecCombo_->setCurrentIndex(codecCombo_->findData(QStringLiteral("h265")));
        codecCombo_->setEnabled(!enabled);
    });
    connect(flowStateCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        directionLockCheck_->setEnabled(enabled);
        if (!enabled) directionLockCheck_->setChecked(false);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateResolutionForFormat();
    directionLockCheck_->setEnabled(flowStateCheck_->isChecked());

    if (editing_) {
        nameEdit_->setText(baseProject_.name);
        directoryEdit_->setText(baseProject_.projectDir);
        nameEdit_->setEnabled(false);
        directoryEdit_->setEnabled(false);
        browseDirButton->setEnabled(false);
        const ExportSettings &s = baseProject_.settings;
        formatCombo_->setCurrentIndex(formatCombo_->findData(static_cast<int>(s.format)));
        updateResolutionForFormat();
        for (int i = 0; i < resolutionCombo_->count(); ++i) {
            if (resolutionCombo_->itemData(i).toSize() == QSize(s.width, s.height)) resolutionCombo_->setCurrentIndex(i);
        }
        fpsCombo_->setCurrentIndex(qMax(0, fpsCombo_->findData(s.fps)));
        bitrateSpin_->setValue(s.bitrateMbps);
        codecCombo_->setCurrentIndex(qMax(0, codecCombo_->findData(s.codec)));
        stitchCombo_->setCurrentIndex(qMax(0, stitchCombo_->findData(static_cast<int>(s.stitchType))));
        flowStateCheck_->setChecked(s.flowState);
        directionLockCheck_->setChecked(s.directionLock);
        cudaCheck_->setChecked(s.cuda);
        softEncodeCheck_->setChecked(s.softEncode);
        softDecodeCheck_->setChecked(s.softDecode);
        tenBitCheck_->setChecked(s.tenBit);
        imageProcessingCpuCheck_->setChecked(s.imageProcessingCpu);
        exportStabilizationDataCheck_->setChecked(s.exportStabilizationData);
        cameraAccessoryCombo_->setCurrentIndex(qMax(0, cameraAccessoryCombo_->findData(s.cameraAccessoryType)));
        sdkLogLevelCombo_->setCurrentIndex(qMax(0, sdkLogLevelCombo_->findData(s.sdkLogLevel)));
        sdkLogPathEdit_->setText(s.sdkLogPath);
        aiModelEdit_->setText(s.aiModelPath);
    }
}

Project ProjectDialog::createProject() const
{
    Project project = baseProject_;
    project.name = nameEdit_->text().trimmed();
    if (project.name.isEmpty()) {
        project.name = QStringLiteral("Insta360 Project");
    }
    project.projectDir = QDir::cleanPath(directoryEdit_->text());
    project.projectFile = QDir(project.projectDir).filePath(project.name + QStringLiteral(".i360proj"));

    ExportSettings settings;
    settings.format = static_cast<ProjectFormat>(formatCombo_->currentData().toInt());
    const QSize resolution = resolutionCombo_->currentData().toSize();
    settings.width = resolution.width();
    settings.height = resolution.height();
    settings.fps = fpsCombo_->currentData().toInt();
    settings.bitrateMbps = bitrateSpin_->value();
    settings.codec = codecCombo_->currentData().toString();
    settings.stitchType = static_cast<StitchType>(stitchCombo_->currentData().toInt());
    settings.flowState = flowStateCheck_->isChecked();
    settings.directionLock = directionLockCheck_->isChecked();
    settings.cuda = cudaCheck_->isChecked();
    settings.softEncode = softEncodeCheck_->isChecked();
    settings.softDecode = softDecodeCheck_->isChecked();
    settings.tenBit = tenBitCheck_->isChecked();
    settings.imageProcessingCpu = imageProcessingCpuCheck_->isChecked();
    settings.exportStabilizationData = exportStabilizationDataCheck_->isChecked();
    settings.cameraAccessoryType = cameraAccessoryCombo_->currentData().toInt();
    settings.sdkLogLevel = sdkLogLevelCombo_->currentData().toString();
    settings.sdkLogPath = sdkLogPathEdit_->text().trimmed();
    settings.aiModelPath = aiModelEdit_->text().trimmed();
    project.settings = settings;

    return project;
}

void ProjectDialog::chooseProjectDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Pilih lokasi projek"), directoryEdit_->text());
    if (!dir.isEmpty()) {
        directoryEdit_->setText(dir);
    }
}

void ProjectDialog::chooseAiModel()
{
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Pilih folder model MediaSDK"));
    if (!path.isEmpty()) {
        aiModelEdit_->setText(path);
    }
}

void ProjectDialog::updateResolutionForFormat()
{
    const auto format = static_cast<ProjectFormat>(formatCombo_->currentData().toInt());
    resolutionCombo_->blockSignals(true);
    resolutionCombo_->clear();
    if (format == ProjectFormat::Panorama360) {
        resolutionCombo_->addItem(QStringLiteral("3840 x 1920 (4K 360)"), QSize(3840, 1920));
        resolutionCombo_->addItem(QStringLiteral("5760 x 2880 (5.7K 360)"), QSize(5760, 2880));
        resolutionCombo_->addItem(QStringLiteral("7680 x 3840 (8K 360)"), QSize(7680, 3840));
    } else {
        resolutionCombo_->addItem(QStringLiteral("1280 x 720 (HD)"), QSize(1280, 720));
        resolutionCombo_->addItem(QStringLiteral("1920 x 1080 (Full HD)"), QSize(1920, 1080));
        resolutionCombo_->addItem(QStringLiteral("2560 x 1440 (QHD)"), QSize(2560, 1440));
        resolutionCombo_->addItem(QStringLiteral("3840 x 2160 (UHD 4K)"), QSize(3840, 2160));
    }
    resolutionCombo_->blockSignals(false);
}
