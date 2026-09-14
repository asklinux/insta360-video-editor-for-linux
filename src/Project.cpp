#include "Project.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>

QString projectFormatToString(ProjectFormat format)
{
    return format == ProjectFormat::Panorama360 ? QStringLiteral("360_panorama") : QStringLiteral("standard");
}

ProjectFormat projectFormatFromString(const QString &value)
{
    return value == QStringLiteral("standard") ? ProjectFormat::StandardVideo : ProjectFormat::Panorama360;
}

QString stitchTypeToSdkArg(StitchType type)
{
    switch (type) {
    case StitchType::Template:
        return QStringLiteral("template");
    case StitchType::DynamicStitch:
        return QStringLiteral("dynamicstitch");
    case StitchType::AiFlow:
        return QStringLiteral("aistitch");
    case StitchType::OptFlow:
    default:
        return QStringLiteral("optflow");
    }
}

QString stitchTypeToLabel(StitchType type)
{
    switch (type) {
    case StitchType::Template:
        return QStringLiteral("Template");
    case StitchType::DynamicStitch:
        return QStringLiteral("Dynamic Stitch");
    case StitchType::AiFlow:
        return QStringLiteral("AI Flow");
    case StitchType::OptFlow:
    default:
        return QStringLiteral("Optical Flow");
    }
}

QJsonObject exportSettingsToJson(const ExportSettings &settings)
{
    QJsonObject object;
    object["format"] = projectFormatToString(settings.format);
    object["width"] = settings.width;
    object["height"] = settings.height;
    object["fps"] = settings.fps;
    object["bitrateMbps"] = settings.bitrateMbps;
    object["codec"] = settings.codec;
    object["stitchType"] = stitchTypeToSdkArg(settings.stitchType);
    object["flowState"] = settings.flowState;
    object["directionLock"] = settings.directionLock;
    object["cuda"] = settings.cuda;
    object["softEncode"] = settings.softEncode;
    object["softDecode"] = settings.softDecode;
    object["tenBit"] = settings.tenBit;
    object["imageProcessingCpu"] = settings.imageProcessingCpu;
    object["exportStabilizationData"] = settings.exportStabilizationData;
    object["cameraAccessoryType"] = settings.cameraAccessoryType;
    object["sdkLogLevel"] = settings.sdkLogLevel;
    object["sdkLogPath"] = settings.sdkLogPath;
    object["aiModelPath"] = settings.aiModelPath;
    return object;
}

ExportSettings exportSettingsFromJson(const QJsonObject &object)
{
    ExportSettings settings;
    settings.format = projectFormatFromString(object["format"].toString(projectFormatToString(settings.format)));
    settings.width = object["width"].toInt(settings.width);
    settings.height = object["height"].toInt(settings.height);
    settings.fps = object["fps"].toInt(settings.fps);
    settings.bitrateMbps = object["bitrateMbps"].toInt(settings.bitrateMbps);
    settings.codec = object["codec"].toString(settings.codec);

    const QString stitch = object["stitchType"].toString(QStringLiteral("optflow"));
    if (stitch == QStringLiteral("template")) {
        settings.stitchType = StitchType::Template;
    } else if (stitch == QStringLiteral("dynamicstitch")) {
        settings.stitchType = StitchType::DynamicStitch;
    } else if (stitch == QStringLiteral("aistitch")) {
        settings.stitchType = StitchType::AiFlow;
    } else {
        settings.stitchType = StitchType::OptFlow;
    }

    settings.flowState = object["flowState"].toBool(settings.flowState);
    settings.directionLock = object["directionLock"].toBool(settings.directionLock);
    settings.cuda = object["cuda"].toBool(settings.cuda);
    settings.softEncode = object["softEncode"].toBool(settings.softEncode);
    settings.softDecode = object["softDecode"].toBool(settings.softDecode);
    settings.tenBit = object["tenBit"].toBool(settings.tenBit);
    settings.imageProcessingCpu = object["imageProcessingCpu"].toBool(settings.imageProcessingCpu);
    settings.exportStabilizationData = object["exportStabilizationData"].toBool(settings.exportStabilizationData);
    settings.cameraAccessoryType = object["cameraAccessoryType"].toInt(settings.cameraAccessoryType);
    settings.sdkLogLevel = object["sdkLogLevel"].toString(settings.sdkLogLevel);
    settings.sdkLogPath = object["sdkLogPath"].toString();
    settings.aiModelPath = object["aiModelPath"].toString();
    return settings;
}

QJsonObject projectToJson(const Project &project)
{
    QJsonObject object;
    object["schema"] = QStringLiteral("insta360-editor-project-v1");
    object["name"] = project.name;
    object["projectFile"] = project.projectFile;
    object["projectDir"] = project.projectDir;
    object["settings"] = exportSettingsToJson(project.settings);

    QJsonArray mediaArray;
    for (const auto &item : project.media) {
        QJsonObject media;
        media["id"] = item.id;
        media["displayName"] = item.displayName;
        media["path"] = item.path;
        media["originalPath"] = item.originalPath;
        media["kind"] = item.kind;
        media["insta360"] = item.insta360;
        mediaArray.append(media);
    }
    object["media"] = mediaArray;

    QJsonArray timelineArray;
    for (const auto &clip : project.timeline) {
        QJsonObject item;
        item["mediaId"] = clip.mediaId;
        item["displayName"] = clip.displayName;
        item["path"] = clip.path;
        item["inSeconds"] = clip.inSeconds;
        item["outSeconds"] = clip.outSeconds;
        item["positionX"] = clip.positionX;
        item["positionY"] = clip.positionY;
        item["scalePercent"] = clip.scalePercent;
        item["rotationDegrees"] = clip.rotationDegrees;
        item["yawDegrees"] = clip.yawDegrees;
        item["pitchDegrees"] = clip.pitchDegrees;
        item["rollDegrees"] = clip.rollDegrees;
        item["sphericalMetadata"] = clip.sphericalMetadata;
        item["brightness"] = clip.brightness;
        item["contrast"] = clip.contrast;
        item["saturation"] = clip.saturation;
        item["temperature"] = clip.temperature;
        item["audioEnabled"] = clip.audioEnabled;
        item["volumePercent"] = clip.volumePercent;
        item["fadeInSeconds"] = clip.fadeInSeconds;
        item["fadeOutSeconds"] = clip.fadeOutSeconds;
        item["stabilize"] = clip.stabilize;
        item["denoise"] = clip.denoise;
        item["defringe"] = clip.defringe;
        item["deflicker"] = clip.deflicker;
        item["stitchFusion"] = clip.stitchFusion;
        item["coolingShellDetection"] = clip.coolingShellDetection;
        item["colorPlus"] = clip.colorPlus;
        item["colorPlusStrength"] = clip.colorPlusStrength;
        item["sdkExposure"] = clip.sdkExposure;
        item["sdkHighlights"] = clip.sdkHighlights;
        item["sdkShadows"] = clip.sdkShadows;
        item["sdkContrast"] = clip.sdkContrast;
        item["sdkBrightness"] = clip.sdkBrightness;
        item["sdkBlackpoint"] = clip.sdkBlackpoint;
        item["sdkSaturation"] = clip.sdkSaturation;
        item["sdkVibrance"] = clip.sdkVibrance;
        item["sdkWarmth"] = clip.sdkWarmth;
        item["sdkTint"] = clip.sdkTint;
        item["sdkDefinition"] = clip.sdkDefinition;
        timelineArray.append(item);
    }
    object["timeline"] = timelineArray;
    return object;
}

bool projectFromJson(const QJsonObject &object, Project *project, QString *error)
{
    if (!project) {
        return false;
    }
    if (object["schema"].toString() != QStringLiteral("insta360-editor-project-v1")) {
        if (error) {
            *error = QStringLiteral("Fail projek tidak dikenali.");
        }
        return false;
    }

    Project loaded;
    loaded.name = object["name"].toString();
    loaded.projectFile = object["projectFile"].toString();
    loaded.projectDir = object["projectDir"].toString();
    loaded.settings = exportSettingsFromJson(object["settings"].toObject());

    for (const auto value : object["media"].toArray()) {
        const QJsonObject media = value.toObject();
        MediaItem item;
        item.id = media["id"].toString();
        item.displayName = media["displayName"].toString();
        item.path = media["path"].toString();
        item.originalPath = media["originalPath"].toString();
        item.kind = media["kind"].toString();
        item.insta360 = media["insta360"].toBool();
        loaded.media.append(item);
    }

    for (const auto value : object["timeline"].toArray()) {
        const QJsonObject timeline = value.toObject();
        TimelineClip clip;
        clip.mediaId = timeline["mediaId"].toString();
        clip.displayName = timeline["displayName"].toString();
        clip.path = timeline["path"].toString();
        clip.inSeconds = timeline["inSeconds"].toDouble();
        clip.outSeconds = timeline["outSeconds"].toDouble();
        clip.positionX = timeline["positionX"].toInt();
        clip.positionY = timeline["positionY"].toInt();
        clip.scalePercent = timeline["scalePercent"].toInt(100);
        clip.rotationDegrees = timeline["rotationDegrees"].toInt();
        clip.yawDegrees = timeline["yawDegrees"].toInt();
        clip.pitchDegrees = timeline["pitchDegrees"].toInt();
        clip.rollDegrees = timeline["rollDegrees"].toInt();
        clip.sphericalMetadata = timeline["sphericalMetadata"].toBool(true);
        clip.brightness = timeline["brightness"].toInt();
        clip.contrast = timeline["contrast"].toInt(100);
        clip.saturation = timeline["saturation"].toInt(100);
        clip.temperature = timeline["temperature"].toInt(6500);
        clip.audioEnabled = timeline["audioEnabled"].toBool(true);
        clip.volumePercent = timeline["volumePercent"].toInt(100);
        clip.fadeInSeconds = timeline["fadeInSeconds"].toDouble();
        clip.fadeOutSeconds = timeline["fadeOutSeconds"].toDouble();
        clip.stabilize = timeline["stabilize"].toBool();
        clip.denoise = timeline["denoise"].toBool();
        clip.defringe = timeline["defringe"].toBool();
        clip.deflicker = timeline["deflicker"].toBool();
        clip.stitchFusion = timeline["stitchFusion"].toBool();
        clip.coolingShellDetection = timeline["coolingShellDetection"].toBool();
        clip.colorPlus = timeline["colorPlus"].toBool();
        clip.colorPlusStrength = timeline["colorPlusStrength"].toInt(100);
        clip.sdkExposure = timeline["sdkExposure"].toInt();
        clip.sdkHighlights = timeline["sdkHighlights"].toInt();
        clip.sdkShadows = timeline["sdkShadows"].toInt();
        clip.sdkContrast = timeline["sdkContrast"].toInt();
        clip.sdkBrightness = timeline["sdkBrightness"].toInt();
        clip.sdkBlackpoint = timeline["sdkBlackpoint"].toInt();
        clip.sdkSaturation = timeline["sdkSaturation"].toInt();
        clip.sdkVibrance = timeline["sdkVibrance"].toInt();
        clip.sdkWarmth = timeline["sdkWarmth"].toInt();
        clip.sdkTint = timeline["sdkTint"].toInt();
        clip.sdkDefinition = timeline["sdkDefinition"].toInt();
        loaded.timeline.append(clip);
    }

    *project = loaded;
    return true;
}

bool saveProject(const Project &project, QString *error)
{
    QDir().mkpath(project.projectDir);
    QFile file(project.projectFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    file.write(QJsonDocument(projectToJson(project)).toJson(QJsonDocument::Indented));
    return true;
}

bool loadProject(const QString &filePath, Project *project, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = parseError.errorString();
        }
        return false;
    }
    if (!projectFromJson(doc.object(), project, error)) {
        return false;
    }
    project->projectFile = filePath;
    project->projectDir = QFileInfo(filePath).absolutePath();
    return true;
}

QStringList supportedMediaFilters()
{
    return {
        QStringLiteral("Insta360 and video (*.insv *.insp *.lrv *.mp4 *.mov *.m4v *.jpg *.jpeg)"),
        QStringLiteral("Insta360 media (*.insv *.insp *.lrv)"),
        QStringLiteral("Video (*.mp4 *.mov *.m4v)"),
        QStringLiteral("Images (*.jpg *.jpeg)")
    };
}

bool isSupportedMediaFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("insv") || suffix == QStringLiteral("insp")
        || suffix == QStringLiteral("lrv") || suffix == QStringLiteral("mp4")
        || suffix == QStringLiteral("mov") || suffix == QStringLiteral("m4v")
        || suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg");
}

bool isInsta360File(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("insv") || suffix == QStringLiteral("insp") || suffix == QStringLiteral("lrv");
}

QString makeMediaId()
{
    return QStringLiteral("m%1%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
}
