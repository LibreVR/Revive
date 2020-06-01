#ifndef WINDOWSSERVICES_H
#define WINDOWSSERVICES_H

#include <QString>

typedef struct _CREDENTIALW* PCREDENTIALW;

class WindowsServices
{
public:
	static bool CreateShortcut(QString shortcutPath, QString exePath);
	static bool CopyFiles(QStringList files, QString destination, QStringList newName);
	static bool PromptCredentials(QString& user, QString& password, bool failed = false);
	static bool ReadCredentials(QString& user, QString& password);
	static bool WriteCredentials(const QString& user, const QString& password);
	static bool DeleteCredentials();
	static void UnpackCredentials(PCREDENTIALW pCred, QString& user, QString& password);

private:
	static const wchar_t* CredTargetName;
};

#endif // WINDOWSSERVICES_H
