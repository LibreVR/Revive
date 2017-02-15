#include "trayiconcontroller.h"
#include <windowsservices.h>
#include <qt_windows.h>
#include <winsparkle.h>

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
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
	, m_trayIcon(QIcon(":/revive_white.ico"))
{
	QObject::connect(&m_trayIcon, SIGNAL(messageClicked()), this, SLOT(messageClicked()));
}

CTrayIconController::~CTrayIconController()
{
}

bool CTrayIconController::Init()
{
	m_trayIconMenu.addAction("&Inject...", this, SLOT(inject()));
	m_trayIconMenu.addAction("&Patch...", this, SLOT(patch()));
	m_trayIconMenu.addAction("&Help", this, SLOT(showHelp()));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("Check for &updates", win_sparkle_check_update_with_ui);
	m_trayIconMenu.addAction("&Quit", this, SLOT(quit()));
	m_trayIcon.setContextMenu(&m_trayIconMenu);
	m_trayIcon.setToolTip("Revive Dashboard");
	m_trayIcon.show();

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

void CTrayIconController::quit()
{
	m_trayIcon.setVisible(false);
	QCoreApplication::quit();
}

void CTrayIconController::inject()
{
	QString file = openDialog();
	if (file.isNull())
		return;

	QStringList args;
	args.append(file);
	QProcess::execute(QCoreApplication::applicationDirPath() + "/Revive/ReviveInjector_x64.exe", args);
}

void CTrayIconController::patch()
{
	QString file = openDialog();
	if (file.isNull())
		return;

	DWORD type;
	if (!GetBinaryType(qUtf16Printable(file), &type))
		return;

	QString dir = QCoreApplication::applicationDirPath();
	QStringList files;
	if (type == SCS_32BIT_BINARY)
	{
		files.append(dir + "/Revive/x86/LibRevive32_1.dll");
		files.append(dir + "/Revive/x86/LibRevive32_1.dll");
		files.append(dir + "/Revive/x86/openvr_api.dll");
	}
	if (type == SCS_64BIT_BINARY)
	{
		files.append(dir + "/Revive/x64/LibRevive64_1.dll");
		files.append(dir + "/Revive/x64/LibRevive64_1.dll");
		files.append(dir + "/Revive/x64/openvr_api.dll");
	}
	QStringList names = { "xinput1_3.dll", "xinput9_1_0.dll", "openvr_api.dll" };

	QFileInfo info(file);
	WindowsServices::CopyFiles(files, info.absolutePath(), names);
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

QString CTrayIconController::openDialog()
{
	return QFileDialog::getOpenFileName(
				nullptr, "Revive",
				QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
				"Application (*.exe)");
}
