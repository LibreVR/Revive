#ifndef WINDOWSSERVICES_H
#define WINDOWSSERVICES_H

#include <QString>

class WindowsServices
{
public:
	static bool CreateShortcut(QString shortcutPath, QString exePath);
	static bool CopyFiles(QStringList files, QString destination, QStringList newName);
};

#endif // WINDOWSSERVICES_H
