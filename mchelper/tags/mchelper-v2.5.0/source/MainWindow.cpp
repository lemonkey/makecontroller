/*********************************************************************************

 Copyright 2006-2009 MakingThings

 Licensed under the Apache License,
 Version 2.0 (the "License"); you may not use this file except in compliance
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.

*********************************************************************************/

#include <QSettings>
#include <QHostInfo>
#include <QTextStream>
#include <QDir>
#include <QFileDialog>
#include <QTextBlock>
#include <QTime>
#include <QDesktopServices>
#include <QUrl>
#include "MainWindow.h"
#include "PacketUsbSerial.h"

/**
  MainWindow represents, not surprisingly, the main window of the application.
  It handles all the menu items and the UI.
*/
MainWindow::MainWindow(bool no_ui) : QMainWindow( 0 )
{
  ui.setupUi(this);
  this->no_ui = no_ui;
  appUpdater = new AppUpdater();
  readSettings( );

  // add an item to the list as a UI cue that no boards were found.
  // remove when boards have been discovered
  deviceListPlaceholder = QListWidgetItem( tr("No Make Controllers found...") );
  deviceListPlaceholder.setData( Qt::ForegroundRole, Qt::gray );
  ui.deviceList->addItem( &deviceListPlaceholder );

  // default these to off until we see a board
  ui.actionUpload->setEnabled(false);
  ui.actionInspector->setEnabled(false);
  ui.actionResetBoard->setEnabled(false);
  ui.actionEraseBoard->setEnabled(false);

  grayText.setForeground( Qt::gray );
  blackText.setForeground( Qt::black );

  // initializations
  inspector = new Inspector(this);
  oscXmlServer = new OscXmlServer(this);
  usbMonitor = new UsbMonitor(this);
  networkMonitor = new NetworkMonitor(this);
  preferences = new Preferences(this, networkMonitor, oscXmlServer);
  uploader = new Uploader(this);
  about = new About();

  // device list connections
  connect( ui.deviceList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onDoubleClick()));
  connect( ui.deviceList, SIGNAL(itemSelectionChanged()), this, SLOT(onDeviceSelectionChanged()));

  // menu connections
  connect( ui.actionInspector, SIGNAL(triggered()), inspector, SLOT(loadAndShow()));
  connect( ui.actionPreferences, SIGNAL(triggered()), preferences, SLOT(loadAndShow()));
  connect( ui.actionUpload, SIGNAL(triggered()), uploader, SLOT(show()));
  connect( ui.actionClearConsole, SIGNAL(triggered()), ui.outputConsole, SLOT(clear()));
  connect( ui.actionAbout, SIGNAL(triggered()), about, SLOT(show()));
  connect( ui.actionResetBoard, SIGNAL(triggered()), this, SLOT(onDeviceResetRequest()));
  connect( ui.actionEraseBoard, SIGNAL(triggered()), this, SLOT(onEraseRequest()));
  connect( ui.actionHide_OSC, SIGNAL(triggered(bool)), this, SLOT(onHideOsc(bool)));
  connect( ui.actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(onCheckForUpdates()));
  connect( ui.actionHelp, SIGNAL(triggered()), this, SLOT(onHelp()));
  connect( ui.actionOscTutorial, SIGNAL(triggered()), this, SLOT(onOscTutorial()));

  // command line connections
  connect( ui.commandLine->lineEdit(), SIGNAL(returnPressed()), this, SLOT(onCommandLine()));
  connect( ui.sendButton, SIGNAL(clicked()), this, SLOT(onCommandLine()));

  #if !defined (Q_OS_WIN) && !defined (Q_OS_MAC)
  // the USB monitor runs in a separate thread...start it up.
  // only need to do this on non-Windows/OSX machines since automatic device detection is not implemented
  usbMonitor->start();
  #endif
}

void MainWindow::readSettings()
{
  QSettings settings;
  QSize size = settings.value( "size" ).toSize( );
  if( size.isValid( ) )
    resize( size );

  QPoint mainWinPos = settings.value("mainwindow_pos").toPoint();
  if(!mainWinPos.isNull())
    move(mainWinPos);

  QList<QVariant> splitterSettings = settings.value( "splitterSizes" ).toList( );
  if( !splitterSettings.isEmpty( ) ) {
    QList<int> splitterSizes;
    foreach( QVariant setting, splitterSettings )
      splitterSizes << setting.toInt( );
    ui.splitter->setSizes( splitterSizes );
  }

  QStringList commands = settings.value( "commands" ).toStringList( );
  foreach( QString cmd, commands )
    ui.commandLine->addItem(cmd);
  if(!ui.commandLine->count()) // always want to have a space on the end of our list of commands
    ui.commandLine->addItem( "" );
  else
    ui.commandLine->setCurrentIndex(ui.commandLine->count() - 1 );

  hideOscMsgs = settings.value("hideOscMsgs", false ).toBool();
  ui.actionHide_OSC->setChecked(hideOscMsgs);

  setMaxMessages(settings.value("max_messages", DEFAULT_ACTIVITY_MESSAGES).toInt( ));

  bool checkForUpdates = settings.value("checkForUpdatesOnStartup", DEFAULT_CHECK_UPDATES).toBool( );
  if( checkForUpdates )
    appUpdater->checkForUpdates(true);
}

void MainWindow::setMaxMessages(int msgs)
{
  ui.outputConsole->setMaximumBlockCount(msgs);
}

void MainWindow::writeSettings()
{
  QSettings settings;
  settings.setValue("size", size() );
  settings.setValue("mainwindow_pos", pos());
  settings.setValue("inspector_pos", inspector->pos());

  QList<QVariant> splitterSettings;
  QList<int> splitterSizes = ui.splitter->sizes();
  foreach( int splitterSize, splitterSizes )
    splitterSettings << splitterSize;
  settings.setValue("splitterSizes", splitterSettings );

  QStringList commands;
  for( int i = 0; i < ui.commandLine->count(); i++ )
    commands << ui.commandLine->itemText(i);
  settings.setValue("commands", commands );

  settings.setValue("hideOscMsgs", hideOscMsgs );
}

/*
 Called as the app is shutting down.
 Save the current app settings.
*/
void MainWindow::closeEvent( QCloseEvent *qcloseevent )
{
  writeSettings( );
  qcloseevent->accept();
  qApp->quit(); // in case the inspector or anything else is still open
}

#ifdef Q_OS_WIN
bool MainWindow::winEvent( MSG* msg, long* result )
{
  if ( msg->message == WM_DEVICECHANGE )
    usbMonitor->onDeviceChangeEventWin( msg->wParam, msg->lParam );
  return false;
}
#endif

/*
 Called back when we get a right click on the device list.
 If it's a SAMBA device, offer to upload firmware,
 otherwise just offer to view the inspector.
*/
void DeviceList::contextMenuEvent(QContextMenuEvent *event)
{
  Board *brd = (Board*)itemAt(event->pos());
  if(brd) {
    QMenu menu(this);
    MainWindow *mw = brd->mainWindowRef();
    if(brd->type() == BoardType::UsbSamba)
      menu.addAction(mw->uploadAction());
    else if(brd->type() == BoardType::Ethernet || brd->type() == BoardType::UsbSerial) {
      menu.addAction(mw->inspectorAction());
      menu.addAction(mw->resetAction());
      menu.addAction(mw->sambaAction());
    }
    menu.exec(event->globalPos());
  }
}

/*
  A board in the list has been double clicked.
  If it's a sam-ba board, bring up the uploader,
  otherwise bring up the inspector.
*/
void MainWindow::onDoubleClick()
{
  Board *brd = getCurrentBoard( );
  if( brd ) {
    if( brd->type() == BoardType::UsbSamba ) {
      if(!uploader->isVisible())
        uploader->show();
      uploader->raise();
      uploader->activateWindow();
    }
    else {
      if(!inspector->isVisible())
        inspector->loadAndShow();
      inspector->raise();
      inspector->activateWindow();
    }
  }
}

/*
 Called back when the selection in the device list changes.
 Update the inspector with the selected device's info.
 If the device isn't a SAM-BA device, disable the upload functionality
*/
void MainWindow::onDeviceSelectionChanged()
{
  Board *brd = getCurrentBoard( );
  if(brd) {
    if(brd->type() == BoardType::UsbSamba) {
      ui.actionUpload->setEnabled(true);
      ui.actionInspector->setEnabled(false);
      ui.actionResetBoard->setEnabled(false);
      ui.actionEraseBoard->setEnabled(false);
    }
    else {
      inspector->setData(brd);
      ui.actionInspector->setEnabled(true);
      ui.actionUpload->setEnabled(false);
      ui.actionResetBoard->setEnabled(true);
      ui.actionEraseBoard->setEnabled(true);
    }
  }
  else
    inspector->clear();
}

/*
 An Ethernet device has been discovered.
 Create an entry for it in the device list.
*/
void MainWindow::onEthernetDeviceArrived(PacketInterface* pi)
{
  QList<Board*> boardList;
  Board *board = new Board(this, pi, oscXmlServer, BoardType::Ethernet, pi->key());
  board->setText(pi->key());
  board->setIcon(QIcon(":/icons/network_icon.png"));
  board->setToolTip(tr("Ethernet Device: ") + pi->key());

  if(noUi()) {
    QTextStream out(stdout);
    out << tr("network device discovered: ") + pi->key() << endl;
  }

  boardInit(board);
  boardList.append(board);
  oscXmlServer->sendBoardListUpdate(boardList, true);
}

/*
  A USB device has arrived.  It could be a UsbSerial or a Samba device.
  Because the UsbMonitor runs in a separate thread, we want to
  create the packet interfaces here, in the main thread.
*/
void MainWindow::onUsbDeviceArrived(const QStringList & keys, BoardType::Type type)
{
  Board *board;
  QList<Board*> boardList;
  QString noUiString;
  foreach(QString key, keys) {
    if( type == BoardType::UsbSerial){
      PacketUsbSerial *usb = new PacketUsbSerial(key);
      board = new Board(this, usb, oscXmlServer, type, key);
      usb->open();
      board->setText(key);
      board->setIcon(QIcon(":/icons/usb_icon.png"));
      board->setToolTip(tr("USB Serial Device: ") + board->location());
      noUiString = tr("usb device discovered: ") + board->location();
    }
    else if(type == BoardType::UsbSamba) {
      board = new Board(this, 0, 0, type, key);
      board->setText(tr("Unprogrammed Board"));
      board->setIcon(QIcon(":/icons/usb_icon.png"));
      board->setToolTip(tr("Unprogrammed device"));
      noUiString = tr("sam-ba device discovered: ") + board->location();
    }

    if(noUi()) {
      QTextStream out(stdout);
      out << noUiString << endl;
    }

    boardInit(board);
    boardList.append(board);
  }
  oscXmlServer->sendBoardListUpdate(boardList, true);
}

/*
  Initialization common to any kind of board.
*/
void MainWindow::boardInit(Board *board)
{
  connect(board, SIGNAL(newInfo(Board*)), this, SLOT(updateBoardInfo(Board*)));
  // if the placeholder is in there, get rid of it
  int placeholderRow = ui.deviceList->row( &deviceListPlaceholder );
  if( placeholderRow >= 0 )
    ui.deviceList->takeItem( placeholderRow );
  ui.deviceList->addItem(board);
  // if no other boards are selected, select this new one
  if( !getCurrentBoard() )
    ui.deviceList->setCurrentRow (ui.deviceList->count()-1);
  board->sendMessage("/system/info-internal"); // get the board's info
  onDeviceSelectionChanged(); // make sure menus get updated
}

/*
  Called when a board's info has changed.
  Update the Inspector and the OSC XML server with the new info.
*/
void MainWindow::updateBoardInfo(Board *board)
{
  emit boardInfoUpdate(board);
  inspector->setData(board);
}

void MainWindow::onDeviceRemoved(const QString & key)
{
  QList<Board*> boards = getConnectedBoards();
  foreach(Board *board, boards) {
    if( board->key() == key ) {
      Board* brd = (Board*)ui.deviceList->takeItem(ui.deviceList->row(board));
      delete brd;
      // NOTE - brd->deleteLater() doesn't actually end up deleting the board here.  wtf?
      if(noUi()) {
        QTextStream out(stdout);
        out << tr("network device removed: ") + key;
      }
      break;
    }
  }
  // if no boards are left, put the placeholder back in
  if( !ui.deviceList->count( ) )
    ui.deviceList->addItem( &deviceListPlaceholder );
}

void MainWindow::message(const QStringList & msgs, MsgType::Type type, const QString & from)
{
  if( !messagesEnabled( type ) )
    return;
  QString currentTime = QTime::currentTime().toString();
  QTextBlockFormat format;
  format.setBackground(msgColor(type));
  QString tofrom("from");
  if(type == MsgType::Command)
    tofrom = "to";

  ui.outputConsole->setUpdatesEnabled(false);
  QString tf = QString("%1 %2").arg(tofrom).arg(from);
  foreach(QString msg, msgs)
    addMessage( currentTime, msg, tf, format);
  ui.outputConsole->setUpdatesEnabled(true);
}

void MainWindow::message(const QString & msg, MsgType::Type type, const QString & from)
{
  if(noUi()) {
    QTextStream out(stdout);
    out << from + ": " + msg << endl;
  }
  else {
    if( !messagesEnabled( type ) )
      return;
    QTextBlockFormat format;
    format.setBackground(msgColor(type));
    QString tofrom = tr("from");

    if(type == MsgType::Command)
      tofrom = tr("to");

    ui.outputConsole->setUpdatesEnabled(false);
    addMessage( QTime::currentTime().toString(), msg, QString("%1 %2").arg(tofrom).arg(from), format);
    ui.outputConsole->setUpdatesEnabled(true);
  }
}

void MainWindow::addMessage( const QString & time, const QString & msg,
                              const QString & tofrom, const QTextBlockFormat & bkgnd )
{
  ui.outputConsole->setCurrentCharFormat(grayText);
  ui.outputConsole->appendPlainText(time + "   ");
  ui.outputConsole->setCurrentCharFormat(blackText);
  ui.outputConsole->insertPlainText(msg);
  ui.outputConsole->setCurrentCharFormat(grayText);
  ui.outputConsole->insertPlainText(" " + tofrom);
  ui.outputConsole->textCursor().setBlockFormat(bkgnd);
}

bool MainWindow::messagesEnabled( MsgType::Type type )
{
  bool retval = true;
  if(type == MsgType::Command || type == MsgType::XMLMessage || type == MsgType::Response || type == MsgType::Warning) {
    if( hideOscMsgs )
      retval = false;
  }
  return retval;
}

void MainWindow::statusMsg(const QString & msg, int duration)
{
  statusBar()->showMessage(msg, duration);
}

QColor MainWindow::msgColor(MsgType::Type type)
{
  switch(type)
  {
    case MsgType::Warning:
      return QColor(255, 228, 118, 255); // orange
    case MsgType::Error:
      return QColor(255, 221, 221, 255); // red
    case MsgType::Notice:
      return QColor(235, 235, 235, 235); // light gray
    case MsgType::Response:
      return Qt::white;
    case MsgType::Command:
      return QColor(229, 237, 247, 255); // light blue
    case MsgType::XMLMessage:
      return QColor(219, 250, 224, 255); // green
    default:
      return Qt::white;
  }
}

void MainWindow::newXmlPacketReceived( const QList<OscMessage*> & msgs, const QString & destination )
{
  QList<Board*> boardList = getConnectedBoards( );
  foreach(Board *board, boardList) {
    if( board->key() == destination ) {
      board->sendMessage( msgs );
      break;
    }
  }
}

void MainWindow::setBoardName( const QString & key, const QString & name )
{
  QList<Board*> boardList = getConnectedBoards( );
  foreach(Board *board, boardList) {
    if( board->key() == key ) {
      ui.deviceList->item(ui.deviceList->row(board))->setText(name);
      break;
    }
  }
}

/*
  Return the currently selected board in the UI list of boards,
  or NULL if none are selected.
*/
Board* MainWindow::getCurrentBoard( )
{
  QListWidgetItem* item = ui.deviceList->currentItem( );
  if( item == &deviceListPlaceholder )
    item = 0;
  // sometimes the board can become unselected - it might not be the one that
  // was most recently selected, but give it a shot and grab the last one in this case
  else if(item == 0 && ui.deviceList->count() != 0) {
    ui.deviceList->setCurrentRow(ui.deviceList->count()-1);
    item = ui.deviceList->currentItem( );
  }
  return (Board*)item;
}

QList<Board*> MainWindow::getConnectedBoards( )
{
  QList<Board*> boardList;
  for( int i = 0; i < ui.deviceList->count( ); i++ ) {
    if( ui.deviceList->item( i ) != &deviceListPlaceholder )
      boardList.append( (Board*)ui.deviceList->item( i ) );
  }
  return boardList;
}

/*
 The user has pressed return in the command line or clicked the send button.
 Grab the text, print it to the screen and send the message to the currently
 selected board.
*/
void MainWindow::onCommandLine()
{
  QString cmd = ui.commandLine->currentText();
  Board* brd = getCurrentBoard();
  if(cmd.isEmpty() || brd == NULL)
    return;
  message(cmd, MsgType::Command, brd->location()); // print it to screen
  brd->sendMessage(cmd); // send it to the board

  // in order to get a readline-style history of commands via up/down arrows
  // we need to keep an empty item at the end of the list so we have a context from which to up-arrow
  if( ui.commandLine->count() >= ui.commandLine->maxCount() )
    ui.commandLine->removeItem( 0 );
  ui.commandLine->insertItem( ui.commandLine->count() - 1, cmd );
  ui.commandLine->setCurrentIndex( ui.commandLine->count() - 1 );
  ui.commandLine->clearEditText();
}

void MainWindow::onDeviceResetRequest()
{
  Board* brd = getCurrentBoard();
  if(brd) {
    brd->sendMessage("/system/reset 1");
    message (tr("Resetting Board"), MsgType::Notice, "mchelper");
  }
}

void MainWindow::onEraseRequest()
{
  Board* brd = getCurrentBoard();
  if(brd) {
    brd->sendMessage("/system/samba 1");
    message (tr("Erasing Board"), MsgType::Notice, "mchelper");
  }
}

void MainWindow::onHideOsc(bool checked)
{
  hideOscMsgs = checked;
}

void MainWindow::onCheckForUpdates(bool inBackground)
{
  appUpdater->checkForUpdates( inBackground );
}

void MainWindow::onHelp()
{
  if( !QDesktopServices::openUrl( QUrl("http://www.makingthings.com/documentation/tutorial/mchelper") ) )
    statusBar()->showMessage( tr("Help is online and requires an internet connection."), 3000 );
}

void MainWindow::onOscTutorial()
{
  if( !QDesktopServices::openUrl( QUrl("http://www.makingthings.com/documentation/tutorial/osc") ) )
    statusBar()->showMessage( tr("Help is online and requires an internet connection."), 3000 );
}















