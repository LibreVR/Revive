#include "trayiconcontroller.h"
#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"
#include "windowsservices.h"
#include "oculusoauthtokencontroller.h"

#include <qt_windows.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QProcess>
#include <QUrl>
#include <QMessageBox>

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
	, m_trayIcon()
	, m_trayIconMenu()
	, m_LastInfo()
{
}

CTrayIconController::~CTrayIconController()
{
}

bool CTrayIconController::Init()
{
	m_trayIcon = std::make_unique<QSystemTrayIcon>(QIcon(":/revive_white.ico"));
	QAction* action = m_trayIconMenu.addAction("Use OpenXR runtime");
	action->setCheckable(true);
	QObject::connect(action, SIGNAL(triggered(bool)), this, SLOT(openxr(bool)));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Open library", this, SLOT(show()));
	m_trayIconMenu.addAction("&Inject...", this, SLOT(inject()));
	m_trayIconMenu.addAction("&Shortcut...", this, SLOT(shortcut()));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Help", this, SLOT(showHelp()));
	m_trayIconMenu.addAction("&Quit", this, SLOT(quit()));
	m_trayIcon->setContextMenu(&m_trayIconMenu);
	m_trayIcon->setToolTip("Revive Dashboard");

	connect(m_trayIcon.get(), &QSystemTrayIcon::messageClicked, this, &CTrayIconController::messageClicked);
	connect(m_trayIcon.get(), &QSystemTrayIcon::activated, this, &CTrayIconController::activated);

	m_trayIcon->show();
	return true;
}

void CTrayIconController::ShowInformation(ETrayInfo info)
{
	m_LastInfo = info;
	if (!m_trayIcon)
		return;

	switch (info)
	{
		case TrayInfo_AutoLaunchEnabled:
			m_trayIcon->showMessage("Revive succesfully installed",
								   "Revive will automatically add Oculus Store games to your library while SteamVR is running.",
								   QSystemTrayIcon::Information);
		break;
		case TrayInfo_AutoLaunchFailed:
			m_trayIcon->showMessage("Revive did not start correctly",
								   "Unable to set the auto-launch flag, please report this to the Revive issue tracker.",
								   QSystemTrayIcon::Critical);
		break;
		case TrayInfo_OculusLibraryNotFound:
			m_trayIcon->showMessage("Revive did not start correctly",
								   "No Oculus Library was found, click here to install the Oculus Software from oculus.com/setup.",
								   QSystemTrayIcon::Warning);
		break;
		case TrayInfo_OculusAccessTokenNotFound:
			m_trayIcon->showMessage("Unable to load Oculus OAuth token",
								   "Multiplayer may have issues! Sign out & back in to the Oculus app, then reboot your PC or restart Revive & OVRService.",
								   QSystemTrayIcon::Warning);
		break;
	}
}

void CTrayIconController::quit()
{
	m_trayIcon.reset();
	QCoreApplication::quit();
}

void CTrayIconController::openxr(bool checked)
{
	CReviveManifestController::SharedInstance()->UseOpenXR(checked);
}

void CTrayIconController::inject()
{
	QString file = openDialog();
	if (file.isNull())
		return;

	CReviveManifestController::SharedInstance()->LaunchInjector(QDir::toNativeSeparators(file));
}

void CTrayIconController::shortcut()
{
	QString file = openDialog();
	if (file.isNull())
		return;

	QString args = file;
	if (CReviveManifestController::SharedInstance()->UsingOpenXR())
		args.prepend("/openxr ");

	if (!WindowsServices::CreateShortcut(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/" + QFileInfo(file).baseName() + ".lnk",
									QCoreApplication::applicationDirPath() + "/ReviveInjector.exe", args, file))
		return;

	m_trayIcon->showMessage("Shortcut created on desktop",
						   "You can use this shortcut to launch the executable with Revive directly.",
						   QSystemTrayIcon::Information);
}

void CTrayIconController::showHelp()
{
	QDesktopServices::openUrl(QUrl("https://github.com/LibreVR/Revive/wiki"));
}

void CTrayIconController::messageClicked()
{
	switch (m_LastInfo)
	{
		case TrayInfo_AutoLaunchEnabled:
			show();
		break;
		case TrayInfo_AutoLaunchFailed:
			QDesktopServices::openUrl(QUrl("https://github.com/LibreVR/Revive/issues"));
		break;
		case TrayInfo_OculusLibraryNotFound:
			QDesktopServices::openUrl(QUrl("https://oculus.com/setup"));
		break;
	}
}

QString CTrayIconController::openDialog()
{
	return QFileDialog::getOpenFileName(
				nullptr, "Revive",
				QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
				"Application (*.exe)");
}

void CTrayIconController::show()
{
	COpenVROverlayController::SharedInstance()->ShowWindow();
}

void CTrayIconController::activated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick)
		show();
}
