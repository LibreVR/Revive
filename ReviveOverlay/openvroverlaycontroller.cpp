//====== Copyright Valve Corporation, All rights reserved. =======


#include "openvroverlaycontroller.h"
#include "trayiconcontroller.h"
#include "qquickwindowscaled.h"

#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QCursor>
#include <QCoreApplication>

using namespace vr;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COpenVROverlayController *s_pSharedVRController = NULL;

COpenVROverlayController *COpenVROverlayController::SharedInstance()
{
	if ( !s_pSharedVRController )
	{
		s_pSharedVRController = new COpenVROverlayController();
	}
	return s_pSharedVRController;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COpenVROverlayController::COpenVROverlayController()
	: BaseClass()
	, m_eLastHmdError( vr::VRInitError_None )
	, m_eCompositorError( vr::VRInitError_None )
	, m_eOverlayError( vr::VRInitError_None )
	, m_strVRDriver( "No Driver" )
	, m_strVRDisplay( "No Display" )
	, m_pOpenGLContext( NULL )
	, m_pRenderControl( NULL )
	, m_pOffscreenSurface ( NULL )
	, m_pFbo( NULL )
	, m_pWindow( NULL )
	, m_pPumpEventsTimer( NULL )
	, m_lastMouseButtons( 0 )
	, m_ulOverlayHandle( vr::k_ulOverlayHandleInvalid )
	, m_bGamepadFocus( false )
	, m_bLoading( false )
{
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COpenVROverlayController::~COpenVROverlayController()
{
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get a string from a tracked device property and turn it
//			into a QString
//-----------------------------------------------------------------------------
QString GetTrackedDeviceString( vr::IVRSystem *pHmd, vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop )
{
	char buf[128];
	vr::TrackedPropertyError err;
	pHmd->GetStringTrackedDeviceProperty( unDevice, prop, buf, sizeof( buf ), &err );
	if( err != vr::TrackedProp_Success )
	{
		return QString( "Error Getting String: " ) + pHmd->GetPropErrorNameFromEnum( err );
	}
	else
	{
		return buf;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COpenVROverlayController::Init()
{
	bool bSuccess = true;

	m_strName = "revive";

	QStringList arguments = qApp->arguments();

	int nNameArg = arguments.indexOf( "-name" );
	if( nNameArg != -1 && nNameArg + 2 <= arguments.size() )
	{
		m_strName = arguments.at( nNameArg + 1 );
	}

	QSurfaceFormat format;
	// Qt Quick may need a depth and stencil buffer. Always make sure these are available.
	format.setDepthBufferSize(16);
	format.setStencilBufferSize(8);
	format.setRedBufferSize(8);
	format.setGreenBufferSize(8);
	format.setBlueBufferSize(8);
	format.setAlphaBufferSize(8);

	m_pOpenGLContext = new QOpenGLContext();
	m_pOpenGLContext->setFormat( format );
	bSuccess = m_pOpenGLContext->create();
	if( !bSuccess )
	{
		qDebug( "Failed to create OpenGL context" );
		return false;
	}

	// create an offscreen surface to attach the context and FBO to
	m_pOffscreenSurface = new QOffscreenSurface();
	// Pass m_context->format(), not format. Format does not specify and color buffer
	// sizes, while the context, that has just been created, reports a format that has
	// these values filled in. Pass this to the offscreen surface to make sure it will be
	// compatible with the context's configuration.
	m_pOffscreenSurface->setFormat(m_pOpenGLContext->format());
	m_pOffscreenSurface->create();
	m_pOpenGLContext->makeCurrent( m_pOffscreenSurface );

	m_pRenderControl = new QQuickRenderControl();

	// Create a QQuickWindow that is associated with out render control. Note that this
	// window never gets created or shown, meaning that it will never get an underlying
	// native (platform) window.
	m_pWindow = new QQuickWindowScaled(m_pRenderControl);
	m_pWindow->setMinimumSize(QSize(1280, 720));
	m_pWindow->setTitle("Revive Dashboard");

	// Load the thumbnail
	QImage image(":/revive_overlay.png");
	m_pThumbnailTexture = new QOpenGLTexture(image);

	// When Quick says there is a need to render, we will not render immediately. Instead,
	// a timer with a small interval is used to get better performance.
	m_pUpdateTimer = new QTimer( this );
	m_pUpdateTimer->setSingleShot( true );
	m_pUpdateTimer->setInterval( 5 );
	connect( m_pUpdateTimer, &QTimer::timeout, this, &COpenVROverlayController::OnSceneChanged );

	connect( m_pRenderControl, &QQuickRenderControl::renderRequested, this, &COpenVROverlayController::OnRequestUpdate);
	connect( m_pRenderControl, &QQuickRenderControl::sceneChanged, this, &COpenVROverlayController::OnRequestUpdate);

	// Loading the OpenVR Runtime
	bSuccess = ConnectToVRRuntime();
	if( !bSuccess )
	{
		qDebug( "Failed to connect to OpenVR Runtime" );
		return false;
	}

	// Check if the compositor is ready
	bSuccess = bSuccess && vr::VRCompositor() != NULL;
	if( !bSuccess )
	{
		qDebug( "Failed to connect to the compositor" );
		return false;
	}

	if( vr::VROverlay() )
	{
		std::string sKey = m_strName.toStdString() + std::string( ".overlay" );
		vr::VROverlayError overlayError = vr::VROverlay()->CreateDashboardOverlay( sKey.c_str(), m_strName.toStdString().c_str(), &m_ulOverlayHandle, &m_ulOverlayThumbnailHandle );
		bSuccess = bSuccess && overlayError == vr::VROverlayError_None;
	}

	if( bSuccess )
	{
		vr::VROverlay()->SetOverlayWidthInMeters( m_ulOverlayHandle, 3.0f );
		vr::VROverlay()->SetOverlayAlpha( m_ulOverlayHandle, 0.9f );
		vr::VROverlay()->SetOverlayInputMethod( m_ulOverlayHandle, VROverlayInputMethod_Mouse );
		vr::VROverlay()->SetOverlayFlag( m_ulOverlayHandle, VROverlayFlags_SendVRDiscreteScrollEvents, true );
		vr::VROverlay()->SetOverlayFlag( m_ulOverlayHandle, VROverlayFlags_AcceptsGamepadEvents, true );
		vr::VROverlay()->SetOverlayFlag( m_ulOverlayHandle, VROverlayFlags_ShowGamepadFocus, true );
		UpdateThumbnail();

		m_pPumpEventsTimer = new QTimer( this );
		connect(m_pPumpEventsTimer, SIGNAL( timeout() ), this, SLOT( OnTimeoutPumpEvents() ) );
		m_pPumpEventsTimer->setInterval( 20 );
		m_pPumpEventsTimer->start();
	}
	else
	{
		qDebug("Failed to create the dashboard overlay (is it already running?)");
	}
	return bSuccess;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::Shutdown()
{
	DisconnectFromVRRuntime();

	delete m_pWindow;
	delete m_pRenderControl;
	delete m_pFbo;
	delete m_pOffscreenSurface;
	delete m_pThumbnailTexture;

	if( m_pOpenGLContext )
	{
//		m_pOpenGLContext->destroy();
		delete m_pOpenGLContext;
		m_pOpenGLContext = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::OnSceneChanged()
{
	// skip rendering if the overlay and window aren't visible
	const bool overlayVisible = vr::VROverlay() && (vr::VROverlay()->IsOverlayVisible(m_ulOverlayHandle) || vr::VROverlay()->IsOverlayVisible(m_ulOverlayThumbnailHandle));
	if(!overlayVisible && !m_pWindow->isVisible())
		return;

	if (!m_pOpenGLContext->makeCurrent(m_pOffscreenSurface))
		return;

	// Polish, synchronize and render the next frame (into our fbo).  In this example
	// everything happens on the same thread and therefore all three steps are performed
	// in succession from here. In a threaded setup the render() call would happen on a
	// separate thread.
	m_pRenderControl->polishItems();
	m_pRenderControl->sync();
	m_pRenderControl->render();

	m_pWindow->resetOpenGLState();
	QOpenGLFramebufferObject::bindDefault();

	m_pOpenGLContext->functions()->glFlush();
	uintptr_t unTexture = m_pFbo->texture();
	if( vr::VROverlay() && unTexture != 0 )
	{
		vr::Texture_t texture = {(void*)unTexture, vr::TextureType_OpenGL, vr::ColorSpace_Auto };
		vr::VROverlay()->SetOverlayTexture( m_ulOverlayHandle, &texture );
	}

	if (!m_pOpenGLContext->makeCurrent(m_pWindow))
		return;

	QRect target(QPoint(), m_pWindow->size());
	QRect source(QPoint(), m_pWindow->renderTargetSize());
	QOpenGLFramebufferObject::blitFramebuffer(nullptr, target, m_pFbo, source, GL_COLOR_BUFFER_BIT, GL_LINEAR);
	m_pOpenGLContext->swapBuffers(m_pWindow);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::OnRequestUpdate()
{
	if (!m_pUpdateTimer->isActive())
		m_pUpdateTimer->start();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::OnTimeoutPumpEvents()
{
	if( !vr::VRSystem() )
		return;

	vr::VREvent_t vrEvent;
	while( vr::VROverlay()->PollNextOverlayEvent( m_ulOverlayHandle, &vrEvent, sizeof( vrEvent )  ) )
	{
		switch( vrEvent.eventType )
		{
		case vr::VREvent_MouseMove:
			{
				QPointF ptNewMouse( vrEvent.data.mouse.x, vrEvent.data.mouse.y );
				QPoint ptGlobal = ptNewMouse.toPoint();
				QMouseEvent mouseEvent( QEvent::MouseMove,
										ptNewMouse,
										ptNewMouse,
										ptGlobal,
										Qt::NoButton,
										m_lastMouseButtons,
										0,
										Qt::MouseEventSynthesizedByApplication );

				m_ptLastMouse = ptNewMouse;

				QCoreApplication::sendEvent( m_pWindow, &mouseEvent );
				OnRequestUpdate();
			}
			break;

		case vr::VREvent_MouseButtonDown:
			{
				Qt::MouseButton button = vrEvent.data.mouse.button == vr::VRMouseButton_Right ? Qt::RightButton : Qt::LeftButton;

				m_lastMouseButtons |= button;

				QPoint ptGlobal = m_ptLastMouse.toPoint();
				QMouseEvent mouseEvent( QEvent::MouseButtonPress,
										m_ptLastMouse,
										m_ptLastMouse,
										ptGlobal,
										button,
										m_lastMouseButtons,
										0,
										Qt::MouseEventSynthesizedByApplication );

				QCoreApplication::sendEvent( m_pWindow, &mouseEvent );
			}
			break;

		case vr::VREvent_MouseButtonUp:
			{
				Qt::MouseButton button = vrEvent.data.mouse.button == vr::VRMouseButton_Right ? Qt::RightButton : Qt::LeftButton;
				m_lastMouseButtons &= ~button;

				QPoint ptGlobal = m_ptLastMouse.toPoint();
				QMouseEvent mouseEvent( QEvent::MouseButtonRelease,
										m_ptLastMouse,
										m_ptLastMouse,
										ptGlobal,
										button,
										m_lastMouseButtons,
										0,
										Qt::MouseEventSynthesizedByApplication );

				QCoreApplication::sendEvent( m_pWindow, &mouseEvent );
			}
			break;

		case vr::VREvent_ScrollDiscrete:
			{
				// Wheel speed is defined in 1/8 of a degree
				QPoint ptNewWheel( vrEvent.data.scroll.xdelta * 360.0f * 8.0f, vrEvent.data.scroll.ydelta * 360.0f * 8.0f );
				QPoint ptGlobal = m_ptLastMouse.toPoint();
				QWheelEvent wheelEvent( m_ptLastMouse,
										ptGlobal,
										QPoint(),
										ptNewWheel,
										0,
										Qt::Vertical,
										m_lastMouseButtons,
										0,
										Qt::NoScrollPhase,
										Qt::MouseEventSynthesizedByApplication );

				QCoreApplication::sendEvent( m_pWindow, &wheelEvent );
			}
			break;

		case vr::VREvent_OverlayShown:
			{
				m_pWindow->update();
			}
			break;

		case vr::VREvent_OverlayHidden:
			{
				m_lastMouseButtons = 0;

				QPoint ptGlobal = m_ptLastMouse.toPoint();
				QMouseEvent mouseEvent( QEvent::MouseButtonRelease,
										m_ptLastMouse,
										m_ptLastMouse,
										ptGlobal,
										Qt::LeftButton,
										m_lastMouseButtons,
										0,
										Qt::MouseEventSynthesizedByApplication );

				QCoreApplication::sendEvent( m_pWindow, &mouseEvent );
			}
			break;

		case vr::VREvent_OverlayGamepadFocusGained:
			m_bGamepadFocus = true;
			break;

		case vr::VREvent_OverlayGamepadFocusLost:
			m_bGamepadFocus = false;
			break;

		case vr::VREvent_Quit:
			vr::VRSystem()->AcknowledgeQuit_Exiting();
			CTrayIconController::SharedInstance()->quit();
			break;
		}
	}

	while( vr::VRSystem()->PollNextEvent( &vrEvent, sizeof( vrEvent )  ) )
	{
		switch( vrEvent.eventType )
		{
		case vr::VREvent_SceneApplicationChanged:
			// Ignore changed-to-compositor event
			if (vrEvent.data.process.pid != 0)
				SetLoading(false);
			break;

		case vr::VREvent_ApplicationTransitionStarted:
		case vr::VREvent_ApplicationTransitionNewAppStarted:
			SetLoading(true);
			break;
		}
	}

	if( m_ulOverlayThumbnailHandle != vr::k_ulOverlayHandleInvalid )
	{
		while( vr::VROverlay()->PollNextOverlayEvent( m_ulOverlayThumbnailHandle, &vrEvent, sizeof( vrEvent)  ) )
		{
			switch( vrEvent.eventType )
			{
			case vr::VREvent_OverlayShown:
				{
					m_pWindow->update();
				}
				break;
			}
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::SetQuickItem( QQuickItem *pItem )
{
	m_pFbo = new QOpenGLFramebufferObject( pItem->width(), pItem->height(),  QOpenGLFramebufferObject::CombinedDepthStencil );
	m_pWindow->setRenderTarget(m_pFbo);

	// The root item is ready. Associate it with the window.
	pItem->setParentItem(m_pWindow->contentItem());

	// Initialize the render control and our OpenGL resources.
	m_pOpenGLContext->makeCurrent(m_pOffscreenSurface);
	m_pRenderControl->initialize(m_pOpenGLContext);

	if( vr::VROverlay() )
	{
		vr::HmdVector2_t vecWindowSize =
		{
			(float)pItem->width(),
			(float)pItem->height()
		};
		vr::VROverlay()->SetOverlayMouseScale( m_ulOverlayHandle, &vecWindowSize );
	}

	if (!QCoreApplication::arguments().contains("-compositor"))
		m_pWindow->show();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COpenVROverlayController::ConnectToVRRuntime()
{
	m_eLastHmdError = vr::VRInitError_None;
	vr::IVRSystem *pVRSystem = vr::VR_Init( &m_eLastHmdError, vr::VRApplication_Background );

	if ( m_eLastHmdError != vr::VRInitError_None )
	{
		m_strVRDriver = "No Driver";
		m_strVRDisplay = "No Display";
		return false;
	}

	m_strVRDriver = GetTrackedDeviceString(pVRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	m_strVRDisplay = GetTrackedDeviceString(pVRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

	uint32_t size = 0;
	std::vector<char> buffer;
	buffer.resize(1);
	VR_GetRuntimePath(buffer.data(), size, &size);
	buffer.resize(size);
	if (VR_GetRuntimePath(buffer.data(), size, &size))
		m_strRuntimeURL = QUrl::fromLocalFile(buffer.data()).url();
	if (!m_strRuntimeURL.endsWith('/'))
		m_strRuntimeURL.append('/');
	emit RuntimeChanged();

	return true;
}


void COpenVROverlayController::DisconnectFromVRRuntime()
{
	vr::VR_Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
QString COpenVROverlayController::GetVRDriverString()
{
	return m_strVRDriver;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
QString COpenVROverlayController::GetVRDisplayString()
{
	return m_strVRDisplay;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COpenVROverlayController::BHMDAvailable()
{
	return vr::VRSystem() != NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
vr::HmdError COpenVROverlayController::GetLastHmdError()
{
	return m_eLastHmdError;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::UpdateThumbnail()
{
	uintptr_t unThumbnail = m_pThumbnailTexture->textureId();
	if( vr::VROverlay() && unThumbnail != 0 )
	{
		vr::Texture_t thumbnail = {(void*)unThumbnail, vr::TextureType_OpenGL, vr::ColorSpace_Auto };
		vr::VROverlay()->SetOverlayTexture( m_ulOverlayThumbnailHandle, &thumbnail );
	}
}
