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

	Q_INVOKABLE int addManifest(const QString &canonicalName, const QString &binaryPath, const QString &arguments, const QString &imagePath, const QString &strings);
	Q_INVOKABLE int removeManifest(const QString &canonicalName);
	Q_INVOKABLE int launchApplication(const QString &canonicalName);
	Q_INVOKABLE bool isApplicationInstalled(const QString &canonicalName);

private:
	bool LoadDocument();
	bool SaveDocument();

	QFile m_manifestFile;
	QJsonObject m_manifest;
};

#endif // REVIVEMANIFESTCONTROLLER_H
