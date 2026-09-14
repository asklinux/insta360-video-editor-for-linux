#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ProjectFormat {
    StandardVideo,
    Panorama360
};

enum class StitchType {
    Template,
    OptFlow,
    DynamicStitch,
    AiFlow
};

struct ExportSettings {
    ProjectFormat format = ProjectFormat::Panorama360;
    int width = 3840;
    int height = 1920;
    int fps = 30;
    int bitrateMbps = 60;
    QString codec = QStringLiteral("h264");
    StitchType stitchType = StitchType::OptFlow;
    bool flowState = true;
    bool directionLock = false;
    bool cuda = true;
    bool softEncode = false;
    bool softDecode = false;
    bool tenBit = false;
    bool imageProcessingCpu = false;
    bool exportStabilizationData = false;
    int cameraAccessoryType = -1;
    QString sdkLogLevel = QStringLiteral("error");
    QString sdkLogPath;
    QString aiModelPath;
};

struct MediaItem {
    QString id;
    QString displayName;
    QString path;
    QString originalPath;
    QString kind;
    bool insta360 = false;
};

struct TimelineClip {
    QString mediaId;
    QString displayName;
    QString path;
    double inSeconds = 0.0;
    double outSeconds = 0.0;
    int positionX = 0;
    int positionY = 0;
    int scalePercent = 100;
    int rotationDegrees = 0;
    int yawDegrees = 0;
    int pitchDegrees = 0;
    int rollDegrees = 0;
    bool sphericalMetadata = true;
    int brightness = 0;
    int contrast = 100;
    int saturation = 100;
    int temperature = 6500;
    bool audioEnabled = true;
    int volumePercent = 100;
    double fadeInSeconds = 0.0;
    double fadeOutSeconds = 0.0;
    bool stabilize = false;
    bool denoise = false;
    bool defringe = false;
    bool deflicker = false;
    bool stitchFusion = false;
    bool coolingShellDetection = false;
    bool colorPlus = false;
    int colorPlusStrength = 100;
    int sdkExposure = 0;
    int sdkHighlights = 0;
    int sdkShadows = 0;
    int sdkContrast = 0;
    int sdkBrightness = 0;
    int sdkBlackpoint = 0;
    int sdkSaturation = 0;
    int sdkVibrance = 0;
    int sdkWarmth = 0;
    int sdkTint = 0;
    int sdkDefinition = 0;
};

struct Project {
    QString name;
    QString projectFile;
    QString projectDir;
    ExportSettings settings;
    QVector<MediaItem> media;
    QVector<TimelineClip> timeline;
};

QString projectFormatToString(ProjectFormat format);
ProjectFormat projectFormatFromString(const QString &value);
QString stitchTypeToSdkArg(StitchType type);
QString stitchTypeToLabel(StitchType type);

QJsonObject exportSettingsToJson(const ExportSettings &settings);
ExportSettings exportSettingsFromJson(const QJsonObject &object);

QJsonObject projectToJson(const Project &project);
bool projectFromJson(const QJsonObject &object, Project *project, QString *error);
bool saveProject(const Project &project, QString *error);
bool loadProject(const QString &filePath, Project *project, QString *error);

QStringList supportedMediaFilters();
bool isSupportedMediaFile(const QString &path);
bool isInsta360File(const QString &path);
QString makeMediaId();
