#pragma once

#include "Project.h"

#include <QObject>
#include <QString>
#include <atomic>

class ExportWorker : public QObject {
    Q_OBJECT

public:
    ExportWorker(Project project, QString outputPath);
    void cancel();

public slots:
    void run();

signals:
    void progress(int value);
    void message(const QString &message);
    void finished(bool ok, const QString &message);

private:
    bool exportClip(const TimelineClip &clip, const QString &outputPath, QString *error);
    bool exportWithFfmpeg(const TimelineClip &clip, const QString &sourcePath, const QString &outputPath, bool image, QString *error);
    bool runProcess(const QString &program, const QStringList &arguments, QString *error);
    bool ffmpegEncoderAvailable(const QString &encoder) const;
    bool sourceHasAudio(const QString &path) const;
    QString ffmpegCodecForSettings(bool *usingGpu) const;
    QString buildVideoFilter(const TimelineClip &clip) const;
    QString buildAudioFilter(const TimelineClip &clip) const;
    QString sdkExporterPath() const;
    QStringList pairedInsta360Inputs(const QString &path) const;

    Project project_;
    QString outputPath_;
    std::atomic_bool cancelRequested_ {false};
};
