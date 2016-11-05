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

	Q_PROPERTY(QString LibraryURL READ GetLibraryURL NOTIFY LibraryChanged)
	Q_PROPERTY(QString LibraryPath READ GetLibraryPath NOTIFY LibraryChanged)
public:
	static CReviveManifestController *SharedInstance();

	const char* AppKey = "revive.dashboard.overlay";
	const char* AppPrefix = "revive.app.";

public:
	CReviveManifestController();
	virtual ~CReviveManifestController();

	bool Init();
	QString GetLibraryURL() { return m_strLibraryURL; }
	QString GetLibraryPath() { return m_strLibraryPath; }

	Q_INVOKABLE bool addManifest(const QString &canonicalName, const QString &manifest);
	Q_INVOKABLE bool removeManifest(const QString &canonicalName);
	Q_INVOKABLE bool launchApplication(const QString &canonicalName);
	Q_INVOKABLE bool isApplicationInstalled(const QString &canonicalName);

signals:
	void LibraryChanged();

private:
	bool LoadDocument();
	bool SaveDocument();
	bool GetDefaultLibraryPath(wchar_t* path, uint32_t length);

	QFile m_manifestFile;
	QJsonObject m_manifest;
	QString m_strLibraryPath;
	QString m_strLibraryURL;
};

#endif // REVIVEMANIFESTCONTROLLER_H
