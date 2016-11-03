#ifndef REVIVEMANIFESTCONTROLLER_H
#define REVIVEMANIFESTCONTROLLER_H

#include <QObject>
#include <QFile>
#include <QJsonObject>
#include <QSystemTrayIcon>

class CReviveManifestController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

public:
	static CReviveManifestController *SharedInstance();

	const char* AppKey = "revive.dashboard.overlay";
	const char* AppPrefix = "revive.app.";

public:
	CReviveManifestController();
	virtual ~CReviveManifestController();

	bool Init();

	Q_INVOKABLE bool addManifest(const QString &canonicalName, const QString &manifest);
	Q_INVOKABLE bool removeManifest(const QString &canonicalName);
	Q_INVOKABLE bool launchApplication(const QString &canonicalName);
	Q_INVOKABLE bool isApplicationInstalled(const QString &canonicalName);

private:
	bool LoadDocument();
	bool SaveDocument();

	QSystemTrayIcon m_trayIcon;

	QFile m_manifestFile;
	QJsonObject m_manifest;
};

#endif // REVIVEMANIFESTCONTROLLER_H
