#include "ExportWorker.h"
#include "SphericalMetadata.h"

#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtMath>

static QString escapedConcatPath(const QString &path)
{
    QString escaped = path;
    escaped.replace(QStringLiteral("'"), QStringLiteral("'\\''"));
    return QStringLiteral("file '%1'\n").arg(escaped);
}

ExportWorker::ExportWorker(Project project, QString outputPath)
    : project_(std::move(project))
    , outputPath_(std::move(outputPath))
{
}

void ExportWorker::cancel()
{
    cancelRequested_.store(true);
}

void ExportWorker::run()
{
    if (project_.timeline.isEmpty()) {
        emit finished(false, QStringLiteral("Timeline kosong."));
        return;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        emit finished(false, QStringLiteral("Tidak boleh cipta direktori sementara."));
        return;
    }

    QStringList clipOutputs;
    int index = 0;
    for (const TimelineClip &clip : project_.timeline) {
        if (cancelRequested_.load()) {
            emit finished(false, QStringLiteral("Export dibatalkan."));
            return;
        }
        const QString clipOutput = QDir(tempDir.path()).filePath(QStringLiteral("clip_%1.mp4").arg(index, 4, 10, QLatin1Char('0')));
        emit message(QStringLiteral("Export klip: %1").arg(clip.displayName));
        QString error;
        if (!exportClip(clip, clipOutput, &error)) {
            emit finished(false, error);
            return;
        }
        clipOutputs.append(clipOutput);
        ++index;
        emit progress(qRound((index * 70.0) / project_.timeline.size()));
    }

    const QString concatFile = QDir(tempDir.path()).filePath(QStringLiteral("concat.txt"));
    QFile concat(concatFile);
    if (!concat.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        emit finished(false, concat.errorString());
        return;
    }
    for (const QString &path : clipOutputs) {
        concat.write(escapedConcatPath(path).toUtf8());
    }
    concat.close();

    QString finalMp4 = outputPath_;
    QString workingOutput = finalMp4;
    const bool write360Metadata = project_.settings.format == ProjectFormat::Panorama360
        && std::any_of(project_.timeline.cbegin(), project_.timeline.cend(), [](const TimelineClip &clip) {
            return clip.sphericalMetadata;
        });
    if (write360Metadata) {
        workingOutput = QDir(tempDir.path()).filePath(QStringLiteral("stitched_without_metadata.mp4"));
    }

    emit message(QStringLiteral("Gabung timeline."));
    QStringList concatArgs {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-f"), QStringLiteral("concat"),
        QStringLiteral("-safe"), QStringLiteral("0"),
        QStringLiteral("-i"), concatFile,
        QStringLiteral("-c"), QStringLiteral("copy"),
        QStringLiteral("-movflags"), QStringLiteral("+faststart"),
        workingOutput
    };
    QString error;
    if (!runProcess(QStringLiteral("ffmpeg"), concatArgs, &error)) {
        emit finished(false, error);
        return;
    }
    emit progress(85);

    if (write360Metadata) {
        emit message(QStringLiteral("Masukkan metadata 360 equirectangular."));
        if (!SphericalMetadata::inject(workingOutput, finalMp4, &error)) {
            emit finished(false, error);
            return;
        }
    }

    emit progress(100);
    emit finished(true, QStringLiteral("Export selesai: %1").arg(finalMp4));
}

bool ExportWorker::exportClip(const TimelineClip &clip, const QString &outputPath, QString *error)
{
    const QFileInfo info(clip.path);
    if (!info.exists()) {
        if (error) {
            *error = QStringLiteral("Fail media tidak ditemui: %1").arg(clip.path);
        }
        return false;
    }

    const bool image = info.suffix().compare(QStringLiteral("jpg"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("jpeg"), Qt::CaseInsensitive) == 0
        || info.suffix().compare(QStringLiteral("insp"), Qt::CaseInsensitive) == 0;

    const QString sdkPath = sdkExporterPath();
    const bool sdkMedia = isInsta360File(clip.path) && !sdkPath.isEmpty();
    if (sdkMedia) {
        emit message(project_.settings.cuda
            ? QStringLiteral("Guna Insta360 MediaSDK dengan GPU/CUDA.")
            : QStringLiteral("Guna Insta360 MediaSDK tanpa CUDA."));
        QTemporaryDir sdkTempDir;
        if (!sdkTempDir.isValid()) {
            if (error) {
                *error = QStringLiteral("Tidak boleh cipta direktori sementara SDK.");
            }
            return false;
        }
        const QString sdkOutput = QDir(sdkTempDir.path()).filePath(
            image ? QStringLiteral("sdk_stitched.jpg") : QStringLiteral("sdk_stitched.mp4"));
        const int sdkWidth = project_.settings.format == ProjectFormat::Panorama360
            ? project_.settings.width : qMax(3840, project_.settings.width * 2);
        const int sdkHeight = sdkWidth / 2;
        QStringList args {
            QStringLiteral("--output"), sdkOutput,
            QStringLiteral("--width"), QString::number(sdkWidth),
            QStringLiteral("--height"), QString::number(sdkHeight),
            QStringLiteral("--fps"), QString::number(project_.settings.fps),
            QStringLiteral("--bitrate"), QString::number(project_.settings.bitrateMbps * 1000 * 1000),
            QStringLiteral("--stitch_type"), stitchTypeToSdkArg(project_.settings.stitchType)
        };
        for (const QString &input : pairedInsta360Inputs(clip.path)) {
            args << QStringLiteral("--input") << input;
        }
        if (project_.settings.codec == QStringLiteral("h265")) {
            args << QStringLiteral("--h265");
        }
        if (project_.settings.flowState) {
            args << QStringLiteral("--flowstate");
        }
        if (project_.settings.directionLock) {
            args << QStringLiteral("--direction_lock");
        }
        if (!project_.settings.cuda) {
            args << QStringLiteral("--disable_cuda");
        }
        if (project_.settings.softEncode) {
            args << QStringLiteral("--soft_encode");
        }
        if (project_.settings.softDecode) {
            args << QStringLiteral("--soft_decode");
        }
        if (project_.settings.tenBit) args << QStringLiteral("--ten_bit");
        if (project_.settings.imageProcessingCpu) args << QStringLiteral("--image_processing_cpu");
        args << QStringLiteral("--camera_accessory") << QString::number(project_.settings.cameraAccessoryType)
             << QStringLiteral("--log_level") << project_.settings.sdkLogLevel;
        if (!project_.settings.sdkLogPath.isEmpty()) args << QStringLiteral("--log_path") << project_.settings.sdkLogPath;
        if (clip.denoise) args << QStringLiteral("--denoise");
        if (clip.defringe) args << QStringLiteral("--defringe");
        if (clip.deflicker) args << QStringLiteral("--deflicker");
        if (clip.stitchFusion) args << QStringLiteral("--stitch_fusion");
        if (clip.coolingShellDetection) args << QStringLiteral("--cooling_shell");
        if (clip.colorPlus) args << QStringLiteral("--color_plus");
        args << QStringLiteral("--color_plus_strength") << QString::number(clip.colorPlusStrength / 100.0, 'f', 2)
             << QStringLiteral("--exposure") << QString::number(clip.sdkExposure)
             << QStringLiteral("--highlights") << QString::number(clip.sdkHighlights)
             << QStringLiteral("--shadows") << QString::number(clip.sdkShadows)
             << QStringLiteral("--contrast") << QString::number(clip.sdkContrast)
             << QStringLiteral("--brightness") << QString::number(clip.sdkBrightness)
             << QStringLiteral("--blackpoint") << QString::number(clip.sdkBlackpoint)
             << QStringLiteral("--saturation") << QString::number(clip.sdkSaturation)
             << QStringLiteral("--vibrance") << QString::number(clip.sdkVibrance)
             << QStringLiteral("--warmth") << QString::number(clip.sdkWarmth)
             << QStringLiteral("--tint") << QString::number(clip.sdkTint)
             << QStringLiteral("--definition") << QString::number(clip.sdkDefinition);
        if (project_.settings.exportStabilizationData && !image) {
            args << QStringLiteral("--stab_output")
                 << QFileInfo(outputPath_).dir().filePath(QFileInfo(clip.path).completeBaseName() + QStringLiteral(".stab"));
        }
        if (!project_.settings.aiModelPath.isEmpty()) {
            args << QStringLiteral("--model_root") << project_.settings.aiModelPath;
        }
        if (!runProcess(sdkPath, args, error)) {
            return false;
        }
        TimelineClip postProcessed = clip;
        postProcessed.denoise = false;
        postProcessed.defringe = false;
        postProcessed.deflicker = false;
        postProcessed.colorPlus = false;
        return exportWithFfmpeg(postProcessed, sdkOutput, outputPath, image, error);
    }

    if (isInsta360File(clip.path) && sdkPath.isEmpty()) {
        if (project_.settings.cuda) {
            if (error) {
                *error = QStringLiteral("GPU dipilih, tetapi Insta360 MediaSDK helper/libraries tidak ditemui. "
                                        "Build dan bundle `insta360_sdk_exporter` dengan SDK rasmi dahulu.");
            }
            return false;
        }
        emit message(QStringLiteral("SDK helper tidak ditemui; cuba decode %1 dengan FFmpeg CPU fallback.").arg(info.fileName()));
    }

    return exportWithFfmpeg(clip, clip.path, outputPath, image, error);
}

bool ExportWorker::exportWithFfmpeg(const TimelineClip &clip, const QString &sourcePath, const QString &outputPath, bool image, QString *error)
{
    QStringList args {
        QStringLiteral("-y"),
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error")
    };

    if (image) {
        args << QStringLiteral("-loop") << QStringLiteral("1")
             << QStringLiteral("-t") << QStringLiteral("5");
    }

    args << QStringLiteral("-i") << sourcePath
         << QStringLiteral("-f") << QStringLiteral("lavfi")
         << QStringLiteral("-i") << QStringLiteral("anullsrc=channel_layout=stereo:sample_rate=48000");

    if (clip.inSeconds > 0.0) {
        args << QStringLiteral("-ss") << QString::number(clip.inSeconds, 'f', 3);
    }
    if (clip.outSeconds > clip.inSeconds) {
        args << QStringLiteral("-t") << QString::number(clip.outSeconds - clip.inSeconds, 'f', 3);
    }

    bool usingGpuCodec = false;
    const QString codec = ffmpegCodecForSettings(&usingGpuCodec);
    if (usingGpuCodec) {
        emit message(QStringLiteral("Guna FFmpeg GPU encoder: %1").arg(codec));
    } else if (project_.settings.cuda) {
        emit message(QStringLiteral("GPU dipilih tetapi NVENC tidak tersedia; guna CPU encoder: %1").arg(codec));
    }
    const QString vf = buildVideoFilter(clip);
    const QString af = buildAudioFilter(clip);

    const bool useSourceAudio = clip.audioEnabled && !image && sourceHasAudio(sourcePath);

    args << QStringLiteral("-map") << QStringLiteral("0:v:0")
         << QStringLiteral("-map") << (useSourceAudio ? QStringLiteral("0:a:0") : QStringLiteral("1:a:0"))
         << QStringLiteral("-vf") << vf
         << QStringLiteral("-c:v") << codec
         << QStringLiteral("-b:v") << QStringLiteral("%1M").arg(project_.settings.bitrateMbps)
         << QStringLiteral("-pix_fmt") << (project_.settings.tenBit
                ? (usingGpuCodec ? QStringLiteral("p010le") : QStringLiteral("yuv420p10le"))
                : QStringLiteral("yuv420p"));

    if (!af.isEmpty()) {
        args << QStringLiteral("-af") << af;
    }
    args << QStringLiteral("-c:a") << QStringLiteral("aac")
         << QStringLiteral("-b:a") << QStringLiteral("192k")
         << QStringLiteral("-shortest");

    args << outputPath;

    return runProcess(QStringLiteral("ffmpeg"), args, error);
}

bool ExportWorker::ffmpegEncoderAvailable(const QString &encoder) const
{
    QProcess process;
    process.start(QStringLiteral("ffmpeg"), {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-encoders")
    });
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput())
        + QString::fromUtf8(process.readAllStandardError());
    return output.contains(encoder);
}

bool ExportWorker::sourceHasAudio(const QString &path) const
{
    QProcess process;
    process.start(QStringLiteral("ffprobe"), {
        QStringLiteral("-v"), QStringLiteral("error"),
        QStringLiteral("-select_streams"), QStringLiteral("a:0"),
        QStringLiteral("-show_entries"), QStringLiteral("stream=codec_type"),
        QStringLiteral("-of"), QStringLiteral("csv=p=0"),
        path
    });
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished();
        return false;
    }
    return QString::fromUtf8(process.readAllStandardOutput()).contains(QStringLiteral("audio"));
}

QString ExportWorker::ffmpegCodecForSettings(bool *usingGpu) const
{
    if (usingGpu) {
        *usingGpu = false;
    }

    if (project_.settings.cuda) {
        const QString gpuCodec = (project_.settings.tenBit || project_.settings.codec == QStringLiteral("h265"))
            ? QStringLiteral("hevc_nvenc")
            : QStringLiteral("h264_nvenc");
        if (ffmpegEncoderAvailable(gpuCodec)) {
            if (usingGpu) {
                *usingGpu = true;
            }
            return gpuCodec;
        }
    }

    return (project_.settings.tenBit || project_.settings.codec == QStringLiteral("h265"))
        ? QStringLiteral("libx265")
        : QStringLiteral("libx264");
}

QString ExportWorker::buildVideoFilter(const TimelineClip &clip) const
{
    QStringList filters;
    const int width = project_.settings.width;
    const int height = project_.settings.height;
    const double scale = clip.scalePercent / 100.0;

    if (project_.settings.format == ProjectFormat::StandardVideo && isInsta360File(clip.path)) {
        filters << QStringLiteral("v360=input=equirect:output=flat:yaw=%1:pitch=%2:roll=%3:h_fov=100:v_fov=75")
            .arg(clip.yawDegrees).arg(clip.pitchDegrees).arg(clip.rollDegrees);
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

    filters << QStringLiteral("fps=%1").arg(project_.settings.fps);

    if (clip.rotationDegrees != 0) {
        filters << QStringLiteral("rotate=%1*PI/180:fillcolor=black").arg(clip.rotationDegrees);
        filters << QStringLiteral("scale=%1:%2").arg(width).arg(height);
    }

    if (project_.settings.format == ProjectFormat::Panorama360
        && (clip.yawDegrees != 0 || clip.pitchDegrees != 0 || clip.rollDegrees != 0)) {
        filters << QStringLiteral("v360=input=equirect:output=equirect:yaw=%1:pitch=%2:roll=%3")
            .arg(clip.yawDegrees)
            .arg(clip.pitchDegrees)
            .arg(clip.rollDegrees);
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

QString ExportWorker::buildAudioFilter(const TimelineClip &clip) const
{
    QStringList filters;
    if (clip.volumePercent != 100) {
        filters << QStringLiteral("volume=%1").arg(clip.volumePercent / 100.0, 0, 'f', 2);
    }
    if (clip.fadeInSeconds > 0.0) {
        filters << QStringLiteral("afade=t=in:st=0:d=%1").arg(clip.fadeInSeconds, 0, 'f', 2);
    }
    if (clip.fadeOutSeconds > 0.0 && clip.outSeconds > clip.inSeconds) {
        const double duration = clip.outSeconds - clip.inSeconds;
        const double start = qMax(0.0, duration - clip.fadeOutSeconds);
        filters << QStringLiteral("afade=t=out:st=%1:d=%2")
            .arg(start, 0, 'f', 2)
            .arg(clip.fadeOutSeconds, 0, 'f', 2);
    }
    return filters.join(QStringLiteral(","));
}

bool ExportWorker::runProcess(const QString &program, const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted()) {
        if (error) {
            *error = QStringLiteral("Gagal mula proses %1: %2").arg(program, process.errorString());
        }
        return false;
    }
    while (!process.waitForFinished(200)) {
        if (cancelRequested_.load()) {
            process.terminate();
            if (!process.waitForFinished(5000)) {
                process.kill();
                process.waitForFinished();
            }
            if (error) *error = QStringLiteral("Export dibatalkan.");
            return false;
        }
    }
    const QString standardOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (!standardOutput.isEmpty()) {
        for (const QString &line : standardOutput.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            emit message(line.trimmed());
        }
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("%1 gagal: %2").arg(program, QString::fromUtf8(process.readAllStandardError()).trimmed());
        }
        return false;
    }
    return true;
}

QString ExportWorker::sdkExporterPath() const
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

QStringList ExportWorker::pairedInsta360Inputs(const QString &path) const
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
