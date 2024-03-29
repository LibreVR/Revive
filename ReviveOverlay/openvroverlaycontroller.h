//====== Copyright Valve Corporation, All rights reserved. =======

#ifndef OPENVROVERLAYCONTROLLER_H
#define OPENVROVERLAYCONTROLLER_H

#ifdef _WIN32
#pragma once
#endif

#include "openvr.h"

#include <QtCore/QtCore>
// because of incompatibilities with QtOpenGL and GLEW we need to cherry pick includes
#include <QVector2D>
#include <QMatrix4x4>
#include <QVector>
#include <QVector2D>
#include <QVector3D>
#include <QtGui/QOpenGLContext>
#include <QOffscreenSurface>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QQuickItem>
#include <QOpenGLTexture>

class COpenVROverlayController : public QObject
{
	Q_OBJECT
	typedef QObject BaseClass;

	Q_PROPERTY(bool gamepadFocus READ GetGamepadFocus)
	Q_PROPERTY(bool loading READ GetLoading WRITE SetLoading NOTIFY LoadingChanged)
	Q_PROPERTY(QString URL READ GetRuntimeURL NOTIFY RuntimeChanged)

public:
	static COpenVROverlayController *SharedInstance();

public:
	COpenVROverlayController();
	virtual ~COpenVROverlayController();

	bool Init();
	void Shutdown();

	bool BHMDAvailable();
	vr::HmdError GetLastHmdError();

	QString GetVRDriverString();
	QString GetVRDisplayString();
	QString GetName() { return m_strName; }
	bool GetGamepadFocus() { return m_bGamepadFocus && vr::VROverlay()->IsDashboardVisible(); }
	bool GetLoading() { return m_bLoading; }
	void SetLoading(bool loading) { m_bLoading = loading; emit LoadingChanged(); }
	QString GetRuntimeURL() { return m_strRuntimeURL; }

	void SetQuickItem( QQuickItem *pItem );
	void ShowWindow() { if (m_pWindow) { m_pWindow->show(); m_pWindow->update(); } }

public slots:
	void OnSceneChanged();
	void OnTimeoutPumpEvents();
	void OnRequestUpdate();

protected:

signals:
	void LoadingChanged();
	void RuntimeChanged();

private:
	bool ConnectToVRRuntime();
	void DisconnectFromVRRuntime();
	void UpdateThumbnail();

	QString m_strVRDriver;
	QString m_strVRDisplay;
	QString m_strName;
	QString m_strRuntimeURL;

	vr::HmdError m_eLastHmdError;

private:
	vr::HmdError m_eCompositorError;
	vr::HmdError m_eOverlayError;
	vr::VROverlayHandle_t m_ulOverlayHandle;
	vr::VROverlayHandle_t m_ulOverlayThumbnailHandle;

	QOpenGLContext *m_pOpenGLContext;
	QQuickRenderControl *m_pRenderControl;
	QOpenGLFramebufferObject *m_pFbo;
	QOffscreenSurface *m_pOffscreenSurface;
	QOpenGLTexture *m_pThumbnailTexture;

	QTimer *m_pPumpEventsTimer;
	QTimer *m_pUpdateTimer;

	// the window we're drawing into the texture
	QQuickWindow *m_pWindow;

	QPointF m_ptLastMouse;
	Qt::MouseButtons m_lastMouseButtons;
	bool m_bGamepadFocus;
	bool m_bLoading;
};


#endif // OPENVROVERLAYCONTROLLER_H
