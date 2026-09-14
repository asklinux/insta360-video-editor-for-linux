#pragma once

#include "Project.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSlider;
class QSpinBox;

class ProjectDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProjectDialog(QWidget *parent = nullptr, const Project *existingProject = nullptr);

    Project createProject() const;

private slots:
    void chooseProjectDirectory();
    void chooseAiModel();
    void updateResolutionForFormat();

private:
    QLineEdit *nameEdit_ = nullptr;
    QLineEdit *directoryEdit_ = nullptr;
    QComboBox *formatCombo_ = nullptr;
    QComboBox *resolutionCombo_ = nullptr;
    QComboBox *fpsCombo_ = nullptr;
    QSpinBox *bitrateSpin_ = nullptr;
    QSlider *bitrateSlider_ = nullptr;
    QComboBox *codecCombo_ = nullptr;
    QComboBox *stitchCombo_ = nullptr;
    QCheckBox *flowStateCheck_ = nullptr;
    QCheckBox *directionLockCheck_ = nullptr;
    QCheckBox *cudaCheck_ = nullptr;
    QCheckBox *softEncodeCheck_ = nullptr;
    QCheckBox *softDecodeCheck_ = nullptr;
    QCheckBox *tenBitCheck_ = nullptr;
    QCheckBox *imageProcessingCpuCheck_ = nullptr;
    QCheckBox *exportStabilizationDataCheck_ = nullptr;
    QComboBox *cameraAccessoryCombo_ = nullptr;
    QComboBox *sdkLogLevelCombo_ = nullptr;
    QLineEdit *sdkLogPathEdit_ = nullptr;
    QLineEdit *aiModelEdit_ = nullptr;
    Project baseProject_;
    bool editing_ = false;
};
