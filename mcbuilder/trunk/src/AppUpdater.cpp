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

/*
  Some code in here modified from Thomas Keller's guitone project
  http://guitone.thomaskeller.biz
  Author - Thomas Keller, me@thomaskeller.biz
*/

#include "AppUpdater.h"

#include <QDomDocument>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>

AppUpdater::AppUpdater( ) : QDialog( )
{
  setModal( true );
  setWindowTitle( tr("Software Update") );

  acceptButton.setDefault( true );
  ignoreButton.setText( tr("Not Right Now") );

  buttonLayout.addStretch( );
  buttonLayout.addWidget( &acceptButton );

  mcbuilderIcon.load( ":icons/mcbuilder96.png" );
  icon.setPixmap( mcbuilderIcon );
  icon.setAlignment( Qt::AlignHCenter );

  headline.setWordWrap( false );
  details.setWordWrap( false );
  browser.setReadOnly( true );

  textLayout.addWidget( &headline );
  textLayout.addWidget( &details );
  textLayout.addLayout( &buttonLayout );
  topLevelLayout.addWidget( &icon );
  topLevelLayout.addLayout( &textLayout );
  topLevelLayout.setAlignment( Qt::AlignHCenter );

  this->setLayout( &topLevelLayout );
  checkingOnStartup = true; // hide the dialog by default
  connect( &http, SIGNAL( requestFinished( int, bool ) ), this, SLOT( finishedRead( int, bool ) ) );
}

void AppUpdater::checkForUpdates( bool inBackground )
{
  checkingOnStartup = inBackground;
  http.setHost("www.makingthings.com");
  httpGetID = http.get("/updates/mcbuilder.xml");
}


void AppUpdater::finishedRead( int id, bool errors )
{
  (void)errors;
  // we'll get called here alternately by the setHost( ) request and the actual GET request
  // we don't care about setHost, so just return and wait for the GET response
  if( id != httpGetID )
    return;

  QDomDocument doc;
  QString err;
  int line, col;

  if (!doc.setContent(http.readAll(), true, &err, &line, &col))
  {
    headline.setText( tr("<font size=4>Couldn't contact the update server...</font>") );
    details.setText( tr( "Make sure you're connected to the internet." ) );
    acceptButton.setText( tr("OK") );
    acceptButton.disconnect( ); // make sure it wasn't connected by anything else previously
    connect( &acceptButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    removeBrowserAndIgnoreButton( );

    if(!checkingOnStartup)
      this->show( );
    return;
  }

  QDomElement channel = doc.documentElement().firstChild().toElement();
  QDomNodeList items = channel.elementsByTagName("item");
  QPair<QString, QString> latest(MCBUILDER_VERSION, "");
  bool updateAvailable = false;

  for (int i=0, j=items.size(); i<j; i++)
  {
    QDomElement item = items.item(i).toElement();
    if( item.isNull() )
      continue;
    QDomNodeList enclosures = item.elementsByTagName("enclosure");

    for (int k=0, l=enclosures.size(); k<l; k++)
    {
      QDomElement enclosure = enclosures.item(k).toElement();
      if (enclosure.isNull()) continue;
      QString version = enclosure.attributeNS(
        "http://www.andymatuschak.org/xml-namespaces/sparkle", "version", "not-found" );

      // each item can have multiple enclosures, of which at least one
      // should have a version field
      if (version == "not-found") continue;

      if( versionCompare(version, latest.first) > 0 )
      {
        latest.first = version;
        QDomNodeList descs = item.elementsByTagName("description");
        //I(descs.size() == 1);
        QDomElement desc = descs.item(0).toElement();
        //I(!desc.isNull());
        latest.second = desc.text();
        updateAvailable = true;
      }
    }
  }

  // add the appropriate elements/info depending on whether an update is available
  if( updateAvailable )
  {
    headline.setText( tr("<font size=4>A new version of mcbuilder is available!</font>" ));
    QString d = tr( "mcbuilder %1 is now available (you have %2).  Would you like to download it?" )
                          .arg(latest.first).arg( MCBUILDER_VERSION );
    details.setText( d );
    browser.setHtml( latest.second );
    acceptButton.setText( tr("Visit Download Page") );
    acceptButton.disconnect( );
    ignoreButton.disconnect( );
    connect( &acceptButton, SIGNAL( clicked() ), this, SLOT( visitDownloadsPage() ) );
    connect( &ignoreButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    if( textLayout.indexOf( &browser ) < 0 ) // if the browser's not in the layout, then insert it after the details line
      textLayout.insertWidget( textLayout.indexOf( &details ) + 1, &browser );
    if( buttonLayout.indexOf( &ignoreButton ) < 0 ) // put the ignore button on the left
      buttonLayout.insertWidget( 0, &ignoreButton );

    this->show( );
  }
  else
  {
    headline.setText( tr("<font size=4>You're up to date!</font>") );
    details.setText( tr( "You're running the latest version of mcbuilder, version %1." ).arg( MCBUILDER_VERSION ) );
    acceptButton.setText( tr("OK") );
    acceptButton.disconnect( );
    connect( &acceptButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    removeBrowserAndIgnoreButton( );
    if(!checkingOnStartup)
      this->show( );
  }
}

void AppUpdater::removeBrowserAndIgnoreButton( )
{
  if( textLayout.indexOf( &browser ) >= 0 ) // if the browser's in the layout, rip it out
    textLayout.removeWidget( &browser );
  browser.setParent( NULL );

  if( textLayout.indexOf( &ignoreButton ) >= 0 ) // if the ignoreButton's in the layout, rip it out
    textLayout.removeWidget( &ignoreButton );
  ignoreButton.setParent( NULL );
}

int AppUpdater::versionCompare(const QString & left, const QString & right)
{
    QStringList leftParts = left.split(".");
    QStringList rightParts = right.split(".");

    int leftCount = leftParts.size();
    int rightCount = rightParts.size();
    int maxCount = leftCount > rightCount ? leftCount : rightCount;

    for (int i=0, j=maxCount; i<j; i++)
    {
        unsigned int l = 0;
        if (i < leftCount) { l = leftParts.at(i).toUInt(); }
        unsigned int r = 0;
        if (i < rightCount) { r = rightParts.at(i).toUInt(); }

        if (l == r) continue;
        if (l > r) return 1;
        return -1;
    }

    return 0;
}

void AppUpdater::visitDownloadsPage( )
{
  QDesktopServices::openUrl( QUrl( "http://www.makingthings.com/resources/downloads" ) );
  accept( );
}



