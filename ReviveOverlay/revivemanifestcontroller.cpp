#include "revivemanifestcontroller.h"
#include "trayiconcontroller.h"
#include "openvr.h"
#include <qt_windows.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QUrl>

CReviveManifestController *s_pSharedRevController = NULL;

CReviveManifestController *CReviveManifestController::SharedInstance()
{
	if ( !s_pSharedRevController )
	{
		s_pSharedRevController = new CReviveManifestController();
	}
	return s_pSharedRevController;
}

bool CReviveManifestController::GetDefaultLibraryPath(wchar_t* path, uint32_t length)
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

CReviveManifestController::CReviveManifestController()
	: BaseClass()
	, m_appFile(QCoreApplication::applicationDirPath() + "/app.vrmanifest")
	, m_manifestFile(QCoreApplication::applicationDirPath() + "/revive.vrmanifest")
	, m_supportFile(QCoreApplication::applicationDirPath() + "/support.vrmanifest")
	, m_bLibraryFound(false)
{
}

CReviveManifestController::~CReviveManifestController()
{
}

bool CReviveManifestController::Init()
{
	bool bSuccess = LoadDocument();

	if (!bSuccess)
	{
		QJsonArray applications;
		m_manifest["applications"] = applications;

		bSuccess = SaveDocument();
	}

	// Add support manifest
	AddApplicationManifest(m_appFile);
	AddApplicationManifest(m_supportFile);

	// Get the base path
	WCHAR path[MAX_PATH];
	if (GetDefaultLibraryPath(path, MAX_PATH))
	{
		QString library = QString::fromWCharArray(path);
		library.append(L'\\');
		qDebug("Oculus Library found: %s", qUtf8Printable(library));

		m_bLibraryFound = true;
		m_strLibraryURL = QUrl::fromLocalFile(library).url();
		m_strLibraryPath = QDir::fromNativeSeparators(library);
		emit LibraryChanged();

		if (!vr::VRApplications()->GetApplicationAutoLaunch(AppKey))
		{
			vr::EVRApplicationError error = vr::VRApplications()->SetApplicationAutoLaunch(AppKey, true);
			if (error == vr::VRApplicationError_None)
			{
				CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_AutoLaunchEnabled);
			}
			else
			{
				CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_AutoLaunchFailed);
				qWarning("Failed to set auto-launch flag (%s)", vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));
			}
		}
	}
	else
	{
		CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_OculusLibraryNotFound);
	}

	return bSuccess;
}

bool CReviveManifestController::AddApplicationManifest(QFile& file)
{
	QFileInfo info(file);
	QString filePath = QDir::toNativeSeparators(info.absoluteFilePath());
	vr::EVRApplicationError error = vr::VRApplications()->AddApplicationManifest(qPrintable(filePath));
	if (error != vr::VRApplicationError_None)
	{
		qWarning("Failed to add manifest file to OpenVR: %s (%s)", qUtf8Printable(filePath), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));
		return false;
	}

	qDebug("Loaded manifest: %s", qUtf8Printable(filePath));

	return true;
}

bool CReviveManifestController::LoadDocument()
{
	if (!m_manifestFile.open(QIODevice::ReadOnly))
	{
		qWarning("Couldn't open manifest file for reading");
		return false;
	}

	QByteArray array = m_manifestFile.readAll();
	QJsonDocument doc = QJsonDocument::fromJson(array);
	m_manifest = doc.object();
	m_manifestFile.close();

	AddApplicationManifest(m_manifestFile);

	return true;
}

bool CReviveManifestController::SaveDocument()
{
	if (!m_manifestFile.open(QIODevice::WriteOnly))
	{
		qWarning("Couldn't open manifest file for writing");
		return false;
	}

	QJsonDocument doc(m_manifest);
	QByteArray array = doc.toJson();
	m_manifestFile.write(array);
	m_manifestFile.close();

	AddApplicationManifest(m_manifestFile);

	return true;
}

bool CReviveManifestController::addManifest(const QString &canonicalName, const QString &manifest)
{
	qDebug("Adding manifest: %s", qUtf8Printable(canonicalName));
	QJsonDocument doc = QJsonDocument::fromJson(manifest.toUtf8());
	QJsonArray apps = m_manifest["applications"].toArray();
	QJsonObject obj = doc.object();
	obj["app_key"] = AppPrefix + canonicalName;
	apps.append(obj);
	m_manifest["applications"] = apps;
	return SaveDocument();
}

bool CReviveManifestController::removeManifest(const QString &canonicalName)
{
	qDebug("Removing manifest: %s", qUtf8Printable(canonicalName));
	QString appKey = AppPrefix + canonicalName;
	QJsonArray apps = m_manifest["applications"].toArray();
	for (auto it = apps.begin(); it != apps.end(); ++it)
	{
		QJsonObject obj = it->toObject();
		if (obj["app_key"] == appKey)
			apps.erase(it);
	}
	m_manifest["applications"] = apps;
	return SaveDocument();
}

bool CReviveManifestController::launchApplication(const QString &canonicalName)
{
	QString appKey = AppPrefix + canonicalName;

	// Don't attempt to launch already-running applications
	if (vr::VRApplications()->GetApplicationProcessId(qPrintable(appKey)) != 0)
		return false;

	qDebug("Launching application: %s", qUtf8Printable(canonicalName));
	vr::EVRApplicationError error = vr::VRApplications()->LaunchApplication(qPrintable(appKey));
	if (error != vr::VRApplicationError_None)
		qWarning("Failed to launch application: %s (%s)", qUtf8Printable(appKey), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));
	return error == vr::VRApplicationError_None;
}

bool CReviveManifestController::isApplicationInstalled(const QString &canonicalName)
{
	QString appKey = AppPrefix + canonicalName;
	return vr::VRApplications()->IsApplicationInstalled(qPrintable(appKey));
}
