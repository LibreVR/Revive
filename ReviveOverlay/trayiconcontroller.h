#ifndef CTRAYICONCONTROLLER_H
#define CTRAYICONCONTROLLER_H

#include <QObject>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QString>
#include <memory>

enum ETrayInfo
{
	TrayInfo_OculusLibraryNotFound,
	TrayInfo_AutoLaunchEnabled,
	TrayInfo_AutoLaunchFailed,
	TrayInfo_OculusAccessTokenNotFound,
};

class CTrayIconController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

public:
	static CTrayIconController *SharedInstance();

public:
	CTrayIconController();
	~CTrayIconController();

	bool Init();
	void ShowInformation(ETrayInfo info);

public slots:
	void quit();

protected slots:
	void openxr(bool checked);
	void inject();
	void show();
	void showHelp();
	void messageClicked();
	void activated(QSystemTrayIcon::ActivationReason reason);

private:
	std::unique_ptr<QSystemTrayIcon> m_trayIcon;
	QMenu m_trayIconMenu;
	ETrayInfo m_LastInfo;

	QString openDialog();
};

#endif // CTRAYICONCONTROLLER_H
