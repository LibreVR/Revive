#include "trayiconcontroller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QIcon>
#include <QProcess>
#include <QUrl>

CTrayIconController *s_pSharedTrayController = NULL;

CTrayIconController *CTrayIconController::SharedInstance()
{
	if ( !s_pSharedTrayController )
	{
		s_pSharedTrayController = new CTrayIconController();
	}
	return s_pSharedTrayController;
}

CTrayIconController::CTrayIconController()
	: BaseClass()
	, m_trayIcon(QIcon(":/revive.ico"))
{
	QObject::connect(&m_trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
}

CTrayIconController::~CTrayIconController()
{
}

bool CTrayIconController::Init()
{
	m_trayIcon.show();
	m_trayIconMenu.addAction("&Inject...", this, SLOT(inject()));
	m_trayIconMenu.addAction("&Help", this, SLOT(showHelp()));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Quit", QCoreApplication::quit);
	m_trayIcon.setContextMenu(&m_trayIconMenu);

	return true;
}

void CTrayIconController::ShowInformation(ETrayInfo info)
{
	m_LastInfo = info;

	switch (info)
	{
		case TrayInfo_AutoLaunchEnabled:
			m_trayIcon.showMessage("Revive succesfully installed",
								   "Revive will automatically add Oculus Store games to your library while SteamVR is running.",
								   QSystemTrayIcon::Information);
		break;
		case TrayInfo_AutoLaunchFailed:
			m_trayIcon.showMessage("Revive did not start correctly",
								   "Unable to set the auto-launch flag, please report this to the Revive issue tracker.",
								   QSystemTrayIcon::Critical);
		break;
		case TrayInfo_OculusLibraryNotFound:
			m_trayIcon.showMessage("Revive did not start correctly",
								   "No Oculus Library was found, please install the Oculus Software from oculus.com/setup.",
								   QSystemTrayIcon::Warning);
		break;
	}
}

void CTrayIconController::inject()
{
	QStringList args;
	QString file = QFileDialog::getOpenFileName(
				nullptr, "Revive Injector",
				QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
				"Application (*.exe)");
	args.append(file);
	QProcess::execute("Revive/ReviveInjector_x64.exe", args);
}

void CTrayIconController::showHelp()
{
	QDesktopServices::openUrl(QUrl("https://github.com/LibreVR/Revive/wiki"));
}

void CTrayIconController::messageClicked()
{
	switch (m_LastInfo)
	{
		case TrayInfo_AutoLaunchFailed:
			QDesktopServices::openUrl(QUrl("https://github.com/LibreVR/Revive/issues"));
		break;
		case TrayInfo_OculusLibraryNotFound:
			QDesktopServices::openUrl(QUrl("https://oculus.com/setup"));
		break;
	}
}
