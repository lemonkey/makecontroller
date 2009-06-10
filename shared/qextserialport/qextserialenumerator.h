/*!
 * \file qextserialenumerator.h
 * \author Michal Policht
 * \see QextSerialEnumerator
 */
 
#ifndef _QEXTSERIALENUMERATOR_H_
#define _QEXTSERIALENUMERATOR_H_


#include <QString>
#include <QList>
#include <QMainWindow>

#ifdef _TTY_WIN_
	#include <windows.h>
	#include <setupapi.h>
  #include <dbt.h>
#endif /*_TTY_WIN_*/

#ifdef Q_WS_MAC
  #include <IOKit/usb/IOUSBLib.h>
#endif

/*!
 * Structure containing port information.
 */
struct QextPortInfo {
	QString portName;		///< Port name.
	QString physName;		///< Physical name.
	QString friendName;	///< Friendly name.
	QString enumName;		///< Enumerator name.
  int vendorID;       ///< Vendor ID.
  int productID;      ///< Product ID
};

#ifdef _TTY_WIN_
class QextSerialEnumerator;

class QextSerialRegistrationWidget : public QWidget
{
  Q_OBJECT
  public:
    QextSerialRegistrationWidget( QextSerialEnumerator* qese ) {
      this->qese = qese;
    }
    ~QextSerialRegistrationWidget( ) { }

  protected:
    QextSerialEnumerator* qese;
    bool winEvent( MSG* message, long* result );
};
#endif // _TTY_WIN_

/*!
 * Serial port enumerator. This class provides list of ports available in the system.
 * 
 * Windows implementation is based on Zach Gorman's work from 
 * <a href="http://www.codeproject.com">The Code Project</a> (http://www.codeproject.com/system/setupdi.asp).
 *
 * OS X implementation, see
 * http://developer.apple.com/documentation/DeviceDrivers/Conceptual/AccessingHardware/AH_Finding_Devices/chapter_4_section_2.html
 */
class QextSerialEnumerator : public QObject
{
	Q_OBJECT
  public:
    QextSerialEnumerator( );
    ~QextSerialEnumerator( );
  
		#ifdef _TTY_WIN_
    LRESULT onDeviceChangeWin( WPARAM wParam, LPARAM lParam );
    private:
			/*!
			 * Get value of specified property from the registry.
			 * 	\param key handle to an open key.
			 * 	\param property property name.
			 * 	\return property value.
			 */
			static QString getRegKeyValue(HKEY key, LPCTSTR property);

			/*!
			 * Get specific property from registry.
			 * 	\param devInfo pointer to the device information set that contains the interface 
			 * 		and its underlying device. Returned by SetupDiGetClassDevs() function.
			 * 	\param devData pointer to an SP_DEVINFO_DATA structure that defines the device instance.
			 * 		this is returned by SetupDiGetDeviceInterfaceDetail() function. 
			 * 	\param property registry property. One of defined SPDRP_* constants. 
			 * 	\return property string.
			 */
			static QString getDeviceProperty(HDEVINFO devInfo, PSP_DEVINFO_DATA devData, DWORD property);

			/*!
			 * Search for serial ports using setupapi.
			 * 	\param infoList list with result.
			 */
			static void setupAPIScan(QList<QextPortInfo> & infoList);
      void setUpNotificationWin( );
      static bool getDeviceDetails( QextPortInfo* portInfo, HDEVINFO devInfo, 
                              PSP_DEVINFO_DATA devData, WPARAM wParam = DBT_DEVICEARRIVAL );
      static void QextSerialEnumerator::enumerateDevicesWin( HDEVINFO devInfo, GUID* guidDev, QList<QextPortInfo>* infoList );
      HDEVNOTIFY notificationHandle;
      QextSerialRegistrationWidget* notificationWidget;
		#endif /*_TTY_WIN_*/
  
    #ifdef _TTY_POSIX_
    #ifdef Q_WS_MAC
      
      void onDeviceDiscoveredOSX( io_object_t service );
      void onDeviceTerminatedOSX( io_object_t service );
      
    private:
      /*!
       * Search for serial ports using IOKit.
       * 	\param infoList list with result.
       */
      static void scanPortsOSX(QList<QextPortInfo> & infoList);
      static void getSamBaBoards(QList<QextPortInfo> & infoList);
      static bool getServiceDetails( io_object_t service, QextPortInfo* portInfo );
      static bool createSambaMatchingDict( CFMutableDictionaryRef* matchingDictionary );
      void setUpNotificationOSX( );
      IONotificationPortRef notificationPortRef;
      
    #else /* Q_WS_MAC */
      /*!
       * Search for serial ports on unix.
       * 	\param infoList list with result.
       */
      static void scanPortsNix(QList<QextPortInfo> & infoList);
    #endif /* Q_WS_MAC */
    #endif /* _TTY_POSIX_ */

	public:
		/*!
		 * Get list of ports.
		 * 	\return list of ports currently available in the system.
		 */
		static QList<QextPortInfo> getPorts();
    void setUpNotifications( );
  
  signals:
    void deviceDiscovered( const QextPortInfo & info );
    void deviceTerminated( const QextPortInfo & info );
};

#endif /*_QEXTSERIALENUMERATOR_H_*/
