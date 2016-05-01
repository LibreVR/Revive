#ifndef REVIVEINJECTCONTROLLER_H
#define REVIVEINJECTCONTROLLER_H

#include <QObject>

class CReviveInjectController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

public:
	static CReviveInjectController *SharedInstance();

public:
	CReviveInjectController();
	virtual ~CReviveInjectController();

	Q_INVOKABLE int CreateProcessAndInject(const QUrl &programPath);
};

#endif // REVIVEINJECTCONTROLLER_H
