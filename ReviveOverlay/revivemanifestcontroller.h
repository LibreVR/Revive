#ifndef REVIVEMANIFESTCONTROLLER_H
#define REVIVEMANIFESTCONTROLLER_H

#include <QObject>
#include <QFile>
#include <QJsonObject>

class CReviveManifestController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

public:
	static CReviveManifestController *SharedInstance();

	const char* AppKey = "revive.dashboard.overlay";

public:
	CReviveManifestController();
	virtual ~CReviveManifestController();

	Q_INVOKABLE bool addManifest(const QString &manifest);
	Q_INVOKABLE bool removeManifest(const QString &appKey);
	Q_INVOKABLE bool launchApplication(const QString &appKey);
	Q_INVOKABLE bool isApplicationInstalled(const QString &appKey);

private:
	bool LoadDocument();
	bool SaveDocument();

	QFile m_manifestFile;
	QJsonObject m_manifest;
};

#endif // REVIVEMANIFESTCONTROLLER_H
