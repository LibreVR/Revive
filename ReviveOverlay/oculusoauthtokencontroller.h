#ifndef OCULUSOAUTHTOKENCONTROLLER_H
#define OCULUSOAUTHTOKENCONTROLLER_H

#include <QObject>
#include <QString>
#include <QLibrary>
#include <QTimer>
#include <QFile>
#include <QtSql>

typedef struct {
} ovrOculusInitParams;

class COculusOauthTokenController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

	Q_PROPERTY(bool connected READ Connected NOTIFY TokenChanged);
	Q_PROPERTY(QString AccessToken READ GetToken NOTIFY TokenChanged);
public:
	static COculusOauthTokenController *SharedInstance();

public:
	COculusOauthTokenController();
	virtual ~COculusOauthTokenController();

	bool Init();
	bool LoadToken();

	bool Connected() { return !m_strAccessToken.isEmpty(); }
	QString GetToken() { return m_strAccessToken; }

signals:
	void TokenChanged();

protected slots:

private:
	QString m_strAccessToken;
	QDir m_roamingAppDataPath;
	QSqlDatabase m_sqliteDb;
};

#endif // OCULUSOAUTHTOKENCONTROLLER_H
