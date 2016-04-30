#include "openvroverlaycontroller.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>

int main(int argc, char *argv[])
{
	QGuiApplication a(argc, argv);

	// Create a QML engine.
	QQmlEngine *qmlEngine = new QQmlEngine;
	QQmlComponent *qmlComponent = new QQmlComponent( qmlEngine, QUrl("Overlay.qml"));

	if (qmlComponent->isError())
	{
		qDebug(qUtf8Printable(qmlComponent->errorString()));
		QGuiApplication::exit(-1);
	}

	QObject *rootObject = qmlComponent->create();
	QQuickItem *rootItem = qobject_cast<QQuickItem*>( rootObject );

	COpenVROverlayController::SharedInstance()->Init();

	COpenVROverlayController::SharedInstance()->SetQuickItem( rootItem );

	// don't show the window that you're going display in an overlay
	//view.show();

	return a.exec();
}
