#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QThread;

class LiveCameraDialog : public QDialog {
    Q_OBJECT

public:
    explicit LiveCameraDialog(QWidget *parent = nullptr);
    ~LiveCameraDialog() override;

signals:
    void startRequested(int width, int height, int stitchType, int accessoryType,
                        int delayMs, int bitrate, bool flowState, bool directionLock,
                        bool defringe, bool deflicker, bool softDecode);
    void stopRequested();

private:
    QLabel *preview_ = nullptr;
    QLabel *status_ = nullptr;
    QComboBox *resolution_ = nullptr;
    QComboBox *stitchType_ = nullptr;
    QComboBox *accessory_ = nullptr;
    QSpinBox *delay_ = nullptr;
    QSpinBox *bitrate_ = nullptr;
    QCheckBox *flowState_ = nullptr;
    QCheckBox *directionLock_ = nullptr;
    QCheckBox *defringe_ = nullptr;
    QCheckBox *deflicker_ = nullptr;
    QCheckBox *softDecode_ = nullptr;
    QPushButton *start_ = nullptr;
    QPushButton *stop_ = nullptr;
    QThread *workerThread_ = nullptr;
    QObject *controller_ = nullptr;
};
