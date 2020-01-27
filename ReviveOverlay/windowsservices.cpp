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

bool WindowsServices::ReadCredentials(QString& user, QString& password)
{
	PCREDENTIALW pcred;
	if (!CredReadW (CredTargetName, CRED_TYPE_GENERIC, 0, &pcred))
		return false;

	user = QString::fromWCharArray(pcred->UserName);
	password = QString::fromWCharArray((wchar_t*)pcred->CredentialBlob, pcred->CredentialBlobSize / sizeof(wchar_t));
	CredFree(pcred);
	return true;
}

bool WindowsServices::WriteCredentials(const QString& user, const QString& password)
{
	// We're casting a bunch of const away here, but it should be safe
	CREDENTIALW cred = {0};
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = (LPWSTR)CredTargetName;
	cred.CredentialBlobSize = (password.size() + 1) * sizeof(wchar_t);
	cred.CredentialBlob = (LPBYTE)password.utf16();
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.UserName = (LPWSTR)user.utf16();
	return !!CredWriteW(&cred, 0);
}

bool WindowsServices::DeleteCredentials()
{
	return !!CredDeleteW(CredTargetName, CRED_TYPE_GENERIC, 0);
}
