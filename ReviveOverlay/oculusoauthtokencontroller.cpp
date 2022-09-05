#include "oculusoauthtokencontroller.h"
#include "trayiconcontroller.h"
#include "windowsservices.h"
#include <stdlib.h>

#include <QCoreApplication>

COculusOauthTokenController *COculusOauthTokenController::SharedInstance()
{
	static COculusOauthTokenController *instance = nullptr;
	if ( !instance )
	{
		instance = new COculusOauthTokenController();
	}
	return instance;
}

COculusOauthTokenController::COculusOauthTokenController()
	: m_strAccessToken()
	, m_roamingAppDataPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
{
}

COculusOauthTokenController::~COculusOauthTokenController()
{
}

bool COculusOauthTokenController::Init()
{
	//Clean up saved credentials from previous versions as they're no longer needed
	WindowsServices::DeleteCredentials();
	//cd up from %APPDATA%/Roaming/APPNAME to %APPDATA%/Roaming
	m_roamingAppDataPath.cdUp();

	if(LoadToken())
	{
		return true;
	}

	CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_OculusAccessTokenNotFound);
	return false;
}

bool COculusOauthTokenController::LoadToken()
{
	QString oculusOfflineDb = m_roamingAppDataPath.filePath("Oculus/Sessions/_oaf/data.sqlite");
	QString tempOculusOfflineDB = m_roamingAppDataPath.filePath("Oculus/Sessions/_oaf/revive_data.sqlite");

	if (!QFile::exists(oculusOfflineDb))
		return false;

	//Check for temp db that somehow missed cleanup
	if (QFile::exists(tempOculusOfflineDB))
		QFile::remove(tempOculusOfflineDB);

	//The SQLite DB is locked by the oculus service, so we need to make a copy
	QFile::copy(oculusOfflineDb, tempOculusOfflineDB);
	QSqlDatabase sqliteDb = QSqlDatabase::addDatabase( "QSQLITE" );
	sqliteDb.setDatabaseName(tempOculusOfflineDB);

	if(!sqliteDb.open())
	{
		QFile::remove(tempOculusOfflineDB);
		return false;
	}

	QSqlQuery query = QSqlQuery( sqliteDb );
	if( !query.exec( "SELECT value FROM 'Objects' WHERE hashkey = '__OAF_OFFLINE_DATA_KEY__'" ))
	{
		QFile::remove(tempOculusOfflineDB);
		return false;
	}
	query.first();

	QByteArray offlineDataByteArray = query.value(0).toByteArray();
	if (!offlineDataByteArray.contains("last_valid_auth_token"))
	{
		QFile::remove(tempOculusOfflineDB);
		 return false;
	}
	qsizetype token_start_index = offlineDataByteArray.indexOf("last_valid_auth_token") + 31;

	qsizetype token_stop_index = -1;
	if(offlineDataByteArray.contains("last_valid_fb_access_token"))
	{
		token_stop_index = offlineDataByteArray.indexOf("last_valid_fb_access_token") - 4;
	}

	if ( offlineDataByteArray.mid(token_start_index).contains("last_valid_auth_token_type"))
	{
		/*'last_valid_auth_token_type' matches 'last_valid_auth_token' in indexOf, so
		search after the index of 'last_valid_auth_token' */
		token_stop_index = offlineDataByteArray.mid(token_start_index).indexOf("last_valid_auth_token_type") - 4;
	}

	if (token_stop_index == -1)
	{
		return false;
	}

	m_strAccessToken = QString::fromStdString(offlineDataByteArray.mid(token_start_index, token_stop_index).toStdString());
	TokenChanged();
	qDebug("Oculus Oauth token: %s", qUtf8Printable(m_strAccessToken));
	QFile::remove(tempOculusOfflineDB);
	return true;
}
