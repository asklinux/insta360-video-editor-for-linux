#pragma once

#include <QListWidget>

class TimelineWidget : public QListWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget *parent = nullptr);

signals:
    void mediaDropped(const QString &mediaId);
    void filesDropped(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
};
