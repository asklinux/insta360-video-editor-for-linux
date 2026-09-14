#include "MainWindow.h"

#include "ExportWorker.h"
#ifdef INSTA360_HAS_LIVE_CAMERA
#include "LiveCameraDialog.h"
#endif
#include "ProjectDialog.h"
#include "TimelineWidget.h"
#include "VideoPreviewWidget.h"

#include <QAction>
#include <QApplication>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QtMath>

namespace {

QSlider *addSliderSpinRow(QFormLayout *form, const QString &label, QSpinBox *spin, int minimum, int maximum, int step = 1)
{
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);

    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(minimum, maximum);
    slider->setSingleStep(step);
    slider->setPageStep(qMax(step, (maximum - minimum) / 10));

    layout->addWidget(slider, 1);
    layout->addWidget(spin);
    form->addRow(label, row);

    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QSlider::setValue);
    return slider;
}

QSlider *addDoubleSliderSpinRow(QFormLayout *form, const QString &label, QDoubleSpinBox *spin, double minimum, double maximum, int factor)
{
    spin->setRange(minimum, maximum);
    spin->setDecimals(2);

    auto *row = new QWidget;
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(qRound(minimum * factor), qRound(maximum * factor));

    layout->addWidget(slider, 1);
    layout->addWidget(spin);
    form->addRow(label, row);

    QObject::connect(slider, &QSlider::valueChanged, spin, [spin, factor](int value) {
        spin->setValue(value / static_cast<double>(factor));
    });
    QObject::connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), slider, [slider, factor](double value) {
        slider->setValue(qRound(value * factor));
    });
    return slider;
}

int wrappedDegrees(int value)
{
    while (value > 180) {
        value -= 360;
    }
    while (value < -180) {
        value += 360;
    }
    return value;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildMenus();
    stack_ = new QStackedWidget;
    startPage_ = buildStartPage();
    editorPage_ = buildEditorPage();
    stack_->addWidget(startPage_);
    stack_->addWidget(editorPage_);
    setCentralWidget(stack_);
    applyStyle();

    previewTimer_ = new QTimer(this);
    previewTimer_->setInterval(250);
    connect(previewTimer_, &QTimer::timeout, this, &MainWindow::renderNextPreviewFrame);

    inspectorPreviewTimer_ = new QTimer(this);
    inspectorPreviewTimer_->setSingleShot(true);
    inspectorPreviewTimer_->setInterval(220);
    connect(inspectorPreviewTimer_, &QTimer::timeout, this, &MainWindow::renderSelectedTimelinePreview);
}

QWidget *MainWindow::buildStartPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(80, 70, 80, 70);
    layout->setSpacing(22);

    auto *title = new QLabel(QStringLiteral("Insta360 Studio Editor"));
    title->setObjectName(QStringLiteral("StartTitle"));
    auto *subtitle = new QLabel(QStringLiteral("Cipta projek video standard atau 360 panorama, import media Insta360, susun timeline, kemudian export ke MP4."));
    subtitle->setWordWrap(true);
    subtitle->setObjectName(QStringLiteral("StartSubtitle"));

    auto *createButton = new QPushButton(QStringLiteral("Create Project"));
    createButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    createButton->setObjectName(QStringLiteral("PrimaryButton"));
    createButton->setMinimumHeight(48);
    auto *openButton = new QPushButton(QStringLiteral("Open Project"));
    openButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    openButton->setMinimumHeight(44);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(createButton);
    buttonRow->addWidget(openButton);
    buttonRow->addStretch();

    auto *details = new QLabel(QStringLiteral("Disokong: ONE X, ONE R/RS, ONE X2, X3, X4, X4 Air dan X5 untuk bahan panorama seperti yang dinyatakan oleh Desktop MediaSDK C++."));
    details->setWordWrap(true);
    details->setObjectName(QStringLiteral("MutedText"));

    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addLayout(buttonRow);
    layout->addWidget(details);
    layout->addStretch();

    connect(createButton, &QPushButton::clicked, this, &MainWindow::newProject);
    connect(openButton, &QPushButton::clicked, this, &MainWindow::openProjectDialog);
    return page;
}

QWidget *MainWindow::buildEditorPage()
{
    auto *page = new QWidget;
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(14, 9, 14, 9);
    toolbar->setSpacing(8);
    projectSummary_ = new QLabel;
    projectSummary_->setObjectName(QStringLiteral("ProjectSummary"));
    auto *saveButton = new QPushButton(QStringLiteral("Save"));
    saveButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    auto *exportButton = new QPushButton(QStringLiteral("Export MP4"));
    exportButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    exportButton->setObjectName(QStringLiteral("PrimaryButton"));
    toolbar->addWidget(projectSummary_, 1);
    toolbar->addWidget(saveButton);
    toolbar->addWidget(exportButton);
    root->addLayout(toolbar);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setObjectName(QStringLiteral("MainSplitter"));

    auto *leftPanel = new QWidget;
    leftPanel->setObjectName(QStringLiteral("SidePanel"));
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12);
    leftLayout->setSpacing(10);
    auto *mediaTitle = new QLabel(QStringLiteral("Media Library"));
    mediaTitle->setObjectName(QStringLiteral("PanelTitle"));
    auto *importButton = new QPushButton(QStringLiteral("Import Media"));
    importButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    auto *cameraButton = new QPushButton(QStringLiteral("Browse Camera (No Download)"));
    cameraButton->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
    cameraButton->setToolTip(QStringLiteral("Papar senarai media kamera tanpa memuat turun fail"));
    auto *addButton = new QPushButton(QStringLiteral("Add To Timeline"));
    addButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    mediaList_ = new QListWidget;
    mediaList_->setDragEnabled(true);
    mediaList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mediaList_->setMinimumWidth(260);
    leftLayout->addWidget(mediaTitle);
    leftLayout->addWidget(importButton);
    leftLayout->addWidget(cameraButton);
    leftLayout->addWidget(mediaList_);
    leftLayout->addWidget(addButton);

    auto *centerPanel = new QWidget;
    centerPanel->setObjectName(QStringLiteral("PreviewPanel"));
    auto *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(14, 12, 14, 12);
    centerLayout->setSpacing(10);
    previewTitle_ = new QLabel(QStringLiteral("Video Preview"));
    previewTitle_->setObjectName(QStringLiteral("PanelTitle"));
    previewLabel_ = new VideoPreviewWidget;
    previewLabel_->setText(QStringLiteral("Pilih media untuk preview"));
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(640, 360);
    previewLabel_->setObjectName(QStringLiteral("Preview"));
    previewLabel_->setScaledContents(false);
    previewLabel_->setToolTip(QStringLiteral("Dalam mode Interactive 360 Mouse View, drag mouse untuk pusing pandangan 360."));

    auto *previewModeRow = new QHBoxLayout;
    previewModeRow->setSpacing(8);
    previewViewCombo_ = new QComboBox;
    previewViewCombo_->addItems({
        QStringLiteral("Source / Equirectangular"),
        QStringLiteral("360 Panorama View"),
        QStringLiteral("Interactive 360 Mouse View")
    });
    reset360ViewButton_ = new QPushButton(QStringLiteral("Reset 360 View"));
    reset360ViewButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    previewModeRow->addWidget(previewViewCombo_, 1);
    previewModeRow->addWidget(reset360ViewButton_);

    auto *previewControls = new QHBoxLayout;
    playButton_ = new QPushButton(QStringLiteral("Play"));
    playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    previewSlider_ = new QSlider(Qt::Horizontal);
    previewSlider_->setRange(0, 600);
    previewControls->addWidget(playButton_);
    previewControls->addWidget(previewSlider_, 1);

    centerLayout->addWidget(previewTitle_);
    centerLayout->addLayout(previewModeRow);
    centerLayout->addWidget(previewLabel_, 1);
    centerLayout->addLayout(previewControls);

    QWidget *inspector = buildInspector();

    splitter->addWidget(leftPanel);
    splitter->addWidget(centerPanel);
    splitter->addWidget(inspector);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({280, 860, 300});
    root->addWidget(splitter, 1);

    auto *timelineTitle = new QLabel(QStringLiteral("Timeline"));
    timelineTitle->setObjectName(QStringLiteral("PanelTitle"));
    timeline_ = new TimelineWidget;
    timeline_->setObjectName(QStringLiteral("Timeline"));
    exportProgress_ = new QProgressBar;
    exportProgress_->setRange(0, 100);
    exportProgress_->setValue(0);
    cancelExportButton_ = new QPushButton(QStringLiteral("Cancel Export"));
    cancelExportButton_->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    cancelExportButton_->setEnabled(false);

    auto *timelinePanel = new QWidget;
    timelinePanel->setObjectName(QStringLiteral("TimelinePanel"));
    auto *timelineLayout = new QVBoxLayout(timelinePanel);
    timelineLayout->setContentsMargins(14, 10, 14, 12);
    timelineLayout->setSpacing(8);
    timelineLayout->addWidget(timelineTitle);
    timelineLayout->addWidget(timeline_);
    timelineLayout->addWidget(exportProgress_);
    timelineLayout->addWidget(cancelExportButton_);

    root->addWidget(timelinePanel);

    connect(importButton, &QPushButton::clicked, this, &MainWindow::importMediaDialog);
    connect(cameraButton, &QPushButton::clicked, this, &MainWindow::importFromCamera);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addSelectedMediaToTimeline);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentProject);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportProject);
    connect(mediaList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        addMediaToTimeline(item->data(Qt::UserRole + 1).toString());
    });
    connect(mediaList_, &QListWidget::itemClicked, this, &MainWindow::previewMedia);
    connect(timeline_, &TimelineWidget::mediaDropped, this, &MainWindow::addMediaToTimeline);
    connect(timeline_, &TimelineWidget::filesDropped, this, &MainWindow::importDroppedFiles);
    connect(timeline_, &QListWidget::currentRowChanged, this, &MainWindow::timelineSelectionChanged);
    connect(previewLabel_, &VideoPreviewWidget::panDragged, this, &MainWindow::previewPanDragged);
    connect(previewViewCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        stopPreviewPlayback();
        if (!projectionCombo_ || !previewModeCombo_) {
            return;
        }
        projectionCombo_->setCurrentIndex(index == 0 ? 0 : 1);
        previewModeCombo_->setCurrentIndex(index == 2 ? 1 : 0);
        previewLabel_->setPanEnabled(index == 2 && project_.settings.format == ProjectFormat::Panorama360);
        renderSelectedTimelinePreview();
    });
    connect(reset360ViewButton_, &QPushButton::clicked, this, [this]() {
        stopPreviewPlayback();
        yawSpin_->setValue(0);
        pitchSpin_->setValue(0);
        rollSpin_->setValue(0);
    });
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::togglePreviewPlayback);
    connect(previewSlider_, &QSlider::valueChanged, this, [this](int value) {
        previewSeconds_ = value;
        if (!previewTimer_->isActive()) {
            renderPreviewFrame(currentPreviewPath(), previewSeconds_);
        }
    });

    return page;
}

QWidget *MainWindow::buildInspector()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("InspectorPanel"));
    panel->setMinimumWidth(280);
    panel->setMaximumWidth(360);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("Inspector"));
    title->setObjectName(QStringLiteral("PanelTitle"));
    inspectorTabs_ = new QTabWidget;
    inspectorTabs_->setObjectName(QStringLiteral("InspectorTabs"));
    inspectorTabs_->addTab(buildVideoInspectorTab(), QStringLiteral("Video"));
    inspectorTabs_->addTab(build360InspectorTab(), QStringLiteral("360"));
    inspectorTabs_->addTab(buildColorInspectorTab(), QStringLiteral("Color"));
    inspectorTabs_->addTab(buildAudioInspectorTab(), QStringLiteral("Audio"));
    inspectorTabs_->addTab(buildEffectsInspectorTab(), QStringLiteral("Effects"));
    inspectorTabs_->addTab(buildSdkInspectorTab(), QStringLiteral("MediaSDK"));

    layout->addWidget(title);
    layout->addWidget(inspectorTabs_, 1);
    return panel;
}

QWidget *MainWindow::buildVideoInspectorTab()
{
    auto *tab = new QWidget;
    auto *form = new QFormLayout(tab);
    form->setContentsMargins(10, 12, 10, 12);
    form->setSpacing(10);

    trimPresetCombo_ = new QComboBox;
    trimPresetCombo_->addItems({QStringLiteral("Manual"), QStringLiteral("First 5s"), QStringLiteral("First 10s"), QStringLiteral("Full clip")});
    form->addRow(QStringLiteral("Trim"), trimPresetCombo_);

    clipInSpin_ = new QDoubleSpinBox;
    clipInSpin_->setSuffix(QStringLiteral(" s"));
    clipOutSpin_ = new QDoubleSpinBox;
    clipOutSpin_->setSpecialValueText(QStringLiteral("Full"));
    clipOutSpin_->setSuffix(QStringLiteral(" s"));
    positionXSpin_ = new QSpinBox;
    positionYSpin_ = new QSpinBox;
    scaleSpin_ = new QSpinBox;
    scaleSpin_->setSuffix(QStringLiteral("%"));
    rotationSpin_ = new QSpinBox;
    rotationSpin_->setSuffix(QStringLiteral(" deg"));
    rotationPresetCombo_ = new QComboBox;
    rotationPresetCombo_->addItems({QStringLiteral("Custom"), QStringLiteral("0 deg"), QStringLiteral("90 deg"), QStringLiteral("180 deg"), QStringLiteral("-90 deg")});

    addDoubleSliderSpinRow(form, QStringLiteral("In"), clipInSpin_, 0.0, 3600.0, 100);
    addDoubleSliderSpinRow(form, QStringLiteral("Out"), clipOutSpin_, 0.0, 3600.0, 100);
    addSliderSpinRow(form, QStringLiteral("Position X"), positionXSpin_, -2048, 2048, 10);
    addSliderSpinRow(form, QStringLiteral("Position Y"), positionYSpin_, -2048, 2048, 10);
    addSliderSpinRow(form, QStringLiteral("Scale"), scaleSpin_, 10, 400, 5);
    form->addRow(QStringLiteral("Rotate preset"), rotationPresetCombo_);
    addSliderSpinRow(form, QStringLiteral("Rotation"), rotationSpin_, -180, 180, 1);

    connect(trimPresetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == 1) {
            clipInSpin_->setValue(0.0);
            clipOutSpin_->setValue(5.0);
        } else if (index == 2) {
            clipInSpin_->setValue(0.0);
            clipOutSpin_->setValue(10.0);
        } else if (index == 3) {
            clipInSpin_->setValue(0.0);
            clipOutSpin_->setValue(0.0);
        }
    });
    connect(rotationPresetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const QList<int> values { rotationSpin_->value(), 0, 90, 180, -90 };
        rotationSpin_->setValue(values.value(index, 0));
    });
    connect(clipInSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(clipOutSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(positionXSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(positionYSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(scaleSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(rotationSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    return tab;
}

QWidget *MainWindow::build360InspectorTab()
{
    auto *tab = new QWidget;
    auto *form = new QFormLayout(tab);
    form->setContentsMargins(10, 12, 10, 12);
    form->setSpacing(10);

    projectionCombo_ = new QComboBox;
    projectionCombo_->addItems({QStringLiteral("Equirectangular"), QStringLiteral("Flat 360 View")});
    previewModeCombo_ = new QComboBox;
    previewModeCombo_->addItems({QStringLiteral("Equirectangular Preview"), QStringLiteral("Interactive 360 Mouse View")});

    yawSpin_ = new QSpinBox;
    yawSpin_->setSuffix(QStringLiteral(" deg"));
    pitchSpin_ = new QSpinBox;
    pitchSpin_->setSuffix(QStringLiteral(" deg"));
    rollSpin_ = new QSpinBox;
    rollSpin_->setSuffix(QStringLiteral(" deg"));
    sphericalMetadataCheck_ = new QCheckBox(QStringLiteral("Write 360 metadata"));
    sphericalMetadataCheck_->setChecked(true);

    form->addRow(QStringLiteral("Projection"), projectionCombo_);
    form->addRow(QStringLiteral("Preview"), previewModeCombo_);
    addSliderSpinRow(form, QStringLiteral("Yaw"), yawSpin_, -180, 180, 1);
    addSliderSpinRow(form, QStringLiteral("Pitch"), pitchSpin_, -90, 90, 1);
    addSliderSpinRow(form, QStringLiteral("Roll"), rollSpin_, -180, 180, 1);
    form->addRow(QStringLiteral("Metadata"), sphericalMetadataCheck_);

    connect(projectionCombo_, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::inspectorValueChanged);
    connect(previewModeCombo_, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::inspectorValueChanged);
    connect(projectionCombo_, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        const int index = previewModeCombo_->currentIndex() == 1 ? 2 : (projectionCombo_->currentIndex() == 1 ? 1 : 0);
        previewViewCombo_->setCurrentIndex(index);
    });
    connect(previewModeCombo_, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        const int index = previewModeCombo_->currentIndex() == 1 ? 2 : (projectionCombo_->currentIndex() == 1 ? 1 : 0);
        previewViewCombo_->setCurrentIndex(index);
    });
    connect(yawSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(pitchSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(rollSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(sphericalMetadataCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    return tab;
}

QWidget *MainWindow::buildColorInspectorTab()
{
    auto *tab = new QWidget;
    auto *form = new QFormLayout(tab);
    form->setContentsMargins(10, 12, 10, 12);
    form->setSpacing(10);

    colorPresetCombo_ = new QComboBox;
    colorPresetCombo_->addItems({QStringLiteral("Custom"), QStringLiteral("Neutral"), QStringLiteral("Vivid"), QStringLiteral("Warm"), QStringLiteral("Cool"), QStringLiteral("Flat")});
    form->addRow(QStringLiteral("Preset"), colorPresetCombo_);

    brightnessSpin_ = new QSpinBox;
    contrastSpin_ = new QSpinBox;
    contrastSpin_->setSuffix(QStringLiteral("%"));
    saturationSpin_ = new QSpinBox;
    saturationSpin_->setSuffix(QStringLiteral("%"));
    temperatureSpin_ = new QSpinBox;
    temperatureSpin_->setSuffix(QStringLiteral(" K"));

    addSliderSpinRow(form, QStringLiteral("Brightness"), brightnessSpin_, -100, 100, 1);
    addSliderSpinRow(form, QStringLiteral("Contrast"), contrastSpin_, 0, 300, 5);
    addSliderSpinRow(form, QStringLiteral("Saturation"), saturationSpin_, 0, 300, 5);
    addSliderSpinRow(form, QStringLiteral("Temperature"), temperatureSpin_, 1000, 12000, 100);

    connect(colorPresetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == 1) {
            brightnessSpin_->setValue(0); contrastSpin_->setValue(100); saturationSpin_->setValue(100); temperatureSpin_->setValue(6500);
        } else if (index == 2) {
            brightnessSpin_->setValue(4); contrastSpin_->setValue(112); saturationSpin_->setValue(118); temperatureSpin_->setValue(6500);
        } else if (index == 3) {
            brightnessSpin_->setValue(2); contrastSpin_->setValue(105); saturationSpin_->setValue(108); temperatureSpin_->setValue(7600);
        } else if (index == 4) {
            brightnessSpin_->setValue(0); contrastSpin_->setValue(104); saturationSpin_->setValue(102); temperatureSpin_->setValue(5200);
        } else if (index == 5) {
            brightnessSpin_->setValue(-2); contrastSpin_->setValue(92); saturationSpin_->setValue(88); temperatureSpin_->setValue(6500);
        }
    });
    connect(brightnessSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(contrastSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(saturationSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(temperatureSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    return tab;
}

QWidget *MainWindow::buildAudioInspectorTab()
{
    auto *tab = new QWidget;
    auto *form = new QFormLayout(tab);
    form->setContentsMargins(10, 12, 10, 12);
    form->setSpacing(10);

    audioPresetCombo_ = new QComboBox;
    audioPresetCombo_->addItems({QStringLiteral("Custom"), QStringLiteral("Use source audio"), QStringLiteral("Mute"), QStringLiteral("Voice boost")});
    form->addRow(QStringLiteral("Preset"), audioPresetCombo_);

    audioEnabledCheck_ = new QCheckBox(QStringLiteral("Enable audio"));
    audioEnabledCheck_->setChecked(true);
    volumeSpin_ = new QSpinBox;
    volumeSpin_->setSuffix(QStringLiteral("%"));
    fadeInSpin_ = new QDoubleSpinBox;
    fadeInSpin_->setRange(0.0, 60.0);
    fadeInSpin_->setDecimals(2);
    fadeInSpin_->setSuffix(QStringLiteral(" s"));
    fadeOutSpin_ = new QDoubleSpinBox;
    fadeOutSpin_->setRange(0.0, 60.0);
    fadeOutSpin_->setDecimals(2);
    fadeOutSpin_->setSuffix(QStringLiteral(" s"));

    form->addRow(QStringLiteral("Audio"), audioEnabledCheck_);
    addSliderSpinRow(form, QStringLiteral("Volume"), volumeSpin_, 0, 300, 5);
    addDoubleSliderSpinRow(form, QStringLiteral("Fade In"), fadeInSpin_, 0.0, 20.0, 100);
    addDoubleSliderSpinRow(form, QStringLiteral("Fade Out"), fadeOutSpin_, 0.0, 20.0, 100);

    connect(audioPresetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == 1) {
            audioEnabledCheck_->setChecked(true); volumeSpin_->setValue(100); fadeInSpin_->setValue(0.0); fadeOutSpin_->setValue(0.0);
        } else if (index == 2) {
            audioEnabledCheck_->setChecked(false);
        } else if (index == 3) {
            audioEnabledCheck_->setChecked(true); volumeSpin_->setValue(125);
        }
    });
    connect(audioEnabledCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(volumeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(fadeInSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    connect(fadeOutSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    return tab;
}

QWidget *MainWindow::buildEffectsInspectorTab()
{
    auto *tab = new QWidget;
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(10);

    effectsPresetCombo_ = new QComboBox;
    effectsPresetCombo_->addItems({QStringLiteral("Custom"), QStringLiteral("Clean 360"), QStringLiteral("Action stabilization"), QStringLiteral("Low light cleanup"), QStringLiteral("Off")});
    stabilizeCheck_ = new QCheckBox(QStringLiteral("Stabilize"));
    denoiseCheck_ = new QCheckBox(QStringLiteral("Denoise"));
    defringeCheck_ = new QCheckBox(QStringLiteral("Defringe / purple fringe"));
    deflickerCheck_ = new QCheckBox(QStringLiteral("Deflicker"));
    stitchFusionCheck_ = new QCheckBox(QStringLiteral("Stitch fusion / lens matching"));
    coolingShellCheck_ = new QCheckBox(QStringLiteral("Cooling shell detection"));
    coolingShellCheck_->setToolTip(QStringLiteral("Disokong untuk X4 Air, X5 dan X6; status sebenar dilaporkan selepas export."));
    colorPlusCheck_ = new QCheckBox(QStringLiteral("Color Plus"));
    colorPlusStrengthSpin_ = new QSpinBox;
    colorPlusStrengthSpin_->setRange(0, 100);
    colorPlusStrengthSpin_->setSuffix(QStringLiteral("%"));
    colorPlusStrengthSpin_->setValue(100);

    layout->addWidget(effectsPresetCombo_);
    layout->addWidget(stabilizeCheck_);
    layout->addWidget(denoiseCheck_);
    layout->addWidget(defringeCheck_);
    layout->addWidget(deflickerCheck_);
    layout->addWidget(stitchFusionCheck_);
    layout->addWidget(coolingShellCheck_);
    layout->addWidget(colorPlusCheck_);
    auto *strengthRow = new QHBoxLayout;
    strengthRow->addWidget(new QLabel(QStringLiteral("ColorPlus strength")));
    strengthRow->addWidget(colorPlusStrengthSpin_);
    layout->addLayout(strengthRow);
    layout->addStretch();

    connect(effectsPresetCombo_, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        if (index == 1) {
            stabilizeCheck_->setChecked(true); denoiseCheck_->setChecked(true); defringeCheck_->setChecked(true); deflickerCheck_->setChecked(true); stitchFusionCheck_->setChecked(true); coolingShellCheck_->setChecked(false); colorPlusCheck_->setChecked(true);
        } else if (index == 2) {
            stabilizeCheck_->setChecked(true); denoiseCheck_->setChecked(false); defringeCheck_->setChecked(false); deflickerCheck_->setChecked(false); stitchFusionCheck_->setChecked(false); coolingShellCheck_->setChecked(false); colorPlusCheck_->setChecked(false);
        } else if (index == 3) {
            stabilizeCheck_->setChecked(false); denoiseCheck_->setChecked(true); defringeCheck_->setChecked(false); deflickerCheck_->setChecked(true); stitchFusionCheck_->setChecked(false); coolingShellCheck_->setChecked(false); colorPlusCheck_->setChecked(false);
        } else if (index == 4) {
            stabilizeCheck_->setChecked(false); denoiseCheck_->setChecked(false); defringeCheck_->setChecked(false); deflickerCheck_->setChecked(false); stitchFusionCheck_->setChecked(false); coolingShellCheck_->setChecked(false); colorPlusCheck_->setChecked(false);
        }
    });
    connect(stabilizeCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(denoiseCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(defringeCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(deflickerCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(stitchFusionCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(coolingShellCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(colorPlusCheck_, &QCheckBox::toggled, this, &MainWindow::inspectorValueChanged);
    connect(colorPlusCheck_, &QCheckBox::toggled, colorPlusStrengthSpin_, &QSpinBox::setEnabled);
    connect(colorPlusStrengthSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    return tab;
}

QWidget *MainWindow::buildSdkInspectorTab()
{
    auto *content = new QWidget;
    auto *form = new QFormLayout(content);
    form->setContentsMargins(8, 10, 8, 10);
    form->setSpacing(8);

    sdkExposureSpin_ = new QSpinBox;
    sdkHighlightsSpin_ = new QSpinBox;
    sdkShadowsSpin_ = new QSpinBox;
    sdkContrastSpin_ = new QSpinBox;
    sdkBrightnessSpin_ = new QSpinBox;
    sdkBlackpointSpin_ = new QSpinBox;
    sdkSaturationSpin_ = new QSpinBox;
    sdkVibranceSpin_ = new QSpinBox;
    sdkWarmthSpin_ = new QSpinBox;
    sdkTintSpin_ = new QSpinBox;
    sdkDefinitionSpin_ = new QSpinBox;

    const QList<QPair<QString, QSpinBox *>> controls {
        {QStringLiteral("Exposure"), sdkExposureSpin_}, {QStringLiteral("Highlights"), sdkHighlightsSpin_},
        {QStringLiteral("Shadows"), sdkShadowsSpin_}, {QStringLiteral("Contrast"), sdkContrastSpin_},
        {QStringLiteral("Brightness"), sdkBrightnessSpin_}, {QStringLiteral("Black point"), sdkBlackpointSpin_},
        {QStringLiteral("Saturation"), sdkSaturationSpin_}, {QStringLiteral("Vibrance"), sdkVibranceSpin_},
        {QStringLiteral("Warmth"), sdkWarmthSpin_}, {QStringLiteral("Tint"), sdkTintSpin_}
    };
    for (const auto &control : controls) {
        addSliderSpinRow(form, control.first, control.second, -100, 100);
        connect(control.second, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);
    }
    addSliderSpinRow(form, QStringLiteral("Definition"), sdkDefinitionSpin_, 0, 100);
    connect(sdkDefinitionSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::inspectorValueChanged);

    auto *note = new QLabel(QStringLiteral("Tetapan ini diproses terus oleh MediaSDK untuk fail Insta360 mentah. Julat mengikut manual SDK 3.1.5."));
    note->setWordWrap(true);
    note->setObjectName(QStringLiteral("MutedText"));
    form->addRow(note);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    fileMenu->addAction(style()->standardIcon(QStyle::SP_FileDialogNewFolder), QStringLiteral("New Project"), this, &MainWindow::newProject);
    fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), QStringLiteral("Open Project"), this, &MainWindow::openProjectDialog);
    fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QStringLiteral("Save Project"), this, &MainWindow::saveCurrentProject);
    fileMenu->addSeparator();
    fileMenu->addAction(style()->standardIcon(QStyle::SP_DialogCloseButton), QStringLiteral("Quit"), qApp, &QApplication::quit);

    auto *editMenu = menuBar()->addMenu(QStringLiteral("Edit"));
    editMenu->addAction(style()->standardIcon(QStyle::SP_ArrowRight), QStringLiteral("Add Selected To Timeline"), this, &MainWindow::addSelectedMediaToTimeline);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("View"));
    viewMenu->addAction(style()->standardIcon(QStyle::SP_BrowserReload), QStringLiteral("Refresh Media Library"), this, &MainWindow::refreshMediaList);
    viewMenu->addAction(style()->standardIcon(QStyle::SP_MessageBoxInformation), QStringLiteral("Media Info (MediaSDK)"), this, &MainWindow::showSelectedMediaInfo);

    auto *playbackMenu = menuBar()->addMenu(QStringLiteral("Playback"));
    playbackMenu->addAction(style()->standardIcon(QStyle::SP_MediaPlay), QStringLiteral("Play/Pause Preview"), this, &MainWindow::togglePreviewPlayback);

    auto *exportMenu = menuBar()->addMenu(QStringLiteral("Export"));
    exportMenu->addAction(style()->standardIcon(QStyle::SP_DialogApplyButton), QStringLiteral("Export MP4"), this, &MainWindow::exportProject);
    exportMenu->addAction(style()->standardIcon(QStyle::SP_DirIcon), QStringLiteral("Export Selected Frames…"), this, &MainWindow::exportSelectedFrames);

    auto *sdkMenu = menuBar()->addMenu(QStringLiteral("MediaSDK"));
    sdkMenu->addAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView), QStringLiteral("Project & SDK Settings…"), this, &MainWindow::editProjectSettings);
    sdkMenu->addAction(style()->standardIcon(QStyle::SP_MessageBoxInformation), QStringLiteral("SDK Status…"), this, &MainWindow::showSdkStatus);
#ifdef INSTA360_HAS_LIVE_CAMERA
    sdkMenu->addAction(style()->standardIcon(QStyle::SP_ComputerIcon), QStringLiteral("Live Camera / Real-Time Stitching…"), this, &MainWindow::openLiveCamera);
#endif
}

void MainWindow::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #15181d; color: #d6d9df; font-size: 14px; }
        QMenuBar { background: #0f1115; color: #d6d9df; border-bottom: 1px solid #2a2f38; padding: 3px 8px; }
        QMenuBar::item { background: transparent; padding: 6px 10px; }
        QMenuBar::item:selected { background: #222833; border-radius: 4px; }
        QMenu { background: #171b22; color: #d6d9df; border: 1px solid #333a45; }
        QMenu::item { padding: 7px 24px; }
        QMenu::item:selected { background: #2b3441; }
        QLabel#StartTitle { font-size: 40px; font-weight: 700; }
        QLabel#StartSubtitle { font-size: 18px; color: #9aa4b2; max-width: 760px; }
        QLabel#MutedText { color: #8791a1; }
        QLabel#PanelTitle, QLabel#ProjectSummary { font-weight: 700; color: #eef1f5; }
        QLabel#ProjectSummary { color: #aeb7c4; }
        QLabel#Preview { background: #07090c; color: #9aa4b2; border: 1px solid #2b313b; border-radius: 4px; }
        QWidget#SidePanel, QWidget#InspectorPanel { background: #11151b; border-left: 1px solid #262c35; border-right: 1px solid #262c35; }
        QWidget#PreviewPanel { background: #15181d; }
        QWidget#TimelinePanel { background: #101318; border-top: 1px solid #262c35; }
        QSplitter::handle { background: #262c35; }
        QListWidget { background: #171c23; color: #d6d9df; border: 1px solid #303743; border-radius: 5px; padding: 6px; }
        QListWidget::item { padding: 8px; border-radius: 4px; }
        QListWidget::item:selected { background: #2f5f74; color: #ffffff; }
        QTextEdit { background: #11151b; color: #aeb7c4; border: 1px solid #303743; border-radius: 5px; }
        QTabWidget::pane { border: 1px solid #303743; background: #171c23; border-radius: 5px; }
        QTabBar::tab { background: #11151b; color: #9aa4b2; padding: 8px 10px; border: 1px solid #303743; border-bottom: none; }
        QTabBar::tab:selected { background: #232a34; color: #ffffff; }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { background: #0f1319; color: #eef1f5; border: 1px solid #394252; border-radius: 4px; padding: 6px; selection-background-color: #0f766e; }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: #18a094; }
        QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { width: 16px; background: #202631; border: none; }
        QCheckBox { color: #c8ced8; spacing: 8px; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #465163; border-radius: 3px; background: #0f1319; }
        QCheckBox::indicator:checked { background: #0f766e; border-color: #18a094; }
        QLabel#InspectorSection { color: #eef1f5; font-weight: 700; padding-bottom: 8px; }
        QLabel#InspectorRow { color: #aeb7c4; background: #11151b; border: 1px solid #2b313b; border-radius: 4px; padding: 8px; }
        QPushButton { background: #202631; color: #d6d9df; border: 1px solid #394252; border-radius: 5px; padding: 8px 12px; }
        QPushButton:hover { background: #2a3240; }
        QPushButton#PrimaryButton { background: #0f766e; color: #ffffff; border-color: #159084; font-weight: 700; }
        QPushButton#PrimaryButton:hover { background: #12877d; }
        QSlider::groove:horizontal { height: 5px; background: #303743; border-radius: 2px; }
        QSlider::handle:horizontal { background: #cbd5e1; width: 14px; margin: -5px 0; border-radius: 7px; }
        QProgressBar { border: 1px solid #303743; border-radius: 4px; text-align: center; background: #171c23; color: #d6d9df; }
        QProgressBar::chunk { background: #0f766e; }
    )"));
}

void MainWindow::newProject()
{
    ProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    Project project = dialog.createProject();
    QDir().mkpath(project.projectDir);
    QDir().mkpath(QDir(project.projectDir).filePath(QStringLiteral("media")));

    QString error;
    if (!saveProject(project, &error)) {
        QMessageBox::critical(this, QStringLiteral("Save gagal"), error);
        return;
    }
    setProject(project);
    appendLog(QStringLiteral("Projek dicipta: %1").arg(project.projectFile));
}

void MainWindow::openProjectDialog()
{
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Open Project"), QString(), QStringLiteral("Insta360 Project (*.i360proj)"));
    if (file.isEmpty()) {
        return;
    }
    Project project;
    QString error;
    if (!loadProject(file, &project, &error)) {
        QMessageBox::critical(this, QStringLiteral("Open gagal"), error);
        return;
    }
    setProject(project);
    appendLog(QStringLiteral("Projek dibuka: %1").arg(file));
}

void MainWindow::saveCurrentProject()
{
    if (!hasProject_) {
        return;
    }
    QString error;
    if (!saveProject(project_, &error)) {
        QMessageBox::critical(this, QStringLiteral("Save gagal"), error);
        return;
    }
    appendLog(QStringLiteral("Projek disimpan."));
}

void MainWindow::editProjectSettings()
{
    if (!hasProject_) return;
    ProjectDialog dialog(this, &project_);
    if (dialog.exec() != QDialog::Accepted) return;
    project_ = dialog.createProject();
    updateProjectSummary();
    loadInspectorFromClip();
    saveCurrentProject();
    appendLog(QStringLiteral("Tetapan projek dan MediaSDK dikemas kini."));
}

void MainWindow::showSdkStatus()
{
    const QString helper = sdkExporterPath();
    if (helper.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("MediaSDK"), QStringLiteral("insta360_sdk_exporter tidak ditemui."));
        return;
    }
    QProcess process;
    process.start(helper, {QStringLiteral("--version")});
    process.waitForFinished(10000);
    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString models = QCoreApplication::applicationDirPath() + QStringLiteral("/models");
    QMessageBox::information(this, QStringLiteral("MediaSDK Status"),
        QStringLiteral("%1\nHelper: %2\nModels: %3 (%4)")
            .arg(output.isEmpty() ? QStringLiteral("Versi tidak tersedia") : output, helper, models,
                 QDir(models).exists() ? QStringLiteral("OK") : QStringLiteral("tidak ditemui")));
}

void MainWindow::showSelectedMediaInfo()
{
    const QString path = currentPreviewPath();
    const QString helper = sdkExporterPath();
    if (path.isEmpty() || !isInsta360File(path)) {
        QMessageBox::information(this, QStringLiteral("Media Info"), QStringLiteral("Pilih fail Insta360 mentah dalam media library atau timeline."));
        return;
    }
    if (helper.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Media Info"), QStringLiteral("MediaSDK helper tidak ditemui."));
        return;
    }
    QStringList args {QStringLiteral("--probe")};
    for (const QString &input : pairedInsta360Inputs(path)) args << QStringLiteral("--input") << input;
    QProcess process;
    process.start(helper, args);
    process.waitForFinished(15000);
    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    QMessageBox::information(this, QStringLiteral("MediaSDK Media Info"),
        QStringLiteral("%1\n\n%2").arg(QFileInfo(path).fileName(), output.isEmpty() ? error : output));
}

void MainWindow::exportSelectedFrames()
{
    const QString path = currentPreviewPath();
    const QString helper = sdkExporterPath();
    if (path.isEmpty() || !isInsta360File(path) || QFileInfo(path).suffix().compare(QStringLiteral("insp"), Qt::CaseInsensitive) == 0) {
        QMessageBox::information(this, QStringLiteral("Frame Sequence"), QStringLiteral("Pilih klip video `.insv` atau `.lrv`."));
        return;
    }
    if (helper.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Frame Sequence"), QStringLiteral("MediaSDK helper tidak ditemui."));
        return;
    }
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("Folder output frame"), project_.projectDir);
    if (directory.isEmpty()) return;
    bool accepted = false;
    const QString frames = QInputDialog::getText(this, QStringLiteral("Frame indices"),
        QStringLiteral("Indeks dipisahkan koma/tanda sengkang. Kosong = semua frame:"), QLineEdit::Normal, QString(), &accepted);
    if (!accepted) return;
    const QString type = QInputDialog::getItem(this, QStringLiteral("Format imej"), QStringLiteral("Format:"),
        {QStringLiteral("jpg"), QStringLiteral("png")}, 0, false, &accepted);
    if (!accepted) return;
    const int sdkWidth = project_.settings.format == ProjectFormat::Panorama360
        ? project_.settings.width : qMax(3840, project_.settings.width * 2);
    QStringList args {QStringLiteral("--image_sequence_dir"), directory, QStringLiteral("--image_type"), type,
        QStringLiteral("--width"), QString::number(sdkWidth), QStringLiteral("--height"), QString::number(sdkWidth / 2),
        QStringLiteral("--stitch_type"), stitchTypeToSdkArg(project_.settings.stitchType)};
    if (!frames.trimmed().isEmpty()) args << QStringLiteral("--frames") << frames;
    for (const QString &input : pairedInsta360Inputs(path)) args << QStringLiteral("--input") << input;
    if (project_.settings.flowState) args << QStringLiteral("--flowstate");
    if (!project_.settings.cuda) args << QStringLiteral("--disable_cuda");
    if (project_.settings.imageProcessingCpu) args << QStringLiteral("--image_processing_cpu");
    args << QStringLiteral("--camera_accessory") << QString::number(project_.settings.cameraAccessoryType);
    if (!project_.settings.aiModelPath.isEmpty()) args << QStringLiteral("--model_root") << project_.settings.aiModelPath;
    if (const TimelineClip *clip = selectedTimelineClip()) {
        if (clip->denoise) args << QStringLiteral("--denoise");
        if (clip->defringe) args << QStringLiteral("--defringe");
        if (clip->deflicker) args << QStringLiteral("--deflicker");
        if (clip->stitchFusion) args << QStringLiteral("--stitch_fusion");
        if (clip->coolingShellDetection) args << QStringLiteral("--cooling_shell");
        if (clip->colorPlus) args << QStringLiteral("--color_plus");
        args << QStringLiteral("--color_plus_strength") << QString::number(clip->colorPlusStrength / 100.0, 'f', 2)
             << QStringLiteral("--exposure") << QString::number(clip->sdkExposure)
             << QStringLiteral("--highlights") << QString::number(clip->sdkHighlights)
             << QStringLiteral("--shadows") << QString::number(clip->sdkShadows)
             << QStringLiteral("--contrast") << QString::number(clip->sdkContrast)
             << QStringLiteral("--brightness") << QString::number(clip->sdkBrightness)
             << QStringLiteral("--blackpoint") << QString::number(clip->sdkBlackpoint)
             << QStringLiteral("--saturation") << QString::number(clip->sdkSaturation)
             << QStringLiteral("--vibrance") << QString::number(clip->sdkVibrance)
             << QStringLiteral("--warmth") << QString::number(clip->sdkWarmth)
             << QStringLiteral("--tint") << QString::number(clip->sdkTint)
             << QStringLiteral("--definition") << QString::number(clip->sdkDefinition);
    }
    QProcess process;
    process.start(helper, args);
    if (!process.waitForFinished(-1) || process.exitCode() != 0) {
        QMessageBox::critical(this, QStringLiteral("Frame Sequence"), QString::fromUtf8(process.readAllStandardError()));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Frame Sequence"), QStringLiteral("Frame berjaya diexport ke %1").arg(directory));
}

void MainWindow::openLiveCamera()
{
#ifdef INSTA360_HAS_LIVE_CAMERA
    LiveCameraDialog dialog(this);
    dialog.exec();
#else
    QMessageBox::information(this, QStringLiteral("Live Camera"), QStringLiteral("Build ini tidak mempunyai CameraSDK."));
#endif
}

void MainWindow::importMediaDialog()
{
    if (!hasProject_) {
        newProject();
        if (!hasProject_) {
            return;
        }
    }
    const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import Media"), QString(), supportedMediaFilters().join(QStringLiteral(";;")));
    if (!files.isEmpty()) {
        importPaths(files, true);
    }
}

void MainWindow::importFromCamera()
{
    if (!hasProject_) {
        newProject();
        if (!hasProject_) {
            return;
        }
    }

    const QString tool = cameraToolPath();
    if (tool.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("CameraSDK"), QStringLiteral("insta360_camera_tool tidak ditemui. Jalankan setup SDK dan build semula."));
        return;
    }

    QProgressDialog progress(QStringLiteral("Membaca senarai media terus dari kamera…"), QStringLiteral("Cancel"), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    QProcess process;
    process.start(tool, {QStringLiteral("--list")});
    if (!process.waitForStarted(10000)) {
        QMessageBox::critical(this, QStringLiteral("CameraSDK"), process.errorString());
        return;
    }
    while (!process.waitForFinished(100)) {
        qApp->processEvents();
        if (progress.wasCanceled()) {
            process.terminate();
            process.waitForFinished(3000);
            return;
        }
    }
    progress.close();
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    if (process.exitCode() != 0) {
        QMessageBox::warning(this, QStringLiteral("CameraSDK"), QString::fromUtf8(process.readAllStandardError()).trimmed());
        return;
    }

    QString device;
    int added = 0;
    for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.startsWith(QStringLiteral("device="))) {
            device = line.mid(7).replace(QLatin1Char('|'), QStringLiteral(" · "));
            continue;
        }
        if (!line.startsWith(QStringLiteral("file="))) continue;
        const QString remotePath = line.mid(5).trimmed();
        const QString fileName = QUrl(remotePath).fileName().isEmpty()
            ? QFileInfo(remotePath).fileName() : QUrl(remotePath).fileName();
        if (!isSupportedMediaFile(fileName)) continue;
        const bool exists = std::any_of(project_.media.cbegin(), project_.media.cend(), [&remotePath](const MediaItem &item) {
            return item.originalPath == remotePath;
        });
        if (exists) continue;
        MediaItem item;
        item.id = makeMediaId();
        item.displayName = fileName;
        item.originalPath = remotePath;
        item.kind = QStringLiteral("camera-remote");
        item.insta360 = isInsta360File(fileName);
        project_.media.append(item);
        ++added;
    }
    refreshMediaList();
    saveCurrentProject();
    appendLog(QStringLiteral("Camera: %1 — %2 item baru dipaparkan tanpa download.")
        .arg(device.isEmpty() ? QStringLiteral("connected") : device).arg(added));
    if (added == 0) {
        QMessageBox::information(this, QStringLiteral("Camera Library"),
            QStringLiteral("Tiada media baru. Item yang sudah pernah diimport kekal dalam senarai kiri."));
    }
}

void MainWindow::addSelectedMediaToTimeline()
{
    for (QListWidgetItem *item : mediaList_->selectedItems()) {
        addMediaToTimeline(item->data(Qt::UserRole + 1).toString());
    }
}

void MainWindow::addMediaToTimeline(const QString &mediaId)
{
    MediaItem *media = findMedia(mediaId);
    if (!media) {
        return;
    }

    if (media->kind == QStringLiteral("camera-remote") && !downloadRemoteMedia(media)) {
        return;
    }

    TimelineClip clip;
    clip.mediaId = media->id;
    clip.displayName = media->displayName;
    clip.path = media->path;
    project_.timeline.append(clip);
    refreshTimeline();
    timeline_->setCurrentRow(project_.timeline.size() - 1);
    saveCurrentProject();
}

void MainWindow::importDroppedFiles(const QStringList &paths)
{
    const int previousCount = project_.media.size();
    if (importPaths(paths, true)) {
        for (int i = previousCount; i < project_.media.size(); ++i) {
            addMediaToTimeline(project_.media[i].id);
        }
    }
}

void MainWindow::previewMedia(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    stopPreviewPlayback();
    previewSeconds_ = 0.0;
    previewSlider_->blockSignals(true);
    previewSlider_->setValue(0);
    previewSlider_->blockSignals(false);
    previewTitle_->setText(item->text());
    if (item->data(Qt::UserRole + 2).toString() == QStringLiteral("camera-remote")) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(QStringLiteral("Media berada pada kamera\nDouble-click atau Add To Timeline untuk download apabila diperlukan."));
        return;
    }
    renderPreviewFrame(item->data(Qt::UserRole).toString(), 0.0);
}

void MainWindow::exportProject()
{
    if (!hasProject_) {
        return;
    }
    if (project_.timeline.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Timeline kosong"), QStringLiteral("Tambah media ke timeline sebelum export."));
        return;
    }

    const QString output = QFileDialog::getSaveFileName(this, QStringLiteral("Export MP4"), project_.projectDir + QStringLiteral("/") + project_.name + QStringLiteral(".mp4"), QStringLiteral("MP4 Video (*.mp4)"));
    if (output.isEmpty()) {
        return;
    }

    saveCurrentProject();
    exportProgress_->setValue(0);
    setEditorEnabled(false);

    auto *thread = new QThread(this);
    auto *worker = new ExportWorker(project_, output);
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &ExportWorker::run);
    connect(cancelExportButton_, &QPushButton::clicked, worker, &ExportWorker::cancel, Qt::DirectConnection);
    cancelExportButton_->setEnabled(true);
    connect(worker, &ExportWorker::progress, exportProgress_, &QProgressBar::setValue);
    connect(worker, &ExportWorker::message, this, &MainWindow::appendLog);
    connect(worker, &ExportWorker::finished, this, [this, thread, worker](bool ok, const QString &message) {
        Q_UNUSED(thread);
        Q_UNUSED(worker);
        setEditorEnabled(true);
        cancelExportButton_->setEnabled(false);
        appendLog(message);
        QMessageBox::information(this, ok ? QStringLiteral("Export selesai") : QStringLiteral("Export gagal"), message);
    });
    connect(worker, &ExportWorker::finished, thread, &QThread::quit);
    connect(worker, &ExportWorker::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void MainWindow::togglePreviewPlayback()
{
    if (playbackProcess_) {
        stopPreviewPlayback();
    } else {
        startPreviewPlayback();
    }
}

void MainWindow::renderNextPreviewFrame()
{
    previewSeconds_ += 0.25;
    if (previewSeconds_ > previewSlider_->maximum()) {
        previewSeconds_ = 0.0;
    }
    previewSlider_->blockSignals(true);
    previewSlider_->setValue(static_cast<int>(previewSeconds_));
    previewSlider_->blockSignals(false);
    renderPreviewFrame(currentPreviewPath(), previewSeconds_);
}

void MainWindow::timelineSelectionChanged()
{
    stopPreviewPlayback();
    selectedTimelineIndex_ = timeline_->currentRow();
    loadInspectorFromClip();
    renderSelectedTimelinePreview();
}

void MainWindow::previewPanDragged(int deltaX, int deltaY)
{
    if (updatingInspector_ || selectedTimelineIndex_ < 0 || selectedTimelineIndex_ >= project_.timeline.size()) {
        return;
    }
    if (project_.settings.format != ProjectFormat::Panorama360 || previewModeCombo_->currentIndex() != 1) {
        return;
    }

    dragging360Preview_ = true;
    yawSpin_->setValue(wrappedDegrees(yawSpin_->value() - deltaX / 2));
    pitchSpin_->setValue(qBound(-90, pitchSpin_->value() + deltaY / 3, 90));
    dragging360Preview_ = false;

    TimelineClip &clip = project_.timeline[selectedTimelineIndex_];
    clip.yawDegrees = yawSpin_->value();
    clip.pitchDegrees = pitchSpin_->value();
    clip.rollDegrees = rollSpin_->value();
    previewLabel_->updatePanoramaView(clip.yawDegrees, clip.pitchDegrees, clip.rollDegrees);
    QString error;
    saveProject(project_, &error);
}

void MainWindow::inspectorValueChanged()
{
    if (updatingInspector_) {
        return;
    }
    saveInspectorToClip();
    if (dragging360Preview_) {
        return;
    }
    if (playbackProcess_) {
        stopPreviewPlayback();
    }
    if (project_.settings.format == ProjectFormat::Panorama360 && previewModeCombo_->currentIndex() == 1) {
        previewLabel_->updatePanoramaView(yawSpin_->value(), pitchSpin_->value(), rollSpin_->value());
        return;
    }
    inspectorPreviewTimer_->start();
}

void MainWindow::setProject(const Project &project)
{
    project_ = project;
    hasProject_ = true;
    selectedTimelineIndex_ = -1;
    refreshMediaList();
    refreshTimeline();
    updateProjectSummary();
    stack_->setCurrentWidget(editorPage_);
}

void MainWindow::refreshMediaList()
{
    mediaList_->clear();
    for (const MediaItem &media : project_.media) {
        const bool remote = media.kind == QStringLiteral("camera-remote");
        auto *item = new QListWidgetItem(remote ? media.displayName + QStringLiteral("\n☁ Pada kamera") : media.displayName);
        item->setIcon(style()->standardIcon(remote ? QStyle::SP_DriveNetIcon : QStyle::SP_FileIcon));
        item->setToolTip(remote ? media.originalPath + QStringLiteral("\nBelum dimuat turun") : media.path);
        item->setData(Qt::UserRole, media.path);
        item->setData(Qt::UserRole + 1, media.id);
        item->setData(Qt::UserRole + 2, media.kind);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        mediaList_->addItem(item);
    }
}

void MainWindow::refreshTimeline()
{
    timeline_->clear();
    int number = 1;
    for (const TimelineClip &clip : project_.timeline) {
        auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(number++).arg(clip.displayName));
        item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        item->setToolTip(clip.path);
        item->setData(Qt::UserRole, clip.mediaId);
        item->setSizeHint(QSize(180, 96));
        timeline_->addItem(item);
    }
    if (!project_.timeline.isEmpty()) {
        timeline_->setCurrentRow(qBound(0, selectedTimelineIndex_, project_.timeline.size() - 1));
    } else {
        selectedTimelineIndex_ = -1;
        loadInspectorFromClip();
    }
}

void MainWindow::loadInspectorFromClip()
{
    updatingInspector_ = true;
    const bool hasClip = selectedTimelineIndex_ >= 0 && selectedTimelineIndex_ < project_.timeline.size();

    const TimelineClip clip = hasClip ? project_.timeline[selectedTimelineIndex_] : TimelineClip();

    trimPresetCombo_->setEnabled(hasClip);
    rotationPresetCombo_->setEnabled(hasClip);
    clipInSpin_->setEnabled(hasClip);
    clipOutSpin_->setEnabled(hasClip);
    positionXSpin_->setEnabled(hasClip);
    positionYSpin_->setEnabled(hasClip);
    scaleSpin_->setEnabled(hasClip);
    rotationSpin_->setEnabled(hasClip);
    projectionCombo_->setEnabled(hasClip);
    previewModeCombo_->setEnabled(hasClip);
    yawSpin_->setEnabled(hasClip);
    pitchSpin_->setEnabled(hasClip);
    rollSpin_->setEnabled(hasClip);
    sphericalMetadataCheck_->setEnabled(hasClip);
    colorPresetCombo_->setEnabled(hasClip);
    brightnessSpin_->setEnabled(hasClip);
    contrastSpin_->setEnabled(hasClip);
    saturationSpin_->setEnabled(hasClip);
    temperatureSpin_->setEnabled(hasClip);
    audioPresetCombo_->setEnabled(hasClip);
    audioEnabledCheck_->setEnabled(hasClip);
    volumeSpin_->setEnabled(hasClip);
    fadeInSpin_->setEnabled(hasClip);
    fadeOutSpin_->setEnabled(hasClip);
    effectsPresetCombo_->setEnabled(hasClip);
    stabilizeCheck_->setEnabled(hasClip);
    denoiseCheck_->setEnabled(hasClip);
    defringeCheck_->setEnabled(hasClip);
    deflickerCheck_->setEnabled(hasClip);
    stitchFusionCheck_->setEnabled(hasClip);
    coolingShellCheck_->setEnabled(hasClip);
    colorPlusCheck_->setEnabled(hasClip);
    colorPlusStrengthSpin_->setEnabled(hasClip);
    const QList<QSpinBox *> sdkColorControls {sdkExposureSpin_, sdkHighlightsSpin_, sdkShadowsSpin_, sdkContrastSpin_,
        sdkBrightnessSpin_, sdkBlackpointSpin_, sdkSaturationSpin_, sdkVibranceSpin_, sdkWarmthSpin_, sdkTintSpin_, sdkDefinitionSpin_};
    for (auto *control : sdkColorControls) control->setEnabled(hasClip);

    trimPresetCombo_->setCurrentIndex(0);
    rotationPresetCombo_->setCurrentIndex(0);
    clipInSpin_->setValue(clip.inSeconds);
    clipOutSpin_->setValue(clip.outSeconds);
    positionXSpin_->setValue(clip.positionX);
    positionYSpin_->setValue(clip.positionY);
    scaleSpin_->setValue(clip.scalePercent);
    rotationSpin_->setValue(clip.rotationDegrees);
    const bool panoramaProject = project_.settings.format == ProjectFormat::Panorama360;
    projectionCombo_->setCurrentIndex(panoramaProject ? 1 : 0);
    previewModeCombo_->setCurrentIndex(panoramaProject ? 1 : 0);
    previewViewCombo_->setCurrentIndex(panoramaProject ? 2 : 0);
    yawSpin_->setValue(clip.yawDegrees);
    pitchSpin_->setValue(clip.pitchDegrees);
    rollSpin_->setValue(clip.rollDegrees);
    sphericalMetadataCheck_->setChecked(clip.sphericalMetadata);
    colorPresetCombo_->setCurrentIndex(0);
    brightnessSpin_->setValue(clip.brightness);
    contrastSpin_->setValue(clip.contrast);
    saturationSpin_->setValue(clip.saturation);
    temperatureSpin_->setValue(clip.temperature);
    audioPresetCombo_->setCurrentIndex(0);
    audioEnabledCheck_->setChecked(clip.audioEnabled);
    volumeSpin_->setValue(clip.volumePercent);
    fadeInSpin_->setValue(clip.fadeInSeconds);
    fadeOutSpin_->setValue(clip.fadeOutSeconds);
    effectsPresetCombo_->setCurrentIndex(0);
    stabilizeCheck_->setChecked(clip.stabilize);
    denoiseCheck_->setChecked(clip.denoise);
    defringeCheck_->setChecked(clip.defringe);
    deflickerCheck_->setChecked(clip.deflicker);
    stitchFusionCheck_->setChecked(clip.stitchFusion);
    coolingShellCheck_->setChecked(clip.coolingShellDetection);
    colorPlusCheck_->setChecked(clip.colorPlus);
    colorPlusStrengthSpin_->setValue(clip.colorPlusStrength);
    sdkExposureSpin_->setValue(clip.sdkExposure);
    sdkHighlightsSpin_->setValue(clip.sdkHighlights);
    sdkShadowsSpin_->setValue(clip.sdkShadows);
    sdkContrastSpin_->setValue(clip.sdkContrast);
    sdkBrightnessSpin_->setValue(clip.sdkBrightness);
    sdkBlackpointSpin_->setValue(clip.sdkBlackpoint);
    sdkSaturationSpin_->setValue(clip.sdkSaturation);
    sdkVibranceSpin_->setValue(clip.sdkVibrance);
    sdkWarmthSpin_->setValue(clip.sdkWarmth);
    sdkTintSpin_->setValue(clip.sdkTint);
    sdkDefinitionSpin_->setValue(clip.sdkDefinition);

    updatingInspector_ = false;
    previewLabel_->setPanEnabled(hasClip && project_.settings.format == ProjectFormat::Panorama360 && previewModeCombo_->currentIndex() == 1);
}

void MainWindow::saveInspectorToClip()
{
    if (selectedTimelineIndex_ < 0 || selectedTimelineIndex_ >= project_.timeline.size()) {
        return;
    }

    TimelineClip &clip = project_.timeline[selectedTimelineIndex_];
    clip.inSeconds = clipInSpin_->value();
    clip.outSeconds = clipOutSpin_->value();
    clip.positionX = positionXSpin_->value();
    clip.positionY = positionYSpin_->value();
    clip.scalePercent = scaleSpin_->value();
    clip.rotationDegrees = rotationSpin_->value();
    clip.yawDegrees = yawSpin_->value();
    clip.pitchDegrees = pitchSpin_->value();
    clip.rollDegrees = rollSpin_->value();
    clip.sphericalMetadata = sphericalMetadataCheck_->isChecked();
    clip.brightness = brightnessSpin_->value();
    clip.contrast = contrastSpin_->value();
    clip.saturation = saturationSpin_->value();
    clip.temperature = temperatureSpin_->value();
    clip.audioEnabled = audioEnabledCheck_->isChecked();
    clip.volumePercent = volumeSpin_->value();
    clip.fadeInSeconds = fadeInSpin_->value();
    clip.fadeOutSeconds = fadeOutSpin_->value();
    clip.stabilize = stabilizeCheck_->isChecked();
    clip.denoise = denoiseCheck_->isChecked();
    clip.defringe = defringeCheck_->isChecked();
    clip.deflicker = deflickerCheck_->isChecked();
    clip.stitchFusion = stitchFusionCheck_->isChecked();
    clip.coolingShellDetection = coolingShellCheck_->isChecked();
    clip.colorPlus = colorPlusCheck_->isChecked();
    clip.colorPlusStrength = colorPlusStrengthSpin_->value();
    clip.sdkExposure = sdkExposureSpin_->value();
    clip.sdkHighlights = sdkHighlightsSpin_->value();
    clip.sdkShadows = sdkShadowsSpin_->value();
    clip.sdkContrast = sdkContrastSpin_->value();
    clip.sdkBrightness = sdkBrightnessSpin_->value();
    clip.sdkBlackpoint = sdkBlackpointSpin_->value();
    clip.sdkSaturation = sdkSaturationSpin_->value();
    clip.sdkVibrance = sdkVibranceSpin_->value();
    clip.sdkWarmth = sdkWarmthSpin_->value();
    clip.sdkTint = sdkTintSpin_->value();
    clip.sdkDefinition = sdkDefinitionSpin_->value();
    previewLabel_->setPanEnabled(project_.settings.format == ProjectFormat::Panorama360 && previewModeCombo_->currentIndex() == 1);

    if (auto *item = timeline_->item(selectedTimelineIndex_)) {
        item->setText(QStringLiteral("%1\n%2\nScale %3% | Rot %4 deg")
            .arg(selectedTimelineIndex_ + 1)
            .arg(clip.displayName)
            .arg(clip.scalePercent)
            .arg(clip.rotationDegrees));
        item->setToolTip(QStringLiteral("%1\nIn %2s | Out %3s\nYaw %4 | Pitch %5 | Roll %6")
            .arg(clip.path)
            .arg(clip.inSeconds, 0, 'f', 2)
            .arg(clip.outSeconds <= 0.0 ? QStringLiteral("Full") : QString::number(clip.outSeconds, 'f', 2))
            .arg(clip.yawDegrees)
            .arg(clip.pitchDegrees)
            .arg(clip.rollDegrees));
    }

    QString error;
    saveProject(project_, &error);
}

void MainWindow::updateProjectSummary()
{
    const QString format = project_.settings.format == ProjectFormat::Panorama360
        ? QStringLiteral("360 Panorama")
        : QStringLiteral("Standard");
    projectSummary_->setText(QStringLiteral("%1 | %2 | %3x%4 | %5 fps | %6%7")
        .arg(project_.name)
        .arg(format)
        .arg(project_.settings.width)
        .arg(project_.settings.height)
        .arg(project_.settings.fps)
        .arg(project_.settings.tenBit ? QStringLiteral("H265") : project_.settings.codec.toUpper())
        .arg(project_.settings.tenBit ? QStringLiteral(" 10-bit") : QString()));
}

void MainWindow::appendLog(const QString &line)
{
    const QString message = QStringLiteral("[%1] %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), line);
    if (logView_) {
        logView_->append(message);
    }
    statusBar()->showMessage(message, 7000);
}

bool MainWindow::importPaths(const QStringList &paths, bool copyToProject)
{
    bool imported = false;
    for (const QString &path : paths) {
        if (!isSupportedMediaFile(path)) {
            continue;
        }

        const QString projectPath = copyToProject ? copyToProjectMedia(path) : path;
        if (projectPath.isEmpty()) {
            appendLog(QStringLiteral("Gagal import: %1").arg(path));
            continue;
        }

        const QFileInfo info(projectPath);
        const bool duplicate = std::any_of(project_.media.cbegin(), project_.media.cend(), [&](const MediaItem &item) {
            return QFileInfo(item.path).canonicalFilePath() == info.canonicalFilePath();
        });
        if (duplicate) {
            continue;
        }

        MediaItem item;
        item.id = makeMediaId();
        item.displayName = info.fileName();
        item.path = projectPath;
        item.originalPath = path;
        item.kind = info.suffix().toLower();
        item.insta360 = isInsta360File(path);
        project_.media.append(item);
        imported = true;
    }

    if (imported) {
        refreshMediaList();
        saveCurrentProject();
        appendLog(QStringLiteral("Media diimport. Jumlah media: %1").arg(project_.media.size()));
    }
    return imported;
}

QString MainWindow::copyToProjectMedia(const QString &path) const
{
    const QFileInfo source(path);
    if (!source.exists()) {
        return QString();
    }

    const QString mediaDirPath = QDir(project_.projectDir).filePath(QStringLiteral("media"));
    QDir().mkpath(mediaDirPath);
    QDir mediaDir(mediaDirPath);

    QString destination = mediaDir.filePath(source.fileName());
    if (QFileInfo(destination).exists() && QFileInfo(destination).canonicalFilePath() != source.canonicalFilePath()) {
        const QString base = source.completeBaseName();
        const QString suffix = source.suffix();
        int counter = 1;
        do {
            destination = mediaDir.filePath(QStringLiteral("%1_%2.%3").arg(base).arg(counter++).arg(suffix));
        } while (QFileInfo(destination).exists());
    }

    if (QFileInfo(destination).canonicalFilePath() == source.canonicalFilePath()) {
        return destination;
    }

    if (!QFile::copy(path, destination)) {
        return QString();
    }
    return destination;
}

QStringList MainWindow::supportedFilesInDirectory(const QString &directory) const
{
    QStringList files;
    QDirIterator it(directory, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (isSupportedMediaFile(path)) {
            files.append(path);
        }
    }
    files.sort();
    return files;
}

QStringList MainWindow::cameraRoots() const
{
    QStringList roots;
    const QString user = qEnvironmentVariable("USER");
    const QStringList candidates {
        QStringLiteral("/media/%1").arg(user),
        QStringLiteral("/run/media/%1").arg(user),
        QStringLiteral("/Volumes")
    };
    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        if (!dir.exists()) {
            continue;
        }
        for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString name = entry.fileName().toLower();
            if (name.contains(QStringLiteral("insta")) || name.contains(QStringLiteral("360")) || QDir(entry.filePath()).exists(QStringLiteral("DCIM"))) {
                roots.append(entry.filePath());
            }
        }
    }
    return roots;
}

MediaItem *MainWindow::findMedia(const QString &mediaId)
{
    for (MediaItem &media : project_.media) {
        if (media.id == mediaId) {
            return &media;
        }
    }
    return nullptr;
}

const TimelineClip *MainWindow::selectedTimelineClip() const
{
    if (selectedTimelineIndex_ >= 0 && selectedTimelineIndex_ < project_.timeline.size()) {
        return &project_.timeline[selectedTimelineIndex_];
    }
    return nullptr;
}

QString MainWindow::currentPreviewPath() const
{
    if (const TimelineClip *clip = selectedTimelineClip()) {
        return clip->path;
    }
    if (auto *item = mediaList_->currentItem()) {
        return item->data(Qt::UserRole).toString();
    }
    return QString();
}

QString MainWindow::sdkExporterPath() const
{
    const QString envPath = qEnvironmentVariable("INSTA360_MEDIASDK_EXPORTER");
    if (!envPath.isEmpty() && QFileInfo::exists(envPath)) {
        return envPath;
    }

    const QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/insta360_sdk_exporter");
    if (QFileInfo::exists(bundled)) {
        return bundled;
    }

    return QStandardPaths::findExecutable(QStringLiteral("insta360_sdk_exporter"));
}

QString MainWindow::cameraToolPath() const
{
    const QString environment = qEnvironmentVariable("INSTA360_CAMERA_TOOL");
    if (!environment.isEmpty() && QFileInfo::exists(environment)) return environment;
    const QString bundled = QCoreApplication::applicationDirPath() + QStringLiteral("/insta360_camera_tool");
    if (QFileInfo::exists(bundled)) return bundled;
    return QStandardPaths::findExecutable(QStringLiteral("insta360_camera_tool"));
}

bool MainWindow::downloadRemoteMedia(MediaItem *media)
{
    if (!media || media->kind != QStringLiteral("camera-remote")) return media != nullptr;
    const QString tool = cameraToolPath();
    if (tool.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Camera download"), QStringLiteral("insta360_camera_tool tidak ditemui."));
        return false;
    }

    QList<MediaItem *> downloads {media};
    QString peerName = media->displayName;
    if (peerName.contains(QStringLiteral("_00_"))) peerName.replace(QStringLiteral("_00_"), QStringLiteral("_10_"));
    else if (peerName.contains(QStringLiteral("_10_"))) peerName.replace(QStringLiteral("_10_"), QStringLiteral("_00_"));
    else peerName.clear();
    if (!peerName.isEmpty()) {
        for (MediaItem &candidate : project_.media) {
            if (&candidate != media && candidate.kind == QStringLiteral("camera-remote") && candidate.displayName == peerName) {
                downloads.append(&candidate);
                break;
            }
        }
    }

    for (MediaItem *item : downloads) {
        const QString destination = QDir(project_.projectDir).filePath(QStringLiteral("media/") + item->displayName);
        if (!QFileInfo::exists(destination)) {
            QProgressDialog progress(QStringLiteral("Download %1 dari kamera…").arg(item->displayName),
                QStringLiteral("Cancel"), 0, 100, this);
            progress.setWindowModality(Qt::WindowModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);

            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(tool, {QStringLiteral("--download"), item->originalPath, destination});
            if (!process.waitForStarted(10000)) {
                QMessageBox::critical(this, QStringLiteral("Camera download"), process.errorString());
                return false;
            }
            QByteArray pending;
            while (!process.waitForFinished(100)) {
                qApp->processEvents();
                pending += process.readAll();
                const QList<QByteArray> lines = pending.split('\n');
                pending = lines.isEmpty() ? QByteArray() : lines.last();
                for (int i = 0; i + 1 < lines.size(); ++i) {
                    if (!lines[i].startsWith("progress=")) continue;
                    const QList<QByteArray> values = lines[i].mid(9).split('/');
                    if (values.size() == 2 && values[1].toLongLong() > 0) {
                        progress.setValue(static_cast<int>(values[0].toDouble() * 100.0 / values[1].toDouble()));
                    }
                }
                if (progress.wasCanceled()) {
                    process.terminate();
                    if (!process.waitForFinished(3000)) process.kill();
                    QFile::remove(destination);
                    return false;
                }
            }
            pending += process.readAll();
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || !QFileInfo::exists(destination)) {
                QFile::remove(destination);
                QMessageBox::critical(this, QStringLiteral("Camera download"), QString::fromUtf8(pending).trimmed());
                return false;
            }
            progress.setValue(100);
        }
        item->path = destination;
        const QString suffix = QFileInfo(destination).suffix().toLower();
        item->kind = (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")
            || suffix == QStringLiteral("png") || suffix == QStringLiteral("insp"))
            ? QStringLiteral("image") : QStringLiteral("video");
    }

    refreshMediaList();
    saveCurrentProject();
    appendLog(QStringLiteral("Media kamera dimuat turun apabila diperlukan: %1").arg(media->displayName));
    return true;
}

QStringList MainWindow::pairedInsta360Inputs(const QString &path) const
{
    QStringList inputs { path };
    const QFileInfo info(path);
    const QString fileName = info.fileName();
    QString peerName;
    if (fileName.contains(QStringLiteral("_00_"))) {
        peerName = fileName;
        peerName.replace(QStringLiteral("_00_"), QStringLiteral("_10_"));
    } else if (fileName.contains(QStringLiteral("_10_"))) {
        peerName = fileName;
        peerName.replace(QStringLiteral("_10_"), QStringLiteral("_00_"));
    }

    if (!peerName.isEmpty()) {
        const QString peerPath = info.dir().filePath(peerName);
        if (QFileInfo::exists(peerPath) && peerPath != path) {
            if (fileName.contains(QStringLiteral("_10_"))) {
                inputs.prepend(peerPath);
            } else {
                inputs.append(peerPath);
            }
        }
    }
    return inputs;
}

bool MainWindow::createSdkPreviewFrame(const QString &path, double seconds, const QString &outputPath) const
{
    const QString exporter = sdkExporterPath();
    if (exporter.isEmpty()) {
        return false;
    }

    QStringList args {
        QStringLiteral("--preview_output"), outputPath,
        QStringLiteral("--frame_index"), QString::number(qMax(0, qRound(seconds * project_.settings.fps))),
        QStringLiteral("--width"), QStringLiteral("1280"),
        QStringLiteral("--height"), QStringLiteral("640"),
        QStringLiteral("--fps"), QString::number(project_.settings.fps),
        QStringLiteral("--stitch_type"), stitchTypeToSdkArg(project_.settings.stitchType)
    };
    for (const QString &input : pairedInsta360Inputs(path)) {
        args << QStringLiteral("--input") << input;
    }
    if (project_.settings.flowState) {
        args << QStringLiteral("--flowstate");
    }
    if (!project_.settings.cuda) {
        args << QStringLiteral("--disable_cuda");
    }
    if (project_.settings.imageProcessingCpu) args << QStringLiteral("--image_processing_cpu");
    args << QStringLiteral("--camera_accessory") << QString::number(project_.settings.cameraAccessoryType)
         << QStringLiteral("--log_level") << project_.settings.sdkLogLevel;
    if (!project_.settings.aiModelPath.isEmpty()) {
        args << QStringLiteral("--model_root") << project_.settings.aiModelPath;
    }
    if (const TimelineClip *clip = selectedTimelineClip()) {
        if (clip->denoise) args << QStringLiteral("--denoise");
        if (clip->defringe) args << QStringLiteral("--defringe");
        if (clip->deflicker) args << QStringLiteral("--deflicker");
        if (clip->stitchFusion) args << QStringLiteral("--stitch_fusion");
        if (clip->coolingShellDetection) args << QStringLiteral("--cooling_shell");
        if (clip->colorPlus) args << QStringLiteral("--color_plus");
        args << QStringLiteral("--color_plus_strength") << QString::number(clip->colorPlusStrength / 100.0, 'f', 2)
             << QStringLiteral("--exposure") << QString::number(clip->sdkExposure)
             << QStringLiteral("--highlights") << QString::number(clip->sdkHighlights)
             << QStringLiteral("--shadows") << QString::number(clip->sdkShadows)
             << QStringLiteral("--contrast") << QString::number(clip->sdkContrast)
             << QStringLiteral("--brightness") << QString::number(clip->sdkBrightness)
             << QStringLiteral("--blackpoint") << QString::number(clip->sdkBlackpoint)
             << QStringLiteral("--saturation") << QString::number(clip->sdkSaturation)
             << QStringLiteral("--vibrance") << QString::number(clip->sdkVibrance)
             << QStringLiteral("--warmth") << QString::number(clip->sdkWarmth)
             << QStringLiteral("--tint") << QString::number(clip->sdkTint)
             << QStringLiteral("--definition") << QString::number(clip->sdkDefinition);
    }

    QProcess process;
    process.start(exporter, args);
    if (!process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 && QFileInfo::exists(outputPath);
}

QString MainWindow::buildPreviewVideoFilter(const TimelineClip &clip) const
{
    const int maxPreviewWidth = qMax(320, previewLabel_->width() - 24);
    int width = qMin(1280, maxPreviewWidth);
    width = qMax(2, width / 2 * 2);
    int height = qMax(2, static_cast<int>(width * (project_.settings.height / static_cast<double>(project_.settings.width))) / 2 * 2);
    const double scale = clip.scalePercent / 100.0;
    QStringList filters;

    const bool interactive360Preview = project_.settings.format == ProjectFormat::Panorama360
        && previewModeCombo_->currentIndex() == 1;
    const bool flat360Preview = project_.settings.format == ProjectFormat::Panorama360
        && !interactive360Preview
        && projectionCombo_->currentIndex() == 1;
    if (project_.settings.format == ProjectFormat::StandardVideo && isInsta360File(clip.path)) {
        filters << QStringLiteral("v360=input=equirect:output=flat:yaw=%1:pitch=%2:roll=%3:h_fov=100:v_fov=75")
            .arg(clip.yawDegrees).arg(clip.pitchDegrees).arg(clip.rollDegrees);
    } else if (flat360Preview) {
        filters << QStringLiteral("v360=input=equirect:output=flat:yaw=%1:pitch=%2:roll=%3:h_fov=100:v_fov=70")
            .arg(clip.yawDegrees)
            .arg(clip.pitchDegrees)
            .arg(clip.rollDegrees);
    } else if (project_.settings.format == ProjectFormat::Panorama360
        && (clip.yawDegrees != 0 || clip.pitchDegrees != 0 || clip.rollDegrees != 0)) {
        filters << QStringLiteral("v360=input=equirect:output=equirect:yaw=%1:pitch=%2:roll=%3")
            .arg(clip.yawDegrees)
            .arg(clip.pitchDegrees)
            .arg(clip.rollDegrees);
    }

    if (clip.scalePercent > 100) {
        filters << QStringLiteral("scale=%1:%2")
            .arg(qMax(2, static_cast<int>(width * scale) / 2 * 2))
            .arg(qMax(2, static_cast<int>(height * scale) / 2 * 2));
        filters << QStringLiteral("crop=%1:%2:%3:%4")
            .arg(width)
            .arg(height)
            .arg(QStringLiteral("(iw-%1)/2-%2").arg(width).arg(clip.positionX))
            .arg(QStringLiteral("(ih-%1)/2-%2").arg(height).arg(clip.positionY));
    } else if (clip.scalePercent < 100) {
        filters << QStringLiteral("scale=%1:%2")
            .arg(qMax(2, static_cast<int>(width * scale) / 2 * 2))
            .arg(qMax(2, static_cast<int>(height * scale) / 2 * 2));
        filters << QStringLiteral("pad=%1:%2:%3:%4:color=black")
            .arg(width)
            .arg(height)
            .arg(QStringLiteral("(ow-iw)/2+%1").arg(clip.positionX))
            .arg(QStringLiteral("(oh-ih)/2+%1").arg(clip.positionY));
    } else if (clip.positionX != 0 || clip.positionY != 0) {
        const int paddedWidth = width + qAbs(clip.positionX) * 2;
        const int paddedHeight = height + qAbs(clip.positionY) * 2;
        filters << QStringLiteral("scale=%1:%2").arg(width).arg(height);
        filters << QStringLiteral("pad=%1:%2:%3:%4:color=black")
            .arg(paddedWidth)
            .arg(paddedHeight)
            .arg((paddedWidth - width) / 2 + clip.positionX)
            .arg((paddedHeight - height) / 2 + clip.positionY);
        filters << QStringLiteral("crop=%1:%2:(iw-%1)/2:(ih-%2)/2").arg(width).arg(height);
    } else {
        filters << QStringLiteral("scale=%1:%2").arg(width).arg(height);
    }

    if (clip.rotationDegrees != 0) {
        filters << QStringLiteral("rotate=%1*PI/180:fillcolor=black").arg(clip.rotationDegrees);
        filters << QStringLiteral("scale=%1:%2").arg(width).arg(height);
    }

    if (clip.brightness != 0 || clip.contrast != 100 || clip.saturation != 100) {
        filters << QStringLiteral("eq=brightness=%1:contrast=%2:saturation=%3")
            .arg(clip.brightness / 100.0, 0, 'f', 2)
            .arg(clip.contrast / 100.0, 0, 'f', 2)
            .arg(clip.saturation / 100.0, 0, 'f', 2);
    }
    if (clip.temperature != 6500) {
        const double warm = qBound(-0.25, (clip.temperature - 6500) / 22000.0, 0.25);
        filters << QStringLiteral("colorbalance=rs=%1:bs=%2")
            .arg(warm, 0, 'f', 3)
            .arg(-warm, 0, 'f', 3);
    }
    if (clip.stabilize) {
        filters << QStringLiteral("deshake");
    }
    if (clip.denoise) {
        filters << QStringLiteral("hqdn3d");
    }
    if (clip.deflicker) {
        filters << QStringLiteral("deflicker");
    }
    if (clip.colorPlus) {
        filters << QStringLiteral("eq=contrast=1.06:saturation=1.10");
    }

    return filters.join(QStringLiteral(","));
}

void MainWindow::renderPreviewFrame(const QString &path, double seconds)
{
    if (path.isEmpty()) {
        return;
    }
    const QFileInfo info(path);
    if (!info.exists()) {
        previewLabel_->setText(QStringLiteral("Fail tidak ditemui"));
        return;
    }

    const TimelineClip *clip = selectedTimelineClip();
    const bool previewTimelineClip = clip && QFileInfo(clip->path).canonicalFilePath() == info.canonicalFilePath();
    const QString suffix = info.suffix().toLower();
    if (!previewTimelineClip && (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg"))) {
        QPixmap pix(path);
        previewLabel_->setImageFrame(pix.toImage());
        return;
    }

    QTemporaryFile imageFile(QDir::tempPath() + QStringLiteral("/insta360_preview_XXXXXX.jpg"));
    imageFile.setAutoRemove(false);
    if (!imageFile.open()) {
        previewLabel_->setText(QStringLiteral("Gagal cipta preview"));
        return;
    }
    const QString imagePath = imageFile.fileName();
    imageFile.close();

    QString sourcePath = path;
    QString sdkFramePath;
    if (previewTimelineClip && isInsta360File(path)) {
        QTemporaryFile sdkFrame(QDir::tempPath() + QStringLiteral("/insta360_sdk_preview_XXXXXX.jpg"));
        sdkFrame.setAutoRemove(false);
        if (sdkFrame.open()) {
            sdkFramePath = sdkFrame.fileName();
            sdkFrame.close();
            if (createSdkPreviewFrame(path, seconds, sdkFramePath)) {
                sourcePath = sdkFramePath;
            } else {
                QFile::remove(sdkFramePath);
                sdkFramePath.clear();
            }
        }
    }

    QStringList args {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), QString::number(sdkFramePath.isEmpty() ? seconds : 0.0, 'f', 2),
        QStringLiteral("-i"), sourcePath,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-vf"), previewTimelineClip ? buildPreviewVideoFilter(*clip) : QStringLiteral("scale=960:-1"),
        imagePath
    };

    QProcess process;
    process.start(QStringLiteral("ffmpeg"), args);
    process.waitForFinished(5000);
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(isInsta360File(path)
            ? QStringLiteral("Preview 360 Insta360 mentah perlukan insta360_sdk_exporter dan MediaSDK rasmi.")
            : QStringLiteral("Preview tidak tersedia untuk codec fail ini."));
        QFile::remove(imagePath);
        if (!sdkFramePath.isEmpty()) {
            QFile::remove(sdkFramePath);
        }
        return;
    }

    QPixmap pix(imagePath);
    QFile::remove(imagePath);
    if (!sdkFramePath.isEmpty()) {
        QFile::remove(sdkFramePath);
    }
    if (pix.isNull()) {
        previewLabel_->setText(QStringLiteral("Preview tidak tersedia"));
        return;
    }
    const bool interactive360Preview = previewTimelineClip
        && project_.settings.format == ProjectFormat::Panorama360
        && previewModeCombo_->currentIndex() == 1;
    if (interactive360Preview && clip) {
        previewLabel_->setPanoramaFrame(pix.toImage(), clip->yawDegrees, clip->pitchDegrees, clip->rollDegrees);
    } else {
        previewLabel_->setImageFrame(pix.toImage());
    }
}

void MainWindow::renderSelectedTimelinePreview()
{
    const TimelineClip *clip = selectedTimelineClip();
    if (!clip) {
        return;
    }
    previewTitle_->setText(QStringLiteral("Video Preview - %1").arg(clip->displayName));
    const double seconds = qMax(previewSeconds_, clip->inSeconds);
    renderPreviewFrame(clip->path, seconds);
}

void MainWindow::startPreviewPlayback()
{
    const QString path = currentPreviewPath();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg") || suffix == QStringLiteral("insp")) {
        renderPreviewFrame(path, previewSeconds_);
        return;
    }

    const TimelineClip *clip = selectedTimelineClip();
    const bool previewTimelineClip = clip && QFileInfo(clip->path).canonicalFilePath() == info.canonicalFilePath();
    playbackPanorama_ = previewTimelineClip
        && project_.settings.format == ProjectFormat::Panorama360
        && previewModeCombo_->currentIndex() == 1;

    QString filter = previewTimelineClip ? buildPreviewVideoFilter(*clip) : QStringLiteral("scale=960:-1");
    if (!filter.isEmpty()) {
        filter += QStringLiteral(",fps=12");
    } else {
        filter = QStringLiteral("fps=12");
    }

    playbackBuffer_.clear();
    playbackProcess_ = new QProcess(this);
    playbackProcess_->setProcessChannelMode(QProcess::SeparateChannels);

    QStringList args {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-re"),
        QStringLiteral("-ss"), QString::number(previewSeconds_, 'f', 2),
        QStringLiteral("-i"), path,
        QStringLiteral("-an"),
        QStringLiteral("-vf"), filter,
        QStringLiteral("-f"), QStringLiteral("image2pipe"),
        QStringLiteral("-vcodec"), QStringLiteral("mjpeg"),
        QStringLiteral("-q:v"), QStringLiteral("6"),
        QStringLiteral("-")
    };

    connect(playbackProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readPlaybackFrames);
    connect(playbackProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
        if (playbackProcess_) {
            playbackProcess_->deleteLater();
            playbackProcess_ = nullptr;
        }
        playbackBuffer_.clear();
        playButton_->setText(QStringLiteral("Play"));
        playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    });

    playbackProcess_->start(QStringLiteral("ffmpeg"), args);
    if (!playbackProcess_->waitForStarted(1000)) {
        appendLog(QStringLiteral("Preview playback gagal mula: %1").arg(playbackProcess_->errorString()));
        playbackProcess_->deleteLater();
        playbackProcess_ = nullptr;
        return;
    }

    playButton_->setText(QStringLiteral("Pause"));
    playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
}

void MainWindow::stopPreviewPlayback()
{
    if (!playbackProcess_) {
        playButton_->setText(QStringLiteral("Play"));
        playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        return;
    }

    QProcess *process = playbackProcess_;
    playbackProcess_ = nullptr;
    process->disconnect(this);
    process->terminate();
    if (!process->waitForFinished(500)) {
        process->kill();
        process->waitForFinished();
    }
    process->deleteLater();
    playbackBuffer_.clear();
    playButton_->setText(QStringLiteral("Play"));
    playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void MainWindow::readPlaybackFrames()
{
    if (!playbackProcess_) {
        return;
    }

    playbackBuffer_.append(playbackProcess_->readAllStandardOutput());
    const QByteArray startMarker = QByteArray::fromHex("ffd8");
    const QByteArray endMarker = QByteArray::fromHex("ffd9");

    while (true) {
        const int start = playbackBuffer_.indexOf(startMarker);
        if (start < 0) {
            if (playbackBuffer_.size() > 1024 * 1024) {
                playbackBuffer_.clear();
            }
            return;
        }
        if (start > 0) {
            playbackBuffer_.remove(0, start);
        }

        const int end = playbackBuffer_.indexOf(endMarker, 2);
        if (end < 0) {
            return;
        }

        const QByteArray frame = playbackBuffer_.left(end + 2);
        playbackBuffer_.remove(0, end + 2);
        const QImage image = QImage::fromData(frame, "JPG");
        if (!image.isNull()) {
            displayPreviewImage(image, playbackPanorama_);
        }
    }
}

void MainWindow::displayPreviewImage(const QImage &image, bool panorama)
{
    const TimelineClip *clip = selectedTimelineClip();
    if (panorama && clip) {
        previewLabel_->setPanoramaFrame(image, clip->yawDegrees, clip->pitchDegrees, clip->rollDegrees);
    } else {
        previewLabel_->setImageFrame(image);
    }
}

void MainWindow::setEditorEnabled(bool enabled)
{
    editorPage_->setEnabled(enabled);
}
