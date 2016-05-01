#ifndef REVIVEMANIFESTCONTROLLER_H
#define REVIVEMANIFESTCONTROLLER_H

#include <QObject>

class CReviveManifestController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

public:
	static CReviveManifestController *SharedInstance();

public:
	CReviveManifestController();
	virtual ~CReviveManifestController();

	Q_INVOKABLE int addManifest(const QString &canonicalName, const QString &binaryPath, const QString &arguments, const QString &imagePath, const QString &strings);
	Q_INVOKABLE int removeManifest(const QString &canonicalName);
	Q_INVOKABLE int launchApplication(const QString &canonicalName);
	Q_INVOKABLE bool isApplicationInstalled(const QString &canonicalName);
};

#endif // REVIVEMANIFESTCONTROLLER_H
