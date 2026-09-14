#include "TimelineWidget.h"

#include <QDataStream>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

TimelineWidget::TimelineWidget(QWidget *parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::DropOnly);
    setDefaultDropAction(Qt::CopyAction);
    setFlow(QListView::LeftToRight);
    setViewMode(QListView::IconMode);
    setWrapping(false);
    setResizeMode(QListView::Adjust);
    setSpacing(8);
    setMinimumHeight(170);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void TimelineWidget::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (mime->hasUrls() || mime->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void TimelineWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mime = event->mimeData();
    if (mime->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
            event->acceptProposedAction();
            return;
        }
    }

    if (mime->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
        QByteArray encoded = mime->data(QStringLiteral("application/x-qabstractitemmodeldatalist"));
        QDataStream stream(&encoded, QIODevice::ReadOnly);
        while (!stream.atEnd()) {
            int row = 0;
            int column = 0;
            QMap<int, QVariant> roleDataMap;
            stream >> row >> column >> roleDataMap;
            const QString mediaId = roleDataMap.value(Qt::UserRole + 1).toString();
            if (!mediaId.isEmpty()) {
                emit mediaDropped(mediaId);
            }
        }
        event->acceptProposedAction();
        return;
    }

    QListWidget::dropEvent(event);
}
