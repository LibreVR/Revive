#include "oculusplatform.h"
#include "windowsservices.h"
#include "trayiconcontroller.h"

#include <QCoreApplication>

const ovrID COculusPlatform::AppId = 572698886195608; // Oculus Runtime

COculusPlatform *COculusPlatform::SharedInstance()
{
	static COculusPlatform *instance = nullptr;
	if ( !instance )
	{
		instance = new COculusPlatform();
	}
	return instance;
}

COculusPlatform::COculusPlatform()
	: m_PlatformLib(this)
	, m_strAccessToken()
	, InitializeStandaloneOculus()
	, GetAccessToken()
	, PopMessage()
	, GetType()
	, GetString()
	, FreeMessage()
{
	connect(&m_PollTimer, &QTimer::timeout, this, QOverload<>::of(&COculusPlatform::Update));
}

COculusPlatform::~COculusPlatform()
{
	m_PlatformLib.unload();
}

bool COculusPlatform::Init(QString basePath)
{
	m_PlatformLib.setFileName(basePath + "/Support/oculus-runtime/LibOVRPlatform64_1");
	if (m_PlatformLib.load())
	{
		InitializeStandaloneOculus = (ovr_Platform_InitializeStandaloneOculus)m_PlatformLib.resolve("ovr_Platform_InitializeStandaloneOculus");
		GetAccessToken = (ovr_User_GetAccessToken)m_PlatformLib.resolve("ovr_User_GetAccessToken");
		PopMessage = (ovr_PopMessage)m_PlatformLib.resolve("ovr_PopMessage");
		GetType = (ovr_Message_GetType)m_PlatformLib.resolve("ovr_Message_GetType");
		GetString = (ovr_Message_GetString)m_PlatformLib.resolve("ovr_Message_GetString");
		GetPlatformInitialize = (ovr_Message_GetPlatformInitialize)m_PlatformLib.resolve("ovr_Message_GetPlatformInitialize");
		GetResult = (ovr_PlatformInitialize_GetResult)m_PlatformLib.resolve("ovr_PlatformInitialize_GetResult");
		FreeMessage = (ovr_FreeMessage)m_PlatformLib.resolve("ovr_FreeMessage");

		QString email, password;
		if (WindowsServices::ReadCredentials(email, password))
			Login(email, password);
		else if (QCoreApplication::arguments().contains("-compositor"))
			CTrayIconController::SharedInstance()->ShowInformation(TrayInfo_OculusNotLinked);

		// Overwrite sensitive credential data
		email.fill(0);
		password.fill(0);
		return true;
	}
	return false;
}

void COculusPlatform::Update()
{
	ovrMessageHandle message = nullptr;
	while ((message = PopMessage()) != nullptr)
	{
		switch (GetType(message))
		{
		case ovrMessage_User_GetAccessToken:
			qDebug("Got the user access token");
			m_strAccessToken = GetString(message);
			TokenChanged();
		break;
		case ovrMessage_Platform_InitializeStandaloneOculus:
			ovrPlatformInitializeResult result = ovrPlatformInitialize_Uninitialized;
			ovrPlatformInitializeHandle handle = GetPlatformInitialize(message);
			if (handle)
				result = GetResult(handle);

			if (result == ovrPlatformInitialize_Success)
			{
				qDebug("Logged into Oculus Platform");
				GetAccessToken();
			}
			else if (result == ovrPlatformInitialize_Unknown ||
				result == ovrPlatformInitialize_InvalidCredentials)
			{
				qDebug("Invalid Oculus Platform credentials");
				QString user, password;
				if (WindowsServices::PromptCredentials(user, password, true))
					Login(user, password);

				// Overwrite sensitive credential data
				user.fill(0);
				password.fill(0);
			}
			else
			{
				qDebug("Oculus Platform failed with %d", result);
			}
		break;
		}
		FreeMessage(message);
	}
}

bool COculusPlatform::Login(const QString& email, const QString& password)
{
	if (email.isEmpty() || password.isEmpty())
		return false;

	QByteArray emailUtf8 = email.toUtf8(), passwordUtf8 = password.toUtf8();
	ovrOculusInitParams params;
	params.sType = ovrPlatformStructureType_OculusInitParams;
	params.email = emailUtf8.constData();
	params.password = passwordUtf8.constData();
	params.appId = AppId;
	params.uriPrefixOverride = nullptr;
	ovrRequest req = InitializeStandaloneOculus(&params);
	m_PollTimer.start(1000);
	return req != 0;
}

void COculusPlatform::Logout()
{
	m_PollTimer.stop();
	m_strAccessToken.clear();
	TokenChanged();
}
