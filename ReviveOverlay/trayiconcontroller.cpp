#include "trayiconcontroller.h"
#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"

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
{
}

CTrayIconController::~CTrayIconController()
{
}

bool CTrayIconController::Init()
{
	m_trayIcon = std::make_unique<QSystemTrayIcon>(QIcon(":/revive_white.ico"));
	m_trayIconMenu.addAction("Enable OpenXR support", this, SLOT(openxr()))->setCheckable(true);
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Inject...", this, SLOT(inject()));
	m_trayIconMenu.addAction("&Patch...", this, SLOT(patch()));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Help", this, SLOT(showHelp()));
	m_trayIconMenu.addAction("&Open library", this, SLOT(show()));
	m_trayIconMenu.addAction("Check for &updates", win_sparkle_check_update_with_ui);
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
								   "No Oculus Library was found, please install the Oculus Software from oculus.com/setup.",
								   QSystemTrayIcon::Warning);
		break;
	}
}

void CTrayIconController::quit()
{
	if (!m_trayIcon)
		return;
	m_trayIcon->setVisible(false);
	QCoreApplication::quit();
}

void CTrayIconController::openxr(bool checked)
{
	CReviveManifestController::SharedInstance()->EnableOpenXR(checked);
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
	QMessageBox::warning(nullptr,
		"This isn't the option you're looking for",
		"Patching will lock the game to always use Revive.\n"
		"It is only needed for Oculus-only games on Steam.\n\n"
		"Do not try to use this feature for any other reason.\n"
		);

	QString file = openDialog();
	if (file.isNull())
		return;

	DWORD type;
	if (!GetBinaryType(qUtf16Printable(file), &type))
		return;

	QString dir = QCoreApplication::applicationDirPath();
	QStringList files, names = QStringList{ "xinput1_3.dll", "openvr_api.dll" };
	if (type == SCS_32BIT_BINARY)
	{
		files.append(dir + "/Revive/x86/xinput1_3.dll");
		files.append(dir + "/Revive/x86/openvr_api.dll");
		files.append(dir + "/Revive/x86/LibRevive32_1.dll");
		names.append("LibRevive32_1.dll");
		files.append(dir + "/Revive/x86/LibRemixed32_1.dll");
		names.append("LibRemixed32_1.dll");
	}
	if (type == SCS_64BIT_BINARY)
	{
		files.append(dir + "/Revive/x64/xinput1_3.dll");
		files.append(dir + "/Revive/x64/openvr_api.dll");
		files.append(dir + "/Revive/x64/LibRevive64_1.dll");
		names.append("LibRevive64_1.dll");
		files.append(dir + "/Revive/x64/LibRemixed64_1.dll");
		names.append("LibRemixed64_1.dll");
	}

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

void CTrayIconController::show()
{
	COpenVROverlayController::SharedInstance()->ShowWindow();
}

void CTrayIconController::activated(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick)
		show();
}
