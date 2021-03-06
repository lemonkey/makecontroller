MCHELPER_VERSION = "2.5.1"
TEMPLATE = app
TARGET = mchelper

FORMS = layouts/mainwindow.ui \
        layouts/inspector.ui \
        layouts/preferences.ui \
        layouts/uploader.ui \
        layouts/about.ui \
        layouts/appupdater.ui

HEADERS = include/MainWindow.h \
          include/OscXmlServer.h \
          include/Osc.h \
          include/Inspector.h \
          include/NetworkMonitor.h \
          include/UsbMonitor.h \
          include/Preferences.h \
          include/Uploader.h \
          include/About.h \
          include/Board.h \
          include/PacketInterface.h \
          include/MsgType.h \
          include/BoardType.h \
          include/PacketUdp.h \
          include/PacketUsbSerial.h \
          include/AppUpdater.h

SOURCES = source/main.cpp \
          source/MainWindow.cpp \
          source/OscXmlServer.cpp \
          source/Osc.cpp \
          source/Inspector.cpp \
          source/NetworkMonitor.cpp \
          source/UsbMonitor.cpp \
          source/Preferences.cpp \
          source/Uploader.cpp \
          source/About.cpp \
          source/Board.cpp \
          source/PacketUdp.cpp \
          source/PacketUsbSerial.cpp \
          source/AppUpdater.cpp

TRANSLATIONS = translations/mchelper_fr.ts

QT += network
DEFINES += MCHELPER_VERSION=\\\"$${MCHELPER_VERSION}\\\"
RESOURCES   += resources/mchelper.qrc
INCLUDEPATH += include
OBJECTS_DIR  = tmp
MOC_DIR      = tmp
RCC_DIR      = tmp
UI_DIR       = tmp

# *******************************************
#           platform specific stuff
# *******************************************
macx {
  QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.5
  QMAKE_MAC_SDK = /Developer/SDKs/MacOSX10.6.sdk #need this if building on PPC
  ICON = resources/icons/mchelper.icns
}

win32 {
  DEFINES += WINVER=0x0501
  LIBS += -lSetupapi
  RC_FILE = resources/icons/mchelper.rc # for application icon
}


# *******************************************
#              qextserialport
# *******************************************
INCLUDEPATH += source/qextserialport
HEADERS +=  source/qextserialport/qextserialport.h \
            source/qextserialport/qextserialenumerator.h
            
SOURCES +=  source/qextserialport/qextserialport.cpp \
            source/qextserialport/qextserialenumerator.cpp

unix:SOURCES  += source/qextserialport/posix_qextserialport.cpp
macx: LIBS += -framework IOKit -framework Carbon # use IOKit on OS X
unix {
  !macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += dbus-1 hal
  }
}

win32:SOURCES += source/qextserialport/win_qextserialport.cpp
win32:DEFINES += _TTY_WIN_


# *******************************************
#              test suite
# *******************************************

test_suite {
  TARGET       = mchelper_test
  DEFINES     += MCHELPER_TEST_SUITE
  CONFIG      += qtestlib
  CONFIG      -= release # always, no matter what it's set to above
  DESTDIR      = tests
  INCLUDEPATH += tests
  OBJECTS_DIR  = tests/tmp
  MOC_DIR      = tests/tmp
  RCC_DIR      = tests/tmp
  UI_DIR       = tests/tmp
  
  SOURCES -=  source/main.cpp
  
  SOURCES +=  tests/main.cpp \
              tests/TestOsc.cpp \
              tests/TestXmlServer.cpp
              
  HEADERS +=  tests/TestOsc.h \
              tests/TestXmlServer.h
}
