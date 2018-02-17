#pragma once

#include <QQuickWindow>

class QQuickWindowScaled : public QQuickWindow
{
	Q_OBJECT

public:
	explicit QQuickWindowScaled(QWindow *parent = Q_NULLPTR);
	explicit QQuickWindowScaled(QQuickRenderControl *renderControl);
	~QQuickWindowScaled();

protected:
	virtual bool event(QEvent *e);
};
