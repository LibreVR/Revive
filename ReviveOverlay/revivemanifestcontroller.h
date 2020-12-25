#ifndef REVIVEMANIFESTCONTROLLER_H
#define REVIVEMANIFESTCONTROLLER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QSystemTrayIcon>
#include <QMap>

class CReviveManifestController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass; 

	Q_PROPERTY(bool LibraryFound READ IsLibraryFound NOTIFY LibraryChanged)
	Q_PROPERTY(QString LibraryURL READ GetLibraryURL NOTIFY LibraryChanged)
	Q_PROPERTY(QStringList Libraries READ GetLibraries NOTIFY LibraryChanged)
	Q_PROPERTY(QStringList LibrariesURL READ GetLibrariesURL NOTIFY LibraryChanged)
	Q_PROPERTY(QString LibraryPath READ GetLibraryPath NOTIFY LibraryChanged)
	Q_PROPERTY(QString BaseURL READ GetBaseURL NOTIFY BaseChanged)
	Q_PROPERTY(QString BasePath READ GetBasePath NOTIFY BaseChanged)
public:
	static CReviveManifestController *SharedInstance();

	static const char* AppKey;
	static const char* AppPrefix;

public:
	CReviveManifestController();
	virtual ~CReviveManifestController();

	void UseLegacyRuntime(bool enabled) { m_LegacyRuntime = enabled; }

	bool Init();
	bool IsLibraryFound() { return m_bLibraryFound; }
	QString GetLibraryURL() { return m_strLibraryURL; }
	QString GetLibraryPath() { return m_strLibraryPath; }
	QString GetBaseURL() { return m_strBaseURL; }
	QString GetBasePath() { return m_strBasePath; }
	QStringList GetLibraries() { return m_lstLibraries; }
	QStringList GetLibrariesURL() { return m_lstLibrariesURL; }

	Q_INVOKABLE bool addManifest(const QString &canonicalName, const QString &manifest);
	Q_INVOKABLE bool removeManifest(const QString &canonicalName);
	Q_INVOKABLE bool launchApplication(const QString &canonicalName);
	Q_INVOKABLE bool isApplicationInstalled(const QString &canonicalName);

signals:
	void LibraryChanged();
	void BaseChanged();

private:
	bool LoadDocument();
	bool SaveDocument();
	bool GetDefaultLibraryPath(wchar_t* path, uint32_t length);
	bool GetLibraries(QStringList &id_array, QStringList & path_array);
	bool AddApplicationManifest(QFile& file);
	bool LaunchSupportApp(const QString& appKey);
	bool LaunchInjector(const QString& args);

	QFile m_appFile;
	QFile m_manifestFile;
	QFile m_supportFile;
	QDir m_appManifests;
	QJsonObject m_manifest;
	QMap<QString, QString> m_supportArgs;

	bool m_bLibraryFound;
	QString m_strLibraryURL;
	QString m_strLibraryPath;
	QString m_strBaseURL;
	QString m_strBasePath;
	QStringList m_lstLibraries;
	QStringList m_lstLibrariesURL;
	bool m_LegacyRuntime;
};

#endif // REVIVEMANIFESTCONTROLLER_H
