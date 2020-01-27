#include "trayiconcontroller.h"
#include "openvroverlaycontroller.h"
#include "revivemanifestcontroller.h"
#include "windowsservices.h"
#include "oculusplatform.h"
#include "logindialog.h"

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
	, m_trayIconMenu()
	, m_LastInfo()
	, m_loginDialog()
{
}

CTrayIconController::~CTrayIconController()
{
}

bool CTrayIconController::Init()
{
	m_trayIcon = std::make_unique<QSystemTrayIcon>(QIcon(":/revive_white.ico"));
	QAction* action = m_trayIconMenu.addAction("Enable OpenXR support");
	action->setCheckable(true);
	QObject::connect(action, SIGNAL(triggered(bool)), this, SLOT(openxr(bool)));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Open library", this, SLOT(show()));
	m_trayIconMenu.addAction("&Inject...", this, SLOT(inject()));
	m_trayIconMenu.addSeparator();
	m_trayIconMenu.addAction("&Link Oculus Account", this, SLOT(login()));
	m_trayIconMenu.addAction("Check for &updates", win_sparkle_check_update_with_ui);
	m_trayIconMenu.addAction("&Help", this, SLOT(showHelp()));
	m_trayIconMenu.addAction("&Quit", this, SLOT(quit()));
	m_trayIcon->setContextMenu(&m_trayIconMenu);
	m_trayIcon->setToolTip("Revive Dashboard");

	connect(m_trayIcon.get(), &QSystemTrayIcon::messageClicked, this, &CTrayIconController::messageClicked);
	connect(m_trayIcon.get(), &QSystemTrayIcon::activated, this, &CTrayIconController::activated);
	connect(&m_loginDialog, &LoginDialog::acceptLogin, this, &CTrayIconController::acceptLogin);

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
	m_trayIcon->showMessage("ReviveXR enabled",
		"OpenXR support is experimental still has known issues, use it wisely.",
		QSystemTrayIcon::Warning);

	CReviveManifestController::SharedInstance()->EnableOpenXR(checked);
}

void CTrayIconController::inject()
{
	QString file = openDialog();
	if (file.isNull())
		return;

	QStringList args;
	args.append(QDir::toNativeSeparators(file));
	QProcess::execute(QCoreApplication::applicationDirPath() + "/Revive/x64/ReviveInjector.exe", args);
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

void CTrayIconController::login()
{
	QString user, password;
	if (WindowsServices::ReadCredentials(user, password))
	{
		m_loginDialog.setUsername(user);
		m_loginDialog.setPassword(password);
	}
	m_loginDialog.show();
}

void CTrayIconController::acceptLogin(QString& username, QString& password, int& indexNumber)
{
	if (COculusPlatform::SharedInstance()->Login(username, password))
		WindowsServices::WriteCredentials(username, password);
}
