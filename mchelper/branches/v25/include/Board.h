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

#ifndef BOARD_H_
#define BOARD_H_

#include <QListWidgetItem>
#include <QTimer>
#include "MainWindow.h"
#include "Osc.h"
#include "OscXmlServer.h"
#include "MsgType.h"
#include "BoardType.h"

class PacketInterface;
class MainWindow;
class OscXmlServer;
class Osc;

#include <QString>

class Board : public QObject, public QListWidgetItem
{
  Q_OBJECT
public:
  enum Type { UsbSerial, UsbSamba, Ethernet };
  Board(MainWindow *mw, PacketInterface *pi,  OscXmlServer *oxs, BoardType::Type type, QString key);
  ~Board();
  void sendMessage( QString rawMessage );
  void sendMessage( QList<OscMessage*> messageList );
  void sendMessage( QStringList messageList );
  void msgReceived( QByteArray msg);
  MainWindow* mainWindowRef() { return mainWindow; } // this is to the device list can have the context of the main window
  BoardType::Type type( ) { return _type; }
  QString key() { return _key; }
  QString location( );
  
  // System properties
  QString name, serialNumber, firmwareVersion, freeMemory;
  
  // Network properties
  QString ip_address, netMask, gateway, udp_listen_port, udp_send_port;
  bool dhcp, webserver;
  
signals:
  void msg(QString msg, MsgType::Type type, QString from);
  void msgs(QStringList msg, MsgType::Type type, QString from);
  void newBoardName(QString key, QString name);
  void newInfo(Board *board);
  
private:
  MainWindow *mainWindow;
  PacketInterface* packetInterface;
  Osc osc;
  OscXmlServer *oscXmlServer;
  QStringList messagesToPost;
  QTimer messagePostTimer;
  QString _key; 
  BoardType::Type _type;
  
  bool extractSystemInfoA( OscMessage* msg );
  bool extractSystemInfoB( OscMessage* msg );
  bool extractNetworkFind( OscMessage* msg );
};

#include "PacketInterface.h"

#endif /*BOARD_H_*/



