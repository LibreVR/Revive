#ifndef OCULUSPLATFORM_H
#define OCULUSPLATFORM_H

#include <QObject>
#include <QString>
#include <QLibrary>
#include <QTimer>

typedef uint64_t ovrID;
typedef uint64_t ovrRequest;
typedef struct ovrMessage *ovrMessageHandle;
typedef struct ovrPlatformInitialize *ovrPlatformInitializeHandle;

typedef enum ovrMessageType_ {
	ovrMessage_User_GetAccessToken = 0x06A85ABE,
	ovrMessage_Platform_InitializeStandaloneOculus = 0x51F8CE0C
} ovrMessageType;

typedef enum ovrPlatformStructureType_ {
	ovrPlatformStructureType_Unknown = 0,
	ovrPlatformStructureType_OculusInitParams = 1,
} ovrPlatformStructureType;

typedef struct {
	ovrPlatformStructureType sType;
	const char *email;
	const char *password;
	ovrID appId;
	const char *uriPrefixOverride;
} ovrOculusInitParams;

typedef enum ovrPlatformInitializeResult_ {
	ovrPlatformInitialize_Success = 0,
	ovrPlatformInitialize_Uninitialized = -1,
	ovrPlatformInitialize_PreLoaded = -2,
	ovrPlatformInitialize_FileInvalid = -3,
	ovrPlatformInitialize_SignatureInvalid = -4,
	ovrPlatformInitialize_UnableToVerify = -5,
	ovrPlatformInitialize_VersionMismatch = -6,
	ovrPlatformInitialize_Unknown = -7,
	ovrPlatformInitialize_InvalidCredentials = -8,
	ovrPlatformInitialize_NotEntitled = -9,
} ovrPlatformInitializeResult;

typedef ovrRequest (*ovr_Platform_InitializeStandaloneOculus)(const ovrOculusInitParams *params);
typedef ovrRequest (*ovr_User_GetAccessToken)();
typedef ovrMessageHandle (*ovr_PopMessage)();
typedef ovrMessageType (*ovr_Message_GetType)(const ovrMessageHandle obj);
typedef const char* (*ovr_Message_GetString)(const ovrMessageHandle obj);
typedef ovrPlatformInitializeHandle(*ovr_Message_GetPlatformInitialize)(const ovrMessageHandle obj);
typedef ovrPlatformInitializeResult(*ovr_PlatformInitialize_GetResult)(const ovrPlatformInitializeHandle obj);
typedef void (*ovr_FreeMessage)(ovrMessageHandle);

class COculusPlatform : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

	Q_PROPERTY(bool connected READ Connected NOTIFY TokenChanged);
	Q_PROPERTY(QString AccessToken READ GetToken NOTIFY TokenChanged);
public:
	static COculusPlatform *SharedInstance();

	static const ovrID AppId;

public:
	COculusPlatform();
	virtual ~COculusPlatform();

	bool Init(QString basePath);
	bool Login(const QString& email, const QString& password);
	void Logout();

	bool Connected() { return !m_strAccessToken.isEmpty(); }
	QString GetToken() { return m_strAccessToken; }

signals:
	void TokenChanged();

protected slots:
	void Update();

private:
	QLibrary m_PlatformLib;
	QString m_strAccessToken;
	QTimer m_PollTimer;

	ovr_Platform_InitializeStandaloneOculus InitializeStandaloneOculus;
	ovr_User_GetAccessToken GetAccessToken;
	ovr_PopMessage PopMessage;
	ovr_Message_GetType GetType;
	ovr_Message_GetString GetString;
	ovr_Message_GetPlatformInitialize GetPlatformInitialize;
	ovr_PlatformInitialize_GetResult GetResult;
	ovr_FreeMessage FreeMessage;
};

#endif // OCULUSPLATFORM_H
