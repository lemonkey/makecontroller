/*********************************************************************************

 Copyright 2006-2008 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

#include "usb_serial.h"
#include "usb_enum.h"
#include "ext.h" //for calling post() to the Max window.

#ifndef WIN32
#include <sys/ioctl.h>
#endif

t_usbInterface* usb_init( t_usbInterface** uip )
{
  t_usbInterface* usbInt = (t_usbInterface*)malloc( sizeof( t_usbInterface ) );
  usbInt->deviceOpen = false;
  usbInt->debug = false;
  return usbInt;
}

int usb_open( t_usbInterface* usbInt, int devicetype )
{		
  //--------------------------------------- Mac-only -------------------------------
  #ifndef WIN32
  
  if( usbInt->deviceOpen )  //if it's already open, do nothing.
    return USB_E_ALREADY_OPEN;
	
	bool success = findUsbDevice( usbInt, devicetype );
	
	if (!success )
		return USB_E_NOT_OPEN;
		
  // now try to actually do something
  usbInt->deviceHandle = open( usbInt->deviceLocation, O_RDWR | O_NOCTTY | O_NDELAY );
  if ( usbInt->deviceHandle < 0 )
    return USB_E_NOT_OPEN;
  else
  {
    usbInt->deviceOpen = true;
		//post( "USB opened at %s, deviceHandle = %d", usbInt->deviceName, usbInt->deviceHandle);
    return USB_OK;
  }
  #endif
		
  //--------------------------------------- Windows-only -------------------------------
  
#ifdef WIN32
  bool result;

  if( usbInt->deviceOpen )  //if it's already open, do nothing.
    return USB_E_ALREADY_OPEN;
  
  if( openDevice( usbInt ) == 0 )
	{
    Sleep( 10 );  // wait after opening it before trying to read/write
		usbInt->deviceOpen = true;
		return USB_OK;
	}
  //post( "mc.usb did not open." );
  return USB_E_NOT_OPEN;
  #endif
}

void usb_close( t_usbInterface* usbInt )
{
  if( usbInt->deviceOpen )
  {
    usb_flush( usbInt );
    //Mac-only
    #ifndef WIN32
    close( usbInt->deviceHandle );
    usbInt->deviceHandle = -1;
    usbInt->deviceOpen = false;
    #endif
	//Windows only
    #ifdef WIN32
    CloseHandle( usbInt->deviceHandle );
    usbInt->deviceHandle = INVALID_HANDLE_VALUE;
    usbInt->deviceOpen = false;
    #endif
  }
}

int usb_read( t_usbInterface* usbInt, char* buffer, int length )
{
  //--------------------------------------- Mac-only -------------------------------
#ifndef WIN32
  int count;

  if( !usbInt->deviceOpen )
    return USB_E_NOT_OPEN;

  count = read( usbInt->deviceHandle, buffer, length );
  if( count < 1 )
  {
    int retval = USB_E_IOERROR;
    if( count == 0 )
      retval = USB_E_CLOSE;
    else if( count == -1 )
    {
      if ( errno == EAGAIN )
        retval = USB_E_NOTHING_AVAILABLE; // non-blocking but no data available
      else
        //post( "Some other error...errno = %d", errno );
        retval = USB_E_IOERROR;
    }
    return retval;
  }
  else
    return count;
#endif

  //--------------------------------------- Windows-only -------------------------------
#ifdef WIN32
  //Windows-only
  int retVal = -1;
  DWORD Win_BytesRead=0;
  DWORD Win_ErrorMask=0;
  if( !ReadFile( usbInt->deviceHandle, buffer, (DWORD)length, &Win_BytesRead, NULL) ) 
  {
    if( usbInt->debug )
      error("mc.usb: read error - %d", GetLastError());
  }
  else
    retVal=((int)Win_BytesRead);

  return retVal;
#endif //Windows-only usbRead( )	
}

int usb_write( t_usbInterface* usbInt, char* buffer, int length )
{ 
  //--------------------------------------- Mac-only -------------------------------
  #ifndef WIN32
	if( !usbInt->deviceOpen )  //then try to open it
    return USB_E_NOT_OPEN;
  
	int size = write( usbInt->deviceHandle, buffer, length );
	if ( size > 0 )
		return size;
	else if( errno == EAGAIN )
		return USB_E_NOTHING_AVAILABLE;
  else
  {
    if( usbInt->debug )
      error("mc.usb: write error - %s (%d)", strerror(errno), errno);
    return USB_E_IOERROR;
  }
  #endif
	
  //--------------------------------------- Windows-only -------------------------------
  #ifdef WIN32
  int retVal = -1;
  DWORD Win_BytesWritten;
  if (!WriteFile( usbInt->deviceHandle, (void*)buffer, (DWORD)length, &Win_BytesWritten, NULL))
  {
    if( usbInt->debug )
      error("mc.usb: write error: %d", GetLastError());
  }
  else
    retVal=((int)Win_BytesWritten);

  return retVal;
  #endif //Windows-only usb_write( )
}

void usb_flush( t_usbInterface* usbInt )
{
#ifndef WIN32
  if (usbInt->deviceOpen)
    tcflush(usbInt->deviceHandle, TCIOFLUSH);
#endif

#ifdef WIN32
  if (usbInt->deviceOpen)
    FlushFileBuffers(usbInt->deviceHandle);
#endif
}

int usb_writeChar( t_usbInterface* usbInt, char c )
{
	return usb_write( usbInt, &c, 1 );
}

int usb_numBytesAvailable( t_usbInterface* usbInt )
{
	int n = 0;

	#ifndef WIN32
	if( ioctl( usbInt->deviceHandle, FIONREAD, &n ) < 0 )
		return USB_E_CLOSE;
	#endif // Mac-only usb_numBytesAvailable( )

	#ifdef WIN32
	COMSTAT status;
	unsigned long state;
	if( ClearCommError( usbInt->deviceHandle, &state, &status) )
	  n = status.cbInQue;
  else
  {
    if( usbInt->debug )
      error("mc.usb: error reading number of bytes in queue - %d", GetLastError());
  }
	#endif // Windows-only usb_numBytesAvailable( )

	return n;
}

// Windows specific functions
#ifdef WIN32

int openDevice( t_usbInterface* usbInt )
{
  DCB dcb;
  COMMTIMEOUTS timeouts;

  // if it's already open, do nothing
  if( usbInt->deviceOpen )
    return 0;

  // Open the port
  usbInt->deviceHandle = CreateFile( (TCHAR*)usbInt->deviceLocation,
			GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0 );
  
  if ( usbInt->deviceHandle == INVALID_HANDLE_VALUE )
  {
    if( usbInt->debug )
    {
      void *buf = formatErrorMsg();
      error("mc.usb: error opening device at %s - %s (%d)", usbInt->deviceLocation, buf, GetLastError());
      LocalFree(buf);
    }
    return -1;
  }

  GetCommState( usbInt->deviceHandle, &dcb );
  dcb.BaudRate = CBR_115200;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fOutxCtsFlow = TRUE;
  dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
  dcb.fAbortOnError = TRUE;
  if( !SetCommState( usbInt->deviceHandle, &dcb ) )
  {
    if( usbInt->debug )
      error("mc.usb: SetCommState failed - %d", GetLastError());
	  return -1;
  }

  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;   
  if( ! SetCommTimeouts( usbInt->deviceHandle, &timeouts ) )
  {
    if( usbInt->debug )
      error("mc.usb: SetCommTimeouts failed - %d", GetLastError());
	  return -1;
  }

  EscapeCommFunction( usbInt->deviceHandle, SETDTR );

  usbInt->deviceOpen = true;

  return 0;
}

void* formatErrorMsg( )
{
  void* buf;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,GetLastError(),0,
                    (LPTSTR)&buf,0,NULL);
  return buf;
}

#endif



