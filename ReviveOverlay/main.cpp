#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"
#include <qt_windows.h>

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlContext>
#include <QUrl>
#include <QDir>

int main(int argc, char *argv[])
{
	QGuiApplication a(argc, argv);

	COpenVROverlayController::SharedInstance()->Init();

	// Get the base path
	DWORD size = MAX_PATH;
	WCHAR path[MAX_PATH];
	HKEY oculusKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
	RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &size);

	// Create a QML engine.
	QQmlEngine qmlEngine;
	qmlEngine.rootContext()->setContextProperty("ReviveManifest", CReviveManifestController::SharedInstance());

	// Set the properties.
	QString str = QString::fromWCharArray(path);
	QUrl url = QUrl::fromLocalFile(str);
	QString base = QDir::fromNativeSeparators(str);
	QUrl runtime = QUrl::fromLocalFile(vr::VR_RuntimePath());
	qmlEngine.rootContext()->setContextProperty("openvrURL", runtime.url());
	qmlEngine.rootContext()->setContextProperty("baseURL", url.url());
	qmlEngine.rootContext()->setContextProperty("basePath", base);

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
