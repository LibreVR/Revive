#include "revivemanifestcontroller.h"

#include <Windows.h>
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

CReviveManifestController::CReviveManifestController()
	: BaseClass()
{
}

CReviveManifestController::~CReviveManifestController()
{
}

int CReviveManifestController::addManifest(const QString &canonicalName, const QString &binaryPath, const QString &arguments, const QString &imagePath, const QString &strings)
{
	return 0;
}

int CReviveManifestController::removeManifest(const QString &canonicalName)
{
	return 0;
}

int CReviveManifestController::launchApplication(const QString &canonicalName)
{
	return 0;
}

bool CReviveManifestController::isApplicationInstalled(const QString &canonicalName)
{
	return false;
}
