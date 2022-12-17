#ifndef WINDOWSSERVICES_H
#define WINDOWSSERVICES_H

#include <QString>

class WindowsServices
{
public:
	static bool CreateShortcut(QString shortcutPath, QString exePath);
	static bool CopyFiles(QStringList files, QString destination, QStringList newName);
	static bool DeleteCredentials();

private:
	static const wchar_t* CredTargetName;
};

#endif // WINDOWSSERVICES_H
