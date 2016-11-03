#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"
#include <qt_windows.h>
#include <wrl.h>
#include <Shlobj.h>

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQmlContext>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

QFile* g_LogFile = nullptr;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	QByteArray localMsg = msg.toLocal8Bit();
	QTextStream log(g_LogFile);
	log << localMsg.constData() << "\n";
	OutputDebugStringA(localMsg.constData());
	OutputDebugStringA("\n");

	if (type == QtFatalMsg)
		abort();
}

bool GetDefaultLibraryPath(PWCHAR path, DWORD length)
{
	LONG error = ERROR_SUCCESS;

	// Open the libraries key
	WCHAR keyPath[MAX_PATH] = { L"Software\\Oculus VR, LLC\\Oculus\\Libraries\\" };
	HKEY oculusKey;
	error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &oculusKey);
	if (error != ERROR_SUCCESS)
	{
		qDebug("Unable to open Libraries key.");
		return false;
	}

	// Get the default library
	WCHAR guid[40] = { L'\0' };
	DWORD guidSize = sizeof(guid);
	error = RegQueryValueExW(oculusKey, L"DefaultLibrary", NULL, NULL, (PBYTE)guid, &guidSize);
	RegCloseKey(oculusKey);
	if (error != ERROR_SUCCESS)
	{
		qDebug("Unable to read DefaultLibrary guid.");
		return false;
	}

	// Open the default library key
	wcsncat(keyPath, guid, MAX_PATH);
	error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &oculusKey);
	if (error != ERROR_SUCCESS)
	{
		qDebug("Unable to open Library path key.");
		return false;
	}

	// Get the volume path to this library
	DWORD pathSize;
	error = RegQueryValueExW(oculusKey, L"Path", NULL, NULL, NULL, &pathSize);
	PWCHAR volumePath = (PWCHAR)malloc(pathSize);
	error = RegQueryValueExW(oculusKey, L"Path", NULL, NULL, (PBYTE)volumePath, &pathSize);
	RegCloseKey(oculusKey);
	if (error != ERROR_SUCCESS)
	{
		free(volumePath);
		qDebug("Unable to read Library path.");
		return false;
	}

	// Resolve the volume path to a mount point
	DWORD total;
	WCHAR volume[50] = { L'\0' };
	wcsncpy(volume, volumePath, 49);
	GetVolumePathNamesForVolumeNameW(volume, path, length, &total);
	wcsncat(path, volumePath + 49, MAX_PATH);
	free(volumePath);

	return true;
}

bool InstallShortcut(QString shortcutPath, QString exePath)
{
	Microsoft::WRL::ComPtr<IShellLink> shellLink;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
	if (SUCCEEDED(hr))
	{
		hr = shellLink->SetPath(qUtf16Printable(exePath));
		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IPersistFile> persistFile;
			hr = shellLink.As(&persistFile);
			if (SUCCEEDED(hr))
			{
				hr = persistFile->Save(qUtf16Printable(shortcutPath), TRUE);
			}
		}
	}
	return SUCCEEDED(hr);
}

bool RegisterAppForNotificationSupport()
{
	// In order to display toasts, a desktop application must have a shortcut on the Start menu.
	QString appData = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
	if (appData.isEmpty())
		return false;

	QString shortcutPath = appData + "/Revive Dashboard.lnk";
	QFileInfo attributes(shortcutPath);
	if (!attributes.exists())
	{
		QString exePath = QCoreApplication::applicationFilePath();
		return InstallShortcut(QDir::toNativeSeparators(shortcutPath), QDir::toNativeSeparators(exePath));
	}

	return true;
}

int main(int argc, char *argv[])
{
	// Open the log file and install our handler.
	QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	if (QDir().mkpath(logPath + "/Revive")) {
		g_LogFile = new QFile(logPath + "/Revive/ReviveOverlay.log");
		g_LogFile->open(QIODevice::WriteOnly | QIODevice::Truncate);
	}
	qInstallMessageHandler(myMessageOutput);

	QGuiApplication a(argc, argv);

	if (!RegisterAppForNotificationSupport())
		qWarning("Failed to register for notification support.");

	// Handle command-line arguments
	if (a.arguments().contains("-manifest")) {
		// Only initialize the manifest
		vr::EVRInitError err = vr::VRInitError_None;
		vr::IVRSystem *pVRSystem = vr::VR_Init( &err, vr::VRApplication_Utility );

		if ( err != vr::VRInitError_None )
			return -1;

		if (!CReviveManifestController::SharedInstance()->Init())
			return -1;

		vr::VR_Shutdown();
		return 0;
	}

	// Initialize singletons
	if (!COpenVROverlayController::SharedInstance()->Init())
		return -1;
	if (!CReviveManifestController::SharedInstance()->Init())
		return -1;

	// Get the base path
	QString str;
	WCHAR path[MAX_PATH];
	if (GetDefaultLibraryPath(path, MAX_PATH) && false)
	{
		QString str = QString::fromWCharArray(path);
		str.append(L'\\');
	}

	// Create a QML engine.
	QQmlEngine qmlEngine;
	qmlEngine.rootContext()->setContextProperty("ReviveManifest", CReviveManifestController::SharedInstance());
	qmlEngine.rootContext()->setContextProperty("OpenVR", COpenVROverlayController::SharedInstance());

	// Set the properties.
	QUrl runtime = QUrl::fromLocalFile(vr::VR_RuntimePath());
	QUrl url = QUrl::fromLocalFile(str);
	QString base = QDir::fromNativeSeparators(str);
	qmlEngine.rootContext()->setContextProperty("openvrURL", runtime.url());
	qmlEngine.rootContext()->setContextProperty("baseURL", url.url());
	qmlEngine.rootContext()->setContextProperty("basePath", base);

	qDebug("Runtime directory: %s", qUtf8Printable(runtime.url()));
	qDebug("Oculus directory: %s", qUtf8Printable(url.url()));

	QQmlComponent qmlComponent( &qmlEngine, QUrl("qrc:/Overlay.qml"));
	if (qmlComponent.isError())
	{
		qDebug(qUtf8Printable(qmlComponent.errorString()));
		return -1;
	}

	QObject *rootObject = qmlComponent.create();
	QQuickItem *rootItem = qobject_cast<QQuickItem*>( rootObject );

	COpenVROverlayController::SharedInstance()->SetQuickItem( rootItem );

	return a.exec();
}
