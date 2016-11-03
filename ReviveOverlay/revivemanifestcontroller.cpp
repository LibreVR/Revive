#include "revivemanifestcontroller.h"
#include "openvr.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

CReviveManifestController *s_pSharedRevController = NULL;

CReviveManifestController *CReviveManifestController::SharedInstance()
{
	if ( !s_pSharedRevController )
	{
		s_pSharedRevController = new CReviveManifestController();
	}
	return s_pSharedRevController;
}

CReviveManifestController::CReviveManifestController()
	: BaseClass()
	, m_manifestFile(QCoreApplication::applicationDirPath() + "/revive.vrmanifest")
	, m_trayIcon(QIcon(":/revive.ico"))
{
	m_trayIcon.show();
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

	if (!vr::VRApplications()->GetApplicationAutoLaunch(AppKey))
	{
		vr::EVRApplicationError error = vr::VRApplications()->SetApplicationAutoLaunch(AppKey, true);
		if (error == vr::VRApplicationError_None)
		{
			m_trayIcon.showMessage("Revive Dashboard",
								   "Revive has been set to auto-launch and will automatically add Oculus Store games to your library.");
		}
		else
		{
			m_trayIcon.showMessage("Revive Dashboard",
								   "Unable to set the auto-launch flag, please report this to the Revive issue tracker.");
			qWarning("Failed to set auto-launch flag (%s)", vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));
		}
	}

	return bSuccess;
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

	QFileInfo info(m_manifestFile);
	QString filePath = QDir::toNativeSeparators(info.absoluteFilePath());
	vr::EVRApplicationError error = vr::VRApplications()->AddApplicationManifest(filePath.toUtf8());
	if (error != vr::VRApplicationError_None)
		qWarning("Failed to add manifest file to OpenVR: %s (%s)", qUtf8Printable(filePath), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));

	qDebug("Loaded manifest from: %s", qUtf8Printable(filePath));

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

	QFileInfo info(m_manifestFile);
	QString filePath = QDir::toNativeSeparators(info.absoluteFilePath());
	vr::EVRApplicationError error = vr::VRApplications()->AddApplicationManifest(filePath.toUtf8());
	if (error != vr::VRApplicationError_None)
		qWarning("Failed to add manifest file to OpenVR: %s (%s)", qUtf8Printable(filePath), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));

	qDebug("Saved manifest to: %s", qUtf8Printable(filePath));

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
	qDebug("Launching application: %s", qUtf8Printable(canonicalName));
	QString appKey = AppPrefix + canonicalName;
	vr::EVRApplicationError error = vr::VRApplications()->LaunchApplication(appKey.toUtf8());
	if (error != vr::VRApplicationError_None)
		qWarning("Failed to launch application: %s (%s)", qUtf8Printable(appKey), vr::VRApplications()->GetApplicationsErrorNameFromEnum(error));
	return error == vr::VRApplicationError_None;
}

bool CReviveManifestController::isApplicationInstalled(const QString &canonicalName)
{
	QString appKey = AppPrefix + canonicalName;
	std::string key = appKey.toStdString();
	return vr::VRApplications()->IsApplicationInstalled(key.c_str());
}
