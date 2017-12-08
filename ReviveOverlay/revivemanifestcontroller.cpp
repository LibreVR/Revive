#include "revivemanifestcontroller.h"
#include "trayiconcontroller.h"
#include "openvr.h"
#include "OVR_CAPI_Keys.h"
#include "Settings.h"
#include <qt_windows.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
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
	, m_defaultsFile(QString(vr::VR_RuntimePath()) + "/resources/settings/default.vrsettings")
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

	// Attempt to set the Revive defaults in the runtime.
	if (!SetDefaults())
		qDebug("Failed to set runtime default values, Revive will fall back to internal defaults");

	// Add support manifest
	AddApplicationManifest(m_appFile);
	AddApplicationManifest(m_supportFile);

	// Ensure the auto-launch flag is set
	if (vr::VRApplications()->SetApplicationAutoLaunch(AppKey, true) != vr::VRApplicationError_None)
		CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_AutoLaunchFailed);

	// Get the base path
	WCHAR path[MAX_PATH];
	if (GetDefaultLibraryPath(path, MAX_PATH))
	{
		QString library = QString::fromWCharArray(path);
		if (!library.endsWith('\\'))
			library.append('\\');
		qDebug("Oculus Library found: %s", qUtf8Printable(library));

		m_bLibraryFound = true;
		m_strLibraryURL = QUrl::fromLocalFile(library).url();
		m_strLibraryPath = QDir::fromNativeSeparators(library);
		emit LibraryChanged();

		if (!QCoreApplication::arguments().contains("-compositor"))
		{
			if (vr::VRApplications()->IsApplicationInstalled(AppKey) && vr::VRApplications()->GetApplicationAutoLaunch(AppKey))
				CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_AutoLaunchEnabled);
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

	QJsonDocument doc = QJsonDocument::fromJson(m_manifestFile.readAll());
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
	m_manifestFile.write(doc.toJson());
	m_manifestFile.close();

	AddApplicationManifest(m_manifestFile);

	return true;
}

bool CReviveManifestController::SetDefaults()
{
	if (!m_defaultsFile.open(QIODevice::ReadOnly))
	{
		qWarning("Couldn't open defaults file for reading");
		return false;
	}

	QJsonDocument doc = QJsonDocument::fromJson(m_defaultsFile.readAll());
	QJsonObject defaults = doc.object();
	m_defaultsFile.close();

	QJsonObject revive;

	// Set the Oculus keys that have defaults
	revive[OVR_KEY_GENDER] = OVR_DEFAULT_GENDER;
	revive[OVR_KEY_PLAYER_HEIGHT] = REV_ROUND(OVR_DEFAULT_PLAYER_HEIGHT);
	revive[OVR_KEY_EYE_HEIGHT] = REV_ROUND(OVR_DEFAULT_EYE_HEIGHT);
	revive[OVR_KEY_NECK_TO_EYE_DISTANCE "[0]"] = REV_ROUND(OVR_DEFAULT_NECK_TO_EYE_HORIZONTAL);
	revive[OVR_KEY_NECK_TO_EYE_DISTANCE "[1]"] = REV_ROUND(OVR_DEFAULT_NECK_TO_EYE_VERTICAL);

	// Set the defaults for Revive keys
	revive[REV_KEY_DEFAULT_ORIGIN] = REV_DEFAULT_ORIGIN;
	revive[REV_KEY_PIXELS_PER_DISPLAY] = REV_ROUND(REV_DEFAULT_PIXELS_PER_DISPLAY);
	revive[REV_KEY_THUMB_DEADZONE] = REV_ROUND(REV_DEFAULT_THUMB_DEADZONE);
	revive[REV_KEY_THUMB_SENSITIVITY] = REV_ROUND(REV_DEFAULT_THUMB_SENSITIVITY);
	revive[REV_KEY_TOGGLE_GRIP] = REV_DEFAULT_TOGGLE_GRIP;
	revive[REV_KEY_TRIGGER_GRIP] = REV_DEFAULT_TRIGGER_GRIP;
	revive[REV_KEY_TOGGLE_DELAY] = REV_DEFAULT_TOGGLE_DELAY;
	revive[REV_KEY_TOUCH_PITCH] = REV_ROUND(REV_DEFAULT_TOUCH_PITCH);
	revive[REV_KEY_TOUCH_YAW] = REV_ROUND(REV_DEFAULT_TOUCH_YAW);
	revive[REV_KEY_TOUCH_ROLL] = REV_ROUND(REV_DEFAULT_TOUCH_ROLL);
	revive[REV_KEY_TOUCH_X] = REV_ROUND(REV_DEFAULT_TOUCH_X);
	revive[REV_KEY_TOUCH_Y] = REV_ROUND(REV_DEFAULT_TOUCH_Y);
	revive[REV_KEY_TOUCH_Z] = REV_ROUND(REV_DEFAULT_TOUCH_Z);
	revive[REV_KEY_IGNORE_ACTIVITYLEVEL] = REV_DEFAULT_IGNORE_ACTIVITYLEVEL;
	revive[REV_KEY_INPUT_SCRIPT] = REV_DEFAULT_INPUT_SCRIPT;

	defaults[REV_SETTINGS_SECTION] = revive;
	doc.setObject(defaults);

	if (!m_defaultsFile.open(QIODevice::WriteOnly))
	{
		qWarning("Couldn't open defaults file for writing");
		return false;
	}

	m_defaultsFile.write(doc.toJson());
	m_defaultsFile.close();

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
		{
			apps.erase(it);
			break;
		}
	}
	m_manifest["applications"] = apps;
	return SaveDocument();
}

bool CReviveManifestController::launchApplication(const QString &canonicalName)
{
	qDebug("Launching application: %s", qUtf8Printable(canonicalName));
	QString appKey = AppPrefix + canonicalName;

	vr::EVRApplicationError error = vr::VRApplications()->LaunchApplication(qPrintable(appKey));
	if (error != vr::VRApplicationError_None)
	{
		qWarning("Failed to launch application, falling back to injector: %s (%s)", qUtf8Printable(appKey), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));

		if (!vr::VRApplications()->IsApplicationInstalled(qPrintable(appKey)))
			return false;

		// Allocate a buffer large enough to store the string
		uint32_t size = vr::VRApplications()->GetApplicationPropertyString(qPrintable(appKey), vr::VRApplicationProperty_Arguments_String, nullptr, 0);
		QByteArray args(size, '\0');

		// Get the arguments string for the injector
		vr::EVRApplicationError error;
		vr::VRApplications()->GetApplicationPropertyString(qPrintable(appKey), vr::VRApplicationProperty_Arguments_String, args.data(), size, &error);
		if (error != vr::VRApplicationError_None)
			return false;

		// Launch the injector with the arguments
		QProcess injector;
		injector.setProgram(QCoreApplication::applicationDirPath() + "/Revive/ReviveInjector_x64.exe");
		injector.setNativeArguments(args);
		injector.start();

		if (!injector.waitForFinished())
			return false;
		return injector.exitCode() == 0;
	}

	return true;
}

bool CReviveManifestController::isApplicationInstalled(const QString &canonicalName)
{
	QString appKey = AppPrefix + canonicalName;
	return vr::VRApplications()->IsApplicationInstalled(qPrintable(appKey));
}
