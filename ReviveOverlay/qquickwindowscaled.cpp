#include "qquickwindowscaled.h"

QQuickWindowScaled::QQuickWindowScaled(QWindow *parent)
	: QQuickWindow(parent)
{
}


QQuickWindowScaled::QQuickWindowScaled(QQuickRenderControl *parent)
	: QQuickWindow(parent)
{
}

QQuickWindowScaled::~QQuickWindowScaled()
{
}

constexpr inline const QPointF operator*(const QPointF &a, const QPointF &b)
{
	return QPointF(a.x() * b.x(), a.y() * b.y());
}

bool QQuickWindowScaled::event(QEvent *e)
{
	// Scale any mouse event to the render target
	QMouseEvent* mouse = dynamic_cast<QMouseEvent*>(e);
	if (mouse && mouse->source() != Qt::MouseEventSynthesizedByApplication)
	{
		QSize rtSize = this->renderTargetSize();
		QPointF scalar(rtSize.width() / (float)this->width(), rtSize.height() / (float)this->height());
		QMouseEvent scaled(mouse->type(), mouse->localPos() * scalar, mouse->windowPos() * scalar, mouse->screenPos() * scalar, mouse->button(),
			mouse->buttons(), mouse->modifiers(), mouse->source());
		return QQuickWindow::event(&scaled);
	}
	return QQuickWindow::event(e);
}
