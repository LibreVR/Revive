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

const wchar_t* WindowsServices::CredTargetName = L"Revive/Oculus";

bool WindowsServices::PromptCredentials(QString& user, QString& password, bool failed)
{
	PVOID authBuffer = nullptr;
	ULONG authSize = 0;
	ULONG pkg = 0;
	BOOL save = false;
	uint32_t flags = CREDUIWIN_GENERIC | CREDUIWIN_CHECKBOX;

	QByteArray packedAuth;
	if (ReadCredentials(user, password))
	{
		DWORD size = 0;
		CredPackAuthenticationBufferW(0, (LPWSTR)user.data(), (LPWSTR)password.data(), nullptr, &size);
		packedAuth.resize(size);
		CredPackAuthenticationBufferW(0, (LPWSTR)user.data(), (LPWSTR)password.data(), (PBYTE)packedAuth.data(), &size);
	}

	CREDUI_INFOW pcred;
	pcred.cbSize = sizeof(CREDUI_INFOW);
	pcred.hwndParent = NULL;
	pcred.pszMessageText = L"Please log in to your Oculus account to enable online multiplayer.";
	pcred.pszCaptionText = L"Revive Dashboard";
	pcred.hbmBanner = nullptr;
	DWORD result = CredUIPromptForWindowsCredentialsW(&pcred, failed ? ERROR_LOGON_FAILURE : 0, &pkg, packedAuth.constData(), packedAuth.size(), &authBuffer, &authSize, &save, flags);
	packedAuth.fill(0);
	if (result == NO_ERROR)
	{
		DWORD userSize = 0, passwordSize = 0;
		CredUnPackAuthenticationBufferW(0,
			authBuffer, authSize,
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

		CredUnPackAuthenticationBufferW(0,
			authBuffer, authSize,
			(LPWSTR)user.data(), &userSize,
			nullptr, nullptr,
			(LPWSTR)password.data(), &passwordSize);

		if (save)
			WriteCredentials(user, password);
		else
			DeleteCredentials();

		SecureZeroMemory(authBuffer, authSize);
		CoTaskMemFree(authBuffer);
		return true;
	}
	return false;
}

bool WindowsServices::ReadCredentials(QString& user, QString& password)
{
	PCREDENTIALW pcred;
	if (!CredReadW (CredTargetName, CRED_TYPE_GENERIC, 0, &pcred))
		return false;

	user = QString::fromWCharArray(pcred->UserName);
	password.setUnicode((QChar*)pcred->CredentialBlob, pcred->CredentialBlobSize / sizeof(QChar));
	CredFree(pcred);
	return true;
}

bool WindowsServices::WriteCredentials(const QString& user, const QString& password)
{
	// We're casting a bunch of const away here, but it should be safe
	CREDENTIALW cred = {0};
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = (LPWSTR)CredTargetName;
	cred.CredentialBlobSize = password.size() * sizeof(QChar);
	cred.CredentialBlob = (LPBYTE)password.unicode();
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.UserName = (LPWSTR)user.utf16();
	return !!CredWriteW(&cred, 0);
}

bool WindowsServices::DeleteCredentials()
{
	return !!CredDeleteW(CredTargetName, CRED_TYPE_GENERIC, 0);
}
