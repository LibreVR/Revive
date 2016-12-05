#ifndef CTRAYICONCONTROLLER_H
#define CTRAYICONCONTROLLER_H

#include <QObject>
#include <QMenu>
#include <QSystemTrayIcon>

enum ETrayInfo
{
	TrayInfo_OculusLibraryNotFound,
	TrayInfo_AutoLaunchEnabled,
	TrayInfo_AutoLaunchFailed
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
	void inject();
	void patch();
	void showHelp();
	void messageClicked();

private:
	QSystemTrayIcon m_trayIcon;
	QMenu m_trayIconMenu;
	ETrayInfo m_LastInfo;

	QString openDialog();
};

#endif // CTRAYICONCONTROLLER_H
