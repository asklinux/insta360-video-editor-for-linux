#pragma once

#include "Project.h"

#include <QMainWindow>

class QLabel;
class QImage;
class QListWidget;
class QListWidgetItem;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QProcess;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QSlider;
class QStackedWidget;
class QTabWidget;
class QTextEdit;
class QThread;
class QTimer;
class TimelineWidget;
class VideoPreviewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void newProject();
    void openProjectDialog();
    void saveCurrentProject();
    void editProjectSettings();
    void importMediaDialog();
    void importFromCamera();
    void addSelectedMediaToTimeline();
    void addMediaToTimeline(const QString &mediaId);
    void importDroppedFiles(const QStringList &paths);
    void previewMedia(QListWidgetItem *item);
    void exportProject();
    void exportSelectedFrames();
    void showSelectedMediaInfo();
    void showSdkStatus();
    void openLiveCamera();
    void togglePreviewPlayback();
    void renderNextPreviewFrame();
    void readPlaybackFrames();
    void timelineSelectionChanged();
    void inspectorValueChanged();
    void previewPanDragged(int deltaX, int deltaY);

private:
    QWidget *buildStartPage();
    QWidget *buildEditorPage();
    QWidget *buildInspector();
    QWidget *buildVideoInspectorTab();
    QWidget *build360InspectorTab();
    QWidget *buildColorInspectorTab();
    QWidget *buildAudioInspectorTab();
    QWidget *buildEffectsInspectorTab();
    QWidget *buildSdkInspectorTab();
    void buildMenus();
    void applyStyle();
    void setProject(const Project &project);
    void refreshMediaList();
    void refreshTimeline();
    void loadInspectorFromClip();
    void saveInspectorToClip();
    void updateProjectSummary();
    void appendLog(const QString &line);
    bool importPaths(const QStringList &paths, bool copyToProject);
    QString copyToProjectMedia(const QString &path) const;
    QStringList supportedFilesInDirectory(const QString &directory) const;
    QStringList cameraRoots() const;
    MediaItem *findMedia(const QString &mediaId);
    const TimelineClip *selectedTimelineClip() const;
    QString currentPreviewPath() const;
    QString sdkExporterPath() const;
    QStringList pairedInsta360Inputs(const QString &path) const;
    bool createSdkPreviewFrame(const QString &path, double seconds, const QString &outputPath) const;
    QString buildPreviewVideoFilter(const TimelineClip &clip) const;
    void renderPreviewFrame(const QString &path, double seconds);
    void renderSelectedTimelinePreview();
    void startPreviewPlayback();
    void stopPreviewPlayback();
    void displayPreviewImage(const QImage &image, bool panorama);
    void setEditorEnabled(bool enabled);

    QStackedWidget *stack_ = nullptr;
    QWidget *startPage_ = nullptr;
    QWidget *editorPage_ = nullptr;
    QLabel *projectSummary_ = nullptr;
    QListWidget *mediaList_ = nullptr;
    TimelineWidget *timeline_ = nullptr;
    VideoPreviewWidget *previewLabel_ = nullptr;
    QLabel *previewTitle_ = nullptr;
    QComboBox *previewViewCombo_ = nullptr;
    QPushButton *reset360ViewButton_ = nullptr;
    QTabWidget *inspectorTabs_ = nullptr;
    QPushButton *playButton_ = nullptr;
    QSlider *previewSlider_ = nullptr;
    QProgressBar *exportProgress_ = nullptr;
    QPushButton *cancelExportButton_ = nullptr;
    QTextEdit *logView_ = nullptr;
    QTimer *previewTimer_ = nullptr;
    QTimer *inspectorPreviewTimer_ = nullptr;
    QProcess *playbackProcess_ = nullptr;
    QByteArray playbackBuffer_;
    bool playbackPanorama_ = false;

    QDoubleSpinBox *clipInSpin_ = nullptr;
    QDoubleSpinBox *clipOutSpin_ = nullptr;
    QComboBox *trimPresetCombo_ = nullptr;
    QComboBox *rotationPresetCombo_ = nullptr;
    QSpinBox *positionXSpin_ = nullptr;
    QSpinBox *positionYSpin_ = nullptr;
    QSpinBox *scaleSpin_ = nullptr;
    QSpinBox *rotationSpin_ = nullptr;
    QComboBox *projectionCombo_ = nullptr;
    QComboBox *previewModeCombo_ = nullptr;
    QSpinBox *yawSpin_ = nullptr;
    QSpinBox *pitchSpin_ = nullptr;
    QSpinBox *rollSpin_ = nullptr;
    QCheckBox *sphericalMetadataCheck_ = nullptr;
    QComboBox *colorPresetCombo_ = nullptr;
    QSpinBox *brightnessSpin_ = nullptr;
    QSpinBox *contrastSpin_ = nullptr;
    QSpinBox *saturationSpin_ = nullptr;
    QSpinBox *temperatureSpin_ = nullptr;
    QComboBox *audioPresetCombo_ = nullptr;
    QCheckBox *audioEnabledCheck_ = nullptr;
    QSpinBox *volumeSpin_ = nullptr;
    QDoubleSpinBox *fadeInSpin_ = nullptr;
    QDoubleSpinBox *fadeOutSpin_ = nullptr;
    QComboBox *effectsPresetCombo_ = nullptr;
    QCheckBox *stabilizeCheck_ = nullptr;
    QCheckBox *denoiseCheck_ = nullptr;
    QCheckBox *defringeCheck_ = nullptr;
    QCheckBox *deflickerCheck_ = nullptr;
    QCheckBox *stitchFusionCheck_ = nullptr;
    QCheckBox *coolingShellCheck_ = nullptr;
    QCheckBox *colorPlusCheck_ = nullptr;
    QSpinBox *colorPlusStrengthSpin_ = nullptr;
    QSpinBox *sdkExposureSpin_ = nullptr;
    QSpinBox *sdkHighlightsSpin_ = nullptr;
    QSpinBox *sdkShadowsSpin_ = nullptr;
    QSpinBox *sdkContrastSpin_ = nullptr;
    QSpinBox *sdkBrightnessSpin_ = nullptr;
    QSpinBox *sdkBlackpointSpin_ = nullptr;
    QSpinBox *sdkSaturationSpin_ = nullptr;
    QSpinBox *sdkVibranceSpin_ = nullptr;
    QSpinBox *sdkWarmthSpin_ = nullptr;
    QSpinBox *sdkTintSpin_ = nullptr;
    QSpinBox *sdkDefinitionSpin_ = nullptr;

    Project project_;
    bool hasProject_ = false;
    bool updatingInspector_ = false;
    bool dragging360Preview_ = false;
    int selectedTimelineIndex_ = -1;
    double previewSeconds_ = 0.0;
};
