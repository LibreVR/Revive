#include "windowsservices.h"
#include <qt_windows.h>
#include <wincred.h>
#include <wrl.h>
#include <Shlobj.h>

#include <QDir>

bool WindowsServices::CreateShortcut(QString shortcutPath, QString exePath)
{
	QString shortcut = QDir::toNativeSeparators(shortcutPath);
	QString exe = QDir::toNativeSeparators(exePath);

	Microsoft::WRL::ComPtr<IShellLink> shellLink;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shellLink));
	if (SUCCEEDED(hr))
	{
		hr = shellLink->SetPath(qUtf16Printable(exe));
		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IPersistFile> persistFile;
			hr = shellLink.As(&persistFile);
			if (SUCCEEDED(hr))
			{
				hr = persistFile->Save(qUtf16Printable(shortcut), TRUE);
			}
		}
	}
	return SUCCEEDED(hr);
}

bool WindowsServices::CopyFiles(QStringList files, QString destination, QStringList newName)
{
	Microsoft::WRL::ComPtr<IFileOperation> pfo;
	HRESULT hr = CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pfo));
	if (SUCCEEDED(hr))
	{
		QString dstDir = QDir::toNativeSeparators(destination);
		Microsoft::WRL::ComPtr<IShellItem> psiTo;
		if (!dstDir.isEmpty())
		{
			hr = SHCreateItemFromParsingName(qUtf16Printable(dstDir),
											 NULL,
											 IID_PPV_ARGS(&psiTo));
		}

		for (QStringList::iterator file = files.begin(), name = newName.begin();
			 SUCCEEDED(hr) && file != files.end() && name != newName.end();
			 file++, name++)
		{
			QString src = QDir::toNativeSeparators(*file);
			Microsoft::WRL::ComPtr<IShellItem> psiFrom;
			hr = SHCreateItemFromParsingName(qUtf16Printable(src), NULL, IID_PPV_ARGS(&psiFrom));
			if (SUCCEEDED(hr))
			{
				hr = pfo->CopyItem(
							psiFrom.Get(),
							psiTo.Get(),
							name->isNull() ? nullptr : qUtf16Printable(*name),
							nullptr);
			}
		}

		if (SUCCEEDED(hr))
		{
			hr = pfo->PerformOperations();
		}
	}
	return SUCCEEDED(hr);
}

const wchar_t* WindowsServices::CredTargetName = L"Revive/OculusPlatform";

bool WindowsServices::PromptCredentials(QString& user, QString& password, bool failed)
{
	PVOID authBuffer = nullptr;
	ULONG authSize = 0;
	ULONG pkg = 0;
	BOOL save = false;
	uint32_t flags = CREDUIWIN_GENERIC | CREDUIWIN_CHECKBOX;

	CREDUI_INFOW pCredInfo;
	pCredInfo.cbSize = sizeof(CREDUI_INFOW);
	pCredInfo.hwndParent = NULL;
	pCredInfo.pszMessageText = L"Please log in to your Oculus account to enable online multiplayer lobbies. Accounts with 2FA are not supported.";
	pCredInfo.pszCaptionText = L"Revive Dashboard";
	pCredInfo.hbmBanner = nullptr;

	PCREDENTIALW pCred;
	DWORD result = ERROR_SUCCESS;
	if (CredReadW(CredTargetName, CRED_TYPE_GENERIC, 0, &pCred))
	{
		result = CredUIPromptForWindowsCredentialsW(&pCredInfo, failed ? ERROR_LOGON_FAILURE : 0, &pkg, pCred->CredentialBlob, pCred->CredentialBlobSize, &authBuffer, &authSize, &save, flags);
		CredFree(pCred);
	}
	else
	{
		result = CredUIPromptForWindowsCredentialsW(&pCredInfo, failed ? ERROR_LOGON_FAILURE : 0, &pkg, nullptr, 0, &authBuffer, &authSize, &save, flags);
	}

	if (result == ERROR_SUCCESS)
		qDebug("success error");
	{
		UnpackCredentials(pCred, user, password);

		if (save)
		{
			CREDENTIALW cred = { 0 };
			cred.Type = CRED_TYPE_GENERIC;
			cred.TargetName = (LPWSTR)CredTargetName;
			cred.CredentialBlobSize = authSize;
			cred.CredentialBlob = (LPBYTE)authBuffer;
			cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
			return !!CredWriteW(&cred, 0);
		}
		else
		{
			DeleteCredentials();
		}

		SecureZeroMemory(authBuffer, authSize);
		CoTaskMemFree(authBuffer);
		return true;
	}
	return false;
}

bool WindowsServices::ReadCredentials(QString& user, QString& password)
{
	PCREDENTIALW pCred;
	if (!CredReadW(CredTargetName, CRED_TYPE_GENERIC, 0, &pCred))
		return false;

	UnpackCredentials(pCred, user, password);
	CredFree(pCred);
	return true;
}

bool WindowsServices::WriteCredentials(const QString& user, const QString& password)
{
	if (user.isEmpty() && password.isEmpty())
	{
		// Did you mean to do this?
		DeleteCredentials();
		return true;
	}

	// We're casting a bunch of const away here, but it should be safe
	QByteArray packedAuth;
	DWORD size = 0;
	CredPackAuthenticationBufferW(0, (LPWSTR)user.data(), (LPWSTR)password.data(), nullptr, &size);
	packedAuth.resize(size);
	CredPackAuthenticationBufferW(0, (LPWSTR)user.data(), (LPWSTR)password.data(), (PBYTE)packedAuth.data(), &size);

	CREDENTIALW cred = {0};
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = (LPWSTR)CredTargetName;
	cred.CredentialBlobSize = packedAuth.size();
	cred.CredentialBlob = (LPBYTE)packedAuth.data();
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	return !!CredWriteW(&cred, 0);
}

bool WindowsServices::DeleteCredentials()
{
	return !!CredDeleteW(CredTargetName, CRED_TYPE_GENERIC, 0);
}

void WindowsServices::UnpackCredentials(PCREDENTIALW pCred, QString& user, QString& password)
{
	DWORD userSize = 0, passwordSize = 0;
	CredUnPackAuthenticationBufferW(0,
		pCred->CredentialBlob, pCred->CredentialBlobSize,
		nullptr, &userSize,
		nullptr, nullptr,
		nullptr, &passwordSize);

	// Destroy old data before resizing the buffer
	user.fill(0);
	password.fill(0);

	// QStrings are not null-terminated, but we need to reserve enough memory for the null terminator
	user.reserve(userSize);
	user.resize(userSize - 1);
	password.reserve(passwordSize);
	password.resize(passwordSize - 1);
	Q_ASSERT((DWORD)user.capacity() >= userSize);
	Q_ASSERT((DWORD)password.capacity() >= passwordSize);

	CredUnPackAuthenticationBufferW(0,
		pCred->CredentialBlob, pCred->CredentialBlobSize,
		(LPWSTR)user.data(), &userSize,
		nullptr, nullptr,
		(LPWSTR)password.data(), &passwordSize);
}
