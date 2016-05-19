//====== Copyright Valve Corporation, All rights reserved. =======


#include "openvroverlaycontroller.h"


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
	, m_bManualMouseHandling( false )
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

	m_pOpenGLContext = new QOpenGLContext();
	m_pOpenGLContext->setFormat( format );
	bSuccess = m_pOpenGLContext->create();
	if( !bSuccess )
		return false;

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
	m_pWindow = new QQuickWindow(m_pRenderControl);

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

	bSuccess = bSuccess && vr::VRCompositor() != NULL;

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
		vr::VROverlay()->SetOverlayInputMethod( m_ulOverlayHandle, vr::VROverlayInputMethod_Mouse );
		vr::VROverlay()->SetOverlayFlag( m_ulOverlayHandle, VROverlayFlags_SendVRScrollEvents, true );

		m_pPumpEventsTimer = new QTimer( this );
		connect(m_pPumpEventsTimer, SIGNAL( timeout() ), this, SLOT( OnTimeoutPumpEvents() ) );
		m_pPumpEventsTimer->setInterval( 20 );
		m_pPumpEventsTimer->start();

	}
	return true;
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
	// skip rendering if the overlay isn't visible
	if( !vr::VROverlay() ||
		!vr::VROverlay()->IsOverlayVisible( m_ulOverlayHandle ) && !vr::VROverlay()->IsOverlayVisible( m_ulOverlayThumbnailHandle ) )
		return;

	if (!m_pOpenGLContext->makeCurrent( m_pOffscreenSurface ))
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

	GLuint unTexture = m_pFbo->texture();
	if( unTexture != 0 )
	{
		vr::Texture_t texture = {(void*)unTexture, vr::API_OpenGL, vr::ColorSpace_Auto };
		vr::VROverlay()->SetOverlayTexture( m_ulOverlayHandle, &texture );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::OnRequestUpdate()
{
	if (!m_pUpdateTimer->isActive())
		m_pUpdateTimer->start();
	UpdateThumbnail();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COpenVROverlayController::OnTimeoutPumpEvents()
{
	if( !vr::VRSystem() )
		return;

	if( m_bManualMouseHandling )
	{
		// tell OpenVR to make some events for us
		for( vr::TrackedDeviceIndex_t unDeviceId = 1; unDeviceId < vr::k_unControllerStateAxisCount; unDeviceId++ )
		{
			if( vr::VROverlay()->HandleControllerOverlayInteractionAsMouse( m_ulOverlayHandle, unDeviceId ) )
			{
				break;
			}
		}
	}

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
										ptGlobal,
										Qt::NoButton,
										m_lastMouseButtons,
										0 );

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
										ptGlobal,
										button,
										m_lastMouseButtons,
										0 );

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
										ptGlobal,
										button,
										m_lastMouseButtons,
										0 );

				QCoreApplication::sendEvent( m_pWindow, &mouseEvent );
			}
			break;

		case vr::VREvent_Scroll:
			{
				// Wheel speed is defined by 2 * 360 * 8 = 5760
				QPoint ptNewWheel( vrEvent.data.scroll.xdelta * 5760.0f, vrEvent.data.scroll.ydelta * 5760.0f );
				QPoint ptGlobal = m_ptLastMouse.toPoint();
				QWheelEvent wheelEvent( m_ptLastMouse,
										ptGlobal,
										QPoint(),
										ptNewWheel,
										0,
										Qt::Vertical,
										m_lastMouseButtons,
										0 );

				QCoreApplication::sendEvent( m_pWindow, &wheelEvent );
			}
			break;

		case vr::VREvent_OverlayShown:
			{
				m_pWindow->update();
			}
			break;

		case vr::VREvent_Quit:
			QCoreApplication::exit();
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
	m_pWindow->setGeometry(0, 0, pItem->width(), pItem->height());

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

}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COpenVROverlayController::ConnectToVRRuntime()
{
	m_eLastHmdError = vr::VRInitError_None;
	vr::IVRSystem *pVRSystem = vr::VR_Init( &m_eLastHmdError, vr::VRApplication_Overlay );

	if ( m_eLastHmdError != vr::VRInitError_None )
	{
		m_strVRDriver = "No Driver";
		m_strVRDisplay = "No Display";
		return false;
	}

	m_strVRDriver = GetTrackedDeviceString(pVRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String);
	m_strVRDisplay = GetTrackedDeviceString(pVRSystem, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String);

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
	GLuint unThumbnail = m_pThumbnailTexture->textureId();
	if( unThumbnail != 0 )
	{
		vr::Texture_t thumbnail = {(void*)unThumbnail, vr::API_OpenGL, vr::ColorSpace_Auto };
		vr::VROverlay()->SetOverlayTexture( m_ulOverlayThumbnailHandle, &thumbnail );
	}
}
