/*********************************************************************************

 Copyright 2008 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

#include "TestXmlServer.h"
#include "PacketUdp.h"
#include "OscXmlServer.h"

/*
  Set up the main window.  We'll add one fake board entry.
*/
void TestXmlServer::initTestCase()
{
  PacketUdp *udp = new PacketUdp(QHostAddress("192.168.0.123"), 10000);
  mainWindow->onEthernetDeviceArrived(udp);
  QVERIFY( mainWindow->deviceList->count() == 1 );
}

/*
  Connect to the OSC XML server, make sure it opens the connection OK, & sends a board update.
*/
void TestXmlServer::clientConnect()
{
  qRegisterMetaType< QList<Board*> >("QList<Board*>");
  QSignalSpy updateSpy(mainWindow->oscXmlServer, SIGNAL(boardListUpdated(QList<Board*>, bool)));
  QSignalSpy client1DataSpy(&xmlClient1, SIGNAL(readyRead()));
  QSettings settings("MakingThings", "mchelper");
  serverPort = settings.value("xml_listen_port", DEFAULT_XML_LISTEN_PORT).toInt();
  xmlClient1.connectToHost(QHostAddress::LocalHost, serverPort);
  int count = 0;
  while( xmlClient1.state() != QAbstractSocket::ConnectedState )
  {
    QTest::qWait(250);
    if(count++ > 10)
      QFAIL("couldn't connect to host.");
  }
  
  QCOMPARE(updateSpy.count(), 1 );
  QCOMPARE(client1DataSpy.count(), 1 );
  QList<QByteArray> newDocuments = xmlClient1.readAll( ).split( '\0' );
  foreach( QByteArray document, newDocuments )
  {
    if(!document.length())
      newDocuments.removeOne(document);
  }
  QCOMPARE(newDocuments.count(), 2); // should have the crossdomain.xml policy file and the list of connected boards
  QDomDocument doc;
  QVERIFY(doc.setContent(newDocuments.at(1)));
  //qDebug("%s", qPrintable(doc.toString(2)));
  QDomElement board = elementsByTagName("BOARD").item(0).toElement();
  QCOMPARE(board.attribute("LOCATION"), "192.168.0.123"); // make sure this is our fake board from above
}









