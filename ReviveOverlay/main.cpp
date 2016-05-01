#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlContext>

int main(int argc, char *argv[])
{
	QGuiApplication a(argc, argv);

	COpenVROverlayController::SharedInstance()->Init();

	// Create a QML engine.
	QQmlEngine qmlEngine;
	qmlEngine.rootContext()->setContextProperty("ReviveManifest", CReviveManifestController::SharedInstance());

	QQmlComponent qmlComponent( &qmlEngine, QUrl("qrc:/Overlay.qml"));
	if (qmlComponent.isError())
	{
		qDebug(qUtf8Printable(qmlComponent.errorString()));
		return -1;
	}

	QObject *rootObject = qmlComponent.create();
	QQuickItem *rootItem = qobject_cast<QQuickItem*>( rootObject );

	COpenVROverlayController::SharedInstance()->SetQuickItem( rootItem );

	// don't show the window that you're going display in an overlay
	//view.show();

	return a.exec();
}
