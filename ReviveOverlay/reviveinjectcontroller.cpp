#include "reviveinjectcontroller.h"

#include <Windows.h>
#include <QUrl>

CReviveInjectController *s_pSharedRevController = NULL;

CReviveInjectController *CReviveInjectController::SharedInstance()
{
	if ( !s_pSharedRevController )
	{
		s_pSharedRevController = new CReviveInjectController();
	}
	return s_pSharedRevController;
}

CReviveInjectController::CReviveInjectController()
	: BaseClass()
{
}

CReviveInjectController::~CReviveInjectController()
{
}

int CReviveInjectController::CreateProcessAndInject(const QUrl &programPath)
{
	return 0;
}
