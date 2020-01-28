#-------------------------------------------------
#
# Project created by QtCreator 2015-06-10T16:57:45
#
#-------------------------------------------------

QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += quick

TARGET = ReviveOverlay
TEMPLATE = app


SOURCES += main.cpp\
    oculusplatform.cpp \
    openvroverlaycontroller.cpp \
    qquickwindowscaled.cpp \
    revivemanifestcontroller.cpp \
    trayiconcontroller.cpp \
    windowsservices.cpp

HEADERS  += \
    oculusplatform.h \
    openvroverlaycontroller.h \
    qquickwindowscaled.h \
    revivemanifestcontroller.h \
    trayiconcontroller.h \
    windowsservices.h

DISTFILES += \
    Library.qml \
    Overlay.qml \
    Oculus.js \
    Settings.qml

INCLUDEPATH += ../Externals/openvr/headers ../Externals/WinSparkle/include ../Externals/LibOVR/Include ../Revive

Debug:LIBS += -L../Debug
Release:LIBS += -L../Release

LIBS += -L../Externals/WinSparkle/x64/Release -lopenvr_api64 -lWinSparkle -lCredui -lAdvapi32

Debug:DESTDIR = ../Debug
Release:DESTDIR = ../Release

RESOURCES += \
    overlay.qrc

win32:RC_ICONS += revive.ico

images.path    = $${DESTDIR}/SupportAssets
images.files   += SupportAssets/*

manifests.path    = $${DESTDIR}
manifests.files   += *.vrmanifest

winsparkle.path    = $${DESTDIR}
winsparkle.files   += ../Externals/WinSparkle/x64/Release/WinSparkle.dll

INSTALLS       += winsparkle images manifests

VERSION = 1.0.0.0
