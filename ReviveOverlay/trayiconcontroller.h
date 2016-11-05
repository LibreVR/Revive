#ifndef CTRAYICONCONTROLLER_H
#define CTRAYICONCONTROLLER_H

#include <QObject>
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

protected slots:
	void messageClicked();

private:
	QSystemTrayIcon m_trayIcon;
	ETrayInfo m_LastInfo;
};

#endif // CTRAYICONCONTROLLER_H
