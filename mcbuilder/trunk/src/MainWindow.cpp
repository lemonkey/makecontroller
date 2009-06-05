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


#include <QFileDialog>
#include <QSettings>
#include <QTextStream>
#include <QTextEdit>
#include <QDate>
#include <QDesktopServices>
#include <QDomDocument>
#include <QUrl>
#include <QMessageBox>
#include <QCloseEvent>
#include "MainWindow.h"

#define RECENT_FILES 5


/**
  MainWindow represents, not surprisingly, the main window of the application.
  It handles all the menu items and the UI.
*/
MainWindow::MainWindow( ) : QMainWindow( 0 )
{
  setupUi(this);

  // add the file dropdown to the toolbar...can't do this in Designer for some reason
  QWidget *stretch = new QWidget(toolBar);
  stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  toolBar->addWidget(stretch);
  currentFileDropDown = new QComboBox(toolBar);
  currentFileDropDown->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  toolBar->addWidget(currentFileDropDown);
  QWidget *pad = new QWidget(toolBar); // this doesn't pad as much as it should...to be fixed...
  toolBar->addWidget(pad);

  //initialization
  buildLog = new BuildLog();
  highlighter = new Highlighter( editor->document() );
  prefs = new Preferences(this);
  projInfo = new ProjectInfo(this);
  uploader = new Uploader(this);
  builder = new Builder(this, projInfo, buildLog);
  usbConsole = new UsbConsole();
  findReplace = new FindReplace(this);
  about = new About();
  updater = new AppUpdater();

  // load
  boardTypeGroup = new QActionGroup(menuBoard_Type);
  loadBoardProfiles( );
  loadExamples( );
  loadLibraries( );
  loadRecentProjects( );
  readSettings( );

  // misc. signals
  connect(editor, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorMoved()));
  connect(actionPreferences, SIGNAL(triggered()), prefs, SLOT(loadAndShow()));
  connect(actionUsb_Monitor, SIGNAL(triggered()), usbConsole, SLOT(loadAndShow()));
  connect(currentFileDropDown, SIGNAL(currentIndexChanged(int)), this, SLOT(onFileSelection(int)));
  connect(editor->document(), SIGNAL(contentsChanged()),this, SLOT(onDocumentModified()));
  connect(outputConsole, SIGNAL(itemDoubleClicked(QListWidgetItem*)),this, SLOT(onConsoleDoubleClick(QListWidgetItem*)));

  // menu actions
  connect(actionNew,					SIGNAL(triggered()), this,		SLOT(onNewFile()));
  connect(actionAdd_Existing_File, SIGNAL(triggered()), this,		SLOT(onAddExistingFile()));
  connect(actionNew_Project,	SIGNAL(triggered()), this,		SLOT(onNewProject()));
  connect(actionOpen,					SIGNAL(triggered()), this,		SLOT(onOpen()));
  connect(actionSave,					SIGNAL(triggered()), this,		SLOT(onSave()));
  connect(actionSave_As,			SIGNAL(triggered()), this,		SLOT(onSaveAs()));
  connect(actionBuild,				SIGNAL(triggered()), this,		SLOT(onBuild()));
  connect(actionStop,         SIGNAL(triggered()), this,		SLOT(onStop()));
  connect(actionClean,				SIGNAL(triggered()), this,		SLOT(onClean()));
  connect(actionProperties,		SIGNAL(triggered()), this,		SLOT(onProperties()));
  connect(actionUpload,				SIGNAL(triggered()), this,		SLOT(onUpload()));
  connect(actionUndo,					SIGNAL(triggered()), editor,	SLOT(undo()));
  connect(actionRedo,					SIGNAL(triggered()), editor,	SLOT(redo()));
  connect(actionCut,					SIGNAL(triggered()), editor,	SLOT(cut()));
  connect(actionCopy,					SIGNAL(triggered()), editor,	SLOT(copy()));
  connect(actionPaste,				SIGNAL(triggered()), editor,	SLOT(paste()));
  connect(actionSelect_All,		SIGNAL(triggered()), editor,	SLOT(selectAll()));
  connect(actionFind,         SIGNAL(triggered()), findReplace,	SLOT(show()));
  connect(actionAbout,        SIGNAL(triggered()), about,   SLOT(show()));
  connect(actionUpdate,        SIGNAL(triggered()), this,   SLOT(onUpdate()));
  connect(actionBuildLog, SIGNAL(triggered()), buildLog, SLOT(show()));
  connect(actionVisitForum,        SIGNAL(triggered()), this,   SLOT(onVisitForum()));
  connect(actionClear_Output_Console,		SIGNAL(triggered()), outputConsole,	SLOT(clear()));
  connect(actionUpload_File_to_Board,		SIGNAL(triggered()), this,	SLOT(onUploadFile()));
  connect(actionMake_Controller_Reference, SIGNAL(triggered()), this, SLOT(openMCReference()));
  connect(actionMcbuilder_User_Manual, SIGNAL(triggered()), this, SLOT(openManual()));
  connect(menuExamples, SIGNAL(triggered(QAction*)), this, SLOT(onExample(QAction*)));
  connect(menuLibraries, SIGNAL(triggered(QAction*)), this, SLOT(onLibrary(QAction*)));
  connect(actionSave_Project_As, SIGNAL(triggered()), this, SLOT(onSaveProjectAs()));
  connect(menuRecent_Projects, SIGNAL(triggered(QAction*)), this, SLOT(openRecentProject(QAction*)));
}

/*
  Restore the app to its state before it was shut down.
*/
void MainWindow::readSettings()
{
  QSettings settings("MakingThings", "mcbuilder");
  settings.beginGroup("MainWindow");

  QSize size = settings.value( "size" ).toSize( );
  if( size.isValid( ) )
    resize( size );

  QList<QVariant> splitterSettings = settings.value( "splitterSizes" ).toList( );
  QList<int> splitterSizes;
  if( !splitterSettings.isEmpty( ) )
  {
    for( int i = 0; i < splitterSettings.count( ); i++ )
      splitterSizes.append( splitterSettings.at(i).toInt( ) );
    splitter->setSizes( splitterSizes );
  }

  if(settings.value("checkForUpdates", true).toBool())
    updater->checkForUpdates(APPUPDATE_BACKGROUND);

  QString lastProject = settings.value("lastOpenProject").toString();
  if(!lastProject.isEmpty())
    openProject(lastProject);
  settings.endGroup();
  QPoint mainWinPos = settings.value("mainwindow_pos").toPoint();
  if(!mainWinPos.isNull())
    move(mainWinPos);
}

/*
  Save app settings.
*/
void MainWindow::writeSettings()
{
  QSettings settings("MakingThings", "mcbuilder");
  settings.beginGroup("MainWindow");
  settings.setValue("size", size() );
  QList<QVariant> splitterSettings;
  QList<int> splitterSizes = splitter->sizes();
  for( int i = 0; i < splitterSizes.count( ); i++ )
    splitterSettings.append( splitterSizes.at(i) );
  settings.setValue("splitterSizes", splitterSettings );
  settings.setValue("lastOpenProject", currentProject);
  settings.endGroup();
  settings.setValue("build_log_size", buildLog->size());
  settings.setValue("mainwindow_pos", pos());
}

/*
  The app is closing.
*/
void MainWindow::closeEvent( QCloseEvent *qcloseevent )
{
  if(!maybeSave( ))
    qcloseevent->ignore();
  else
  {
    writeSettings( );
    qcloseevent->accept();
  }
}

/*
  A key in the editor has been pressed.
  If it's a return or an enter, insert the spaces on the previous
  line so the cursor ends up in the same spot on the new line.
*/
void Editor::keyPressEvent(QKeyEvent* event)
{
  if(event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)
  {
    QTextCursor c = textCursor();
    QString whitespace;
    QString line = c.block().text();
    int count = 0;
    while(count < line.size() && line.at(count).isSpace())
      whitespace += line.at(count++);

    c.beginEditBlock();
    c.insertBlock();
    c.insertText(whitespace);
    c.endEditBlock();
  }
  else
    QPlainTextEdit::keyPressEvent(event); // call the base class implementation if we don't use the event
}

/*
  The cursor has moved.
  Highlight the current line, if appropriate.
  Update the line/column display in the status bar.
*/
void MainWindow::onCursorMoved( )
{
  QTextCursor c = editor->textCursor();
  if(c.hasSelection()) // don't highlight the line if text is selected
    return editor->setExtraSelections(QList<QTextEdit::ExtraSelection>());
  QTextEdit::ExtraSelection highlight;
  highlight.cursor = c;
  highlight.format.setProperty(QTextFormat::FullWidthSelection, true);
  highlight.format.setBackground( QColor::fromRgb(235, 235, 235) ); // light gray

  QList<QTextEdit::ExtraSelection> extras;
  extras << highlight;
  editor->setExtraSelections( extras );

  statusBar()->showMessage( tr("Line: %1  Column: %2").arg(c.blockNumber()+1).arg(c.columnNumber()));
}

/*
  The file in the editor has been changed.
  Set the window 'dirty' state to reflect the state of the file.
*/
void MainWindow::onDocumentModified( )
{
  setWindowModified(editor->document()->isModified());
}

/*
  Find text in the file currently open in the editor.
*/
bool MainWindow::findText(QString text, QTextDocument::FindFlags flags, bool forward)
{
  bool success = false;
  if(!editor->find(text, flags)) // if we didn't find it, try wrapping around
  {
    if(forward)
      editor->moveCursor(QTextCursor::Start);
    else
      editor->moveCursor(QTextCursor::End);

    if(editor->find(text, flags)) // now try again
      success = true;
  }
  else
    success = true;
  return success;
}

/*
  Replace all occurrences of the given text with the given replacement.
*/
void MainWindow::replaceAll(QString find, QString replace, QTextDocument::FindFlags flags)
{
  editor->textCursor().beginEditBlock(); // make sure all replaces are performed as a single edit step, for undos etc.
  bool keepgoing = true;
  editor->moveCursor(QTextCursor::Start);
  do
  {
    if(editor->find(find, flags))
      editor->textCursor().insertText(replace);
    else
      keepgoing = false;
  } while(keepgoing);
  editor->textCursor().endEditBlock();
}

/*
  Replace the selected text with the replace string.
  Presumably, text has been selected as the result of a find operation.
*/
void MainWindow::replace(QString rep)
{
  if(!editor->textCursor().selectedText().isEmpty())
    editor->textCursor().insertText(rep);
}

void MainWindow::setEditorFont(QString family, int pointSize)
{
  editor->setFont(QFont(family, pointSize));
}

void MainWindow::setTabWidth( int width )
{
  QFontMetrics fm(editor->font());
  editor->setTabStopWidth( fm.width(" ") * width );
}

/*
  Return the path to the file that contains the currently selected board's profile.
  This is stored in the data item of the actions in the "Board Type" menu
*/
QString MainWindow::currentBoardProfile( )
{
  QAction *board = boardTypeGroup->checkedAction( );
  if(board)
    return board->data().toString();
  else
    return QString();
}

/*
  Load a source file into the editor.
*/
void MainWindow::editorLoadFile( QString filepath )
{
  Q_ASSERT(!currentProject.isEmpty());
  QFile file(filepath);
  if(file.open(QIODevice::ReadOnly|QFile::Text))
  {
    currentFile = file.fileName();
    editor->setPlainText(file.readAll());
    file.close();
    editor->document()->setModified(false);
    setWindowModified(false);
  }
}

/*
  Called when the "new file" action is triggered.
  Prompt for a file name and create it within the currently open project.
*/
void MainWindow::onNewFile( )
{
  if(currentProject.isEmpty())
  {
    statusBar()->showMessage( tr("Need to open a project first.  Open or create a new one from the File menu."), 3500 );
    return;
  }
  QString newFilePath = QFileDialog::getSaveFileName(this, tr("Create New File"), currentProject, tr("C Files (*.c)"));
  if(!newFilePath.isNull()) // user cancelled
  {
    Q_ASSERT(!currentProject.isEmpty());
    QString newFile = projectManager.createNewFile(currentProject, newFilePath);
    if(!newFile.isEmpty())
    {
      QFileInfo fi(newFile);
      editorLoadFile(fi.filePath());
      currentFileDropDown->addItem(fi.fileName(), fi.filePath()); // store the filepath on the combo box item
      currentFileDropDown->setCurrentIndex(currentFileDropDown->count()-1);
    }
    else
    {
      // print error
    }
  }
}

/*
  Called when the "add existing file" action is triggered.
  Pop up a dialog to select the file to add.
*/
void MainWindow::onAddExistingFile( )
{
  if(currentProject.isEmpty())
  {
    statusBar()->showMessage( tr("Need to open a project first.  Open or create a new one from the File menu."), 3500 );
    return;
  }
  QString newFilePath = QFileDialog::getOpenFileName(this, tr("Add Existing File"), currentProject, tr("C Files (*.c)"));
  if(!newFilePath.isNull()) // user cancelled
  {
    if(projectManager.addToProjectFile(currentProject, QDir(currentProject).filePath(newFilePath), "thumb"))
    {
      editorLoadFile(newFilePath);
      QFileInfo fi(newFilePath);
      currentFileDropDown->addItem(fi.fileName(), fi.filePath());
      currentFileDropDown->setCurrentIndex(currentFileDropDown->count()-1);
    }
    else
    {
      // print error msg
    }
  }
}

/*
  A file has been removed from the project.
  Close it if it's open in the editor, and remove it from the list
  of project files in the dropdown.
*/
void MainWindow::removeFileFromProject(QString file)
{
  QFileInfo fi(file);
  int idx = currentFileDropDown->findData(fi.filePath());
  // remove the file from the dropdown
  if(idx > -1)
    currentFileDropDown->removeItem(idx);
  // if it's currently loaded in the editor, get it out
  if(file == currentFile)
  {
    QString newfile = currentFileDropDown->itemData(currentFileDropDown->currentIndex()).toString();
    if(!newfile.isEmpty())
      editorLoadFile(newfile);
    else
    {
      currentFile = "";
      editor->clear();
      setWindowModified(false);
    }
  }
}

/*
  Create a new source file.
  Fill it with some default text,
  add it to the file dropdown and load it into the editor.
*/
void MainWindow::createNewFile(QString path)
{
  QFileInfo fi(path);
  if(fi.suffix().isEmpty())
    fi.setFile(fi.filePath() + ".c");
  QFile file(fi.filePath());

  if(file.exists()) // don't do anything if this file's already there
    return;
  if(file.open(QIODevice::WriteOnly | QFile::Text))
  {
    QTextStream out(&file);
    out << QString("// %1").arg(fi.fileName()) << endl;
    out << tr("// created %1").arg(QDate::currentDate().toString("MMM d, yyyy") ) << endl << endl;
    file.close();
    editorLoadFile(fi.filePath());
    currentFileDropDown->addItem(fi.fileName(), fi.filePath()); // store the filepath on the combo box item
    currentFileDropDown->setCurrentIndex(currentFileDropDown->count()-1);
    projectManager.addToProjectFile(currentProject, fi.filePath(), "thumb");
  }
}

/*
  A new file has been selected.
  Load it into the editor.
*/
void MainWindow::onFileSelection(int index)
{
  if(index < 0) // the list has just been cleared...no use grabbing anything
    return;
  Q_ASSERT(!currentProject.isEmpty()); // we shouldn't have any files loaded unless a project is open
  QFileInfo file(currentFileDropDown->itemData(index).toString());
  if(file.exists())
    editorLoadFile(file.filePath());
  else
  {
    QString message = tr("Couldn't find %1.").arg(file.fileName());
    currentFileDropDown->removeItem(currentFileDropDown->findText(file.fileName()));
    statusBar()->showMessage(message, 3000);
    // should also be removed from the project file...
  }
}

/*
  Called when the "new project" action is triggered.
  Create a new project directory and project file within it.
*/
void MainWindow::onNewProject( )
{
  QString workspace = Preferences::workspace();
  QString newProjPath = QFileDialog::getSaveFileName(this, tr("Create Project"), workspace, "", 0, QFileDialog::ShowDirsOnly);
  if(!newProjPath.isNull())
  {
    QString newProject = projectManager.createNewProject(newProjPath);
    if(!newProject.isEmpty())
      openProject(newProject);
    else
      return statusBar()->showMessage(tr("Couldn't create new project.  Make sure there are no spaces in the path specified."), 3000);
  }
}

/*
  Called when the "open" action is triggered.
  Prompt the user for which project they'd like to open.
*/
void MainWindow::onOpen( )
{
  QString projectPath = QFileDialog::getExistingDirectory(this, tr("Open Project"),
                                          Preferences::workspace(), QFileDialog::ShowDirsOnly);
  if( !projectPath.isNull() ) // user cancelled
    openProject(projectPath);
}

/*
  Open an existing project.
  Read the project file and load the appropriate files into the UI.
*/
void MainWindow::openProject(QString projectPath)
{
  QDir projectDir(projectPath);
  QString projectName = projectDir.dirName();
  if(!projectDir.exists())
    return statusBar()->showMessage( tr("Couldn't find %1.").arg(projectName), 3500 );

  QString pathname = projectName; // filename should not have spaces
  QFile projFile(projectDir.filePath(pathname + ".xml"));
  QDomDocument doc;
  if(doc.setContent(&projFile))
  {
    currentProject = projectPath;
    currentFileDropDown->clear();
    QDomNodeList allFiles = doc.elementsByTagName("files").at(0).childNodes();
    for(int i = 0; i < allFiles.count(); i++)
    {
      QFileInfo fi(allFiles.at(i).toElement().text());
      if(fi.fileName().isEmpty())
        continue;
      // load the file name into the file dropdown
      if(projectDir.exists(fi.fileName()))
      {
        if(QDir::isAbsolutePath(fi.filePath()))
          currentFileDropDown->addItem(fi.fileName(), fi.filePath());
        else
          currentFileDropDown->addItem(fi.fileName(), projectDir.filePath(fi.filePath()));

        // if this is the main project file, load it into the editor
        if(fi.baseName() == pathname)
        {
          editorLoadFile(projectDir.filePath(fi.filePath()));
          currentFileDropDown->setCurrentIndex(currentFileDropDown->findText(fi.fileName()));
        }
      }
    }
    setWindowTitle( projectName + "[*] - mcbuilder");
    updateRecentProjects(projectPath);
    // diff projects before loading the new one in
    bool clean = projInfo->diffProjects(projectPath);
    projInfo->load(projectPath); // but load the new one in before we clean, so the proper config and Makefiles are created
    if(clean)
      builder->clean(projectPath);
    buildLog->clear();
  }
  else
    return statusBar()->showMessage( tr("Couldn't find main file for %1.").arg(projectName), 3500 );
}

/*
  A new project has been opened.
  If this project isn't already in our recent projects list, stick it in there.
*/
void MainWindow::updateRecentProjects(QString newProject)
{
  QList<QAction*> recentProjects = menuRecent_Projects->actions();
  QStringList recentProjectPaths;
  foreach(QAction* a, recentProjects)
    recentProjectPaths << a->data().toString();
  if(!recentProjectPaths.contains(newProject)) // only need to update if this project is not already included
  {
    if(recentProjects.count() >= RECENT_FILES ) // make room in the list if we need to
    {
      menuRecent_Projects->removeAction(recentProjects.first());
      recentProjectPaths.removeAll(newProject);
    }

    QDir dir(newProject); // make the new action, and add it to the menu
    QAction* action = new QAction(dir.dirName(), menuRecent_Projects);
    action->setData(newProject);
    menuRecent_Projects->addAction(action);
    recentProjectPaths.append(newProject);

    QSettings settings("MakingThings", "mcbuilder");
    settings.setValue("recentProjects", recentProjectPaths);
  }
}

void MainWindow::openRecentProject(QAction* project)
{
  openProject(project->data().toString());
}

/*
  Called when the "save" action is triggered.
*/
void MainWindow::onSave( )
{
  if(currentFile.isEmpty())
    return statusBar()->showMessage( tr("Need to open a file or project first.  Open or create a new one from the File menu."), 3500 );
  save( );
}

/*
  Save the file currently in the editor.
*/
bool MainWindow::save( )
{
  QFile file(currentFile);
  if(file.open(QFile::WriteOnly | QFile::Text))
  {
    QTextStream out(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << editor->toPlainText();
    QApplication::restoreOverrideCursor();
    editor->document()->setModified(false);
    setWindowModified(false);
    return true;
  }
  else
  {
    statusBar()->showMessage( tr("Couldn't save...maybe the current file has been moved or deleted."), 3500 );
    return false;
  }
}

/*
  Prompt the user to save the current file, if necessary.
  Done before building, or closing, etc.
*/
bool MainWindow::maybeSave()
{
  if(editor->document()->isModified())
  {
    QMessageBox::StandardButton ret;
    ret = QMessageBox::warning(this, tr("mcbuilder"),
                               tr("This document has been modified.\n"
                                  "Do you want to save your changes?"),
                               QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)
      return save();
    else if (ret == QMessageBox::Cancel)
      return false;
  }
  return true;
}

/*
  Called when the "save as" action has been triggered.
  Create a new file with the contents of the current one,
  rename it and load it into the editor.
*/
void MainWindow::onSaveAs( )
{
  if(currentFile.isEmpty())
    return statusBar()->showMessage( tr("Need to open a project first.  Open or create a new one from the File menu."), 3500 );

  QString newFileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                      currentProject, tr("C Files (*.c)"));
  if(!newFileName.isNull()) // user cancelled
  {
    QString newFile = projectManager.saveFileAs(currentProject, currentFile, newFileName);
    if(!newFile.isEmpty())
    {
      QFileInfo fi(newFile);
      editorLoadFile(newFile);
      currentFileDropDown->addItem(fi.fileName(), fi.filePath());
      currentFileDropDown->setCurrentIndex(currentFileDropDown->findText(fi.fileName()));
    }
    else
    {
      // print error message
    }
  }
}

/*
  The user has triggered the "save project as" action.
  Pop up a dialog to get the new project's name, then save a copy.
*/
void MainWindow::onSaveProjectAs( )
{
  if(currentProject.isEmpty())
    return statusBar()->showMessage( tr("Need to open a project first.  Open or create a new one from the File menu."), 3500 );

  QString workspace = Preferences::workspace();
  QString newProjectPath = QFileDialog::getSaveFileName(this, tr("Save Project As"), workspace,
                                                        "", 0, QFileDialog::ShowDirsOnly);
  if(!newProjectPath.isNull()) // user canceled
  {
    QString newProject = projectManager.saveCurrentProjectAs(currentProject, newProjectPath);
    if(!newProject.isEmpty())
      openProject(newProject);
    else
     return statusBar()->showMessage( tr("Couldn't create the new project.  Maybe there's a problem with the current project?"), 3500 );
  }
}

/*
  Called when the build action is triggered.
  Check to see if we need to save the current file,
  then try to fire off the build process.
*/
void MainWindow::onBuild( )
{
  if(currentProject.isEmpty())
    return statusBar()->showMessage( tr("Open a project to build, or create a new one from the File menu."), 3500 );
  if(!maybeSave( ))
    return;
  if(builder->state() == QProcess::NotRunning)
  {
    outputConsole->clear();
    actionStop->setEnabled(true);
    builder->build(currentProject);
  }
  else
    return statusBar()->showMessage( tr("Builder is currently busy...give it a second, then try again."), 3500 );
}

/*
  The 'stop' action has been triggered.
  Cancel the build process.
*/
void MainWindow::onStop( )
{
  builder->stop();
}

/*
  The build has completed.
  Add a line to the output console indicating success or failure,
  and set the status bar as well.
*/
void MainWindow::onBuildComplete(bool success)
{
  if(success)
  {
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/success.png"), tr("Build succeeded."), outputConsole));
    outputConsole->scrollToBottom();
    statusBar()->showMessage(tr("Build succeeded."));
  }
  else
  {
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/error.png"), tr("Build failed."), outputConsole));
    outputConsole->scrollToBottom();
    statusBar()->showMessage(tr("Build failed."));
  }
  actionStop->setEnabled(false);
}

void MainWindow::onUploadComplete(bool success)
{
  if(success)
  {
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/success.png"), tr("Upload succeeded."), outputConsole));
    outputConsole->scrollToBottom();
    statusBar()->showMessage(tr("Upload succeeded."));
  }
  else
  {
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/error.png"), tr("Upload failed."), outputConsole));
    outputConsole->scrollToBottom();
    statusBar()->showMessage(tr("Upload failed."));
  }
}

void MainWindow::onCleanComplete()
{
  outputConsole->clear();
  outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/success.png"), tr("Clean succeeded."), outputConsole));
  statusBar()->showMessage(tr("Clean succeeded."));
}

void MainWindow::buildingNow(QString file)
{
  statusBar()->showMessage(tr("Building...") + file);
}

/*
  Called when the "clean" action is triggered.
  Fire off the clean process in the builder.
*/
void MainWindow::onClean( )
{
  if(currentProject.isEmpty())
    return;
  if(builder->state() == QProcess::NotRunning)
    builder->clean(currentProject);
  else
    return statusBar()->showMessage( tr("Builder is currently busy...give it a second, then try again."), 3500 );
}

/*
  Called when the "project properties" action is triggered.
  Pop up the properties dialog.
*/
void MainWindow::onProperties( )
{
  if(currentProject.isEmpty())
    return statusBar()->showMessage( tr("Open a project first, or create a new one from the File menu."), 3500 );
  if( !projInfo->load(currentProject) )
  {
    QDir dir(currentProject);
    return statusBar()->showMessage( tr("Couldn't find/open project properties for ") + dir.dirName(), 3500 );
  }
  else
    projInfo->show();
}

/*
  Called when the "upload" action is triggered.
  Build the current project and upload the resulting .bin to the board.
*/
void MainWindow::onUpload( )
{
  if(currentProject.isEmpty())
    return statusBar()->showMessage( tr("Open a project to upload, or create a new one from the File menu."), 3500 );
  QDir projectDir(currentProject);
  projectDir.cd("build");
  projectDir.setNameFilters(QStringList() << "*.bin");
  QFileInfoList bins = projectDir.entryInfoList();
  if(bins.count())
    uploadFile(bins.first().filePath());
  else
    return statusBar()->showMessage( tr("Couldn't find the file to upload for this project."), 3500 );
}

/*
  Called when the "upload file" action is triggered.
  Pop up a file dialog, and try to upload the given file
  to the board.  This is only for uploading pre-built .bins.
*/
void MainWindow::onUploadFile( )
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"),
                                                 QDir::homePath(), tr("Binaries (*.bin)"));
  if(!fileName.isNull())
    uploadFile(fileName);
}

/*
  Actually upload the file to the board.
  Pass in the board config file name to upload a project to that kind of board
*/
void MainWindow::uploadFile(QString filename)
{
  // get the board type so the uploader can check which upload mechanism to use
  QFileInfo fi(filename);
  if(!fi.exists())
    return statusBar()->showMessage( tr("Couldn't find %1.").arg(fi.fileName()), 3500 );
  QAction *board = boardTypeGroup->checkedAction( );
  if(board)
  {
    if(uploader->state() == QProcess::NotRunning)
      uploader->upload(board->data().toString(), filename);
    else
      return statusBar()->showMessage( tr("Uploader is currently busy...give it a second, then try again."), 3500 );
  }
  else
    return statusBar()->showMessage( tr("Please select a board type from the Project menu first."), 3500 );
}

// read the available board files and load them into the UI
void MainWindow::loadBoardProfiles( )
{
  QDir dir = QDir::current().filePath("resources/board_profiles");
  QStringList boardProfiles = dir.entryList(QStringList("*.xml"));
  QDomDocument doc;
  // get a list of the names of the actions we already have
  QList<QAction*> boardActions = menuBoard_Type->actions( );
  QStringList actionNames;
  foreach(QAction *a, boardActions)
    actionNames << a->text();

  foreach(QString filename, boardProfiles)
  {
    QFile file(dir.filePath(filename));
    if(file.open(QIODevice::ReadOnly))
    {
      if(doc.setContent(&file))
      {
        QString boardName = doc.elementsByTagName("name").at(0).toElement().text();
        if(!actionNames.contains(boardName))
        {
          QAction *boardAction = new QAction(boardName, this);
          boardAction->setCheckable(true);
          if(boardName == Preferences::boardType( ))
            boardAction->setChecked(true);
          boardAction->setData(filename);         // hang onto the filename so we don't have to look it up again later
          menuBoard_Type->addAction(boardAction); // add the action to the actual menu
          boardTypeGroup->addAction(boardAction); // and to the group that maintains an exclusive selection within the menu
        }
      }
      file.close();
    }
  }
}

/*
  Navigate the examples directory and create menu items
  for each of the available examples.
*/
void MainWindow::loadExamples( )
{
  QDir dir = QDir::current().filePath("resources/examples");
  QStringList exampleCategories = dir.entryList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
  foreach(QString category, exampleCategories)
  {
    QMenu *exampleMenu = new QMenu(category, this);
    menuExamples->addMenu(exampleMenu);
    QDir exampleDir(dir.filePath(category));
    QFileInfoList examples = exampleDir.entryInfoList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QFileInfo example, examples)
    {
      QAction *a = new QAction(example.baseName(), exampleMenu); // use the base name for the name in the menu
      a->setData(example.filePath()); // store the path in the action's data
      exampleMenu->addAction(a);
    }
  }
}

/*
  Called when an action in the Examples menu is triggered.
  Find the example's project and try to open it.
*/
void MainWindow::onExample(QAction *example)
{
  openProject(example->data().toString());
}

/*
  Navigate the libraries directory and load the available ones into the UI.
  If the library provides a "friendly name", use that in the dropdown of libraries
  in the UI, and always store the actual library dir name in the action's data
*/
void MainWindow::loadLibraries( )
{
  QDir dir = QDir::current().filePath("cores/makecontroller/libraries");
  if(dir.exists())
  {
    QStringList libraries = dir.entryList(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot);
    foreach(QString library, libraries)
    {
      QDir libdir = dir.filePath(library);
      QFile libfile(libdir.filePath(library + ".xml"));
      QDomDocument doc;
      if(doc.setContent(&libfile))
      {
        libfile.close();
        // add the library to the "Import Library" menu
        QString libname = library;
        QDomNodeList nodes = doc.elementsByTagName("display_name");
        if(nodes.count())
          libname = nodes.at(0).toElement().text();

        // create a menu that allows us to import the library, view its documentation, etc
        QMenu *menu = new QMenu(libname, menuLibraries);
        menuLibraries->addMenu(menu);
        QAction *a = new QAction(tr("Import to Current Project"), menu);
        a->setData(library);
        menu->addAction(a);

        nodes = doc.elementsByTagName("reference");
        if(nodes.count())
        {
          a = new QAction(tr("View Documentation"), menu);
          QString doclink = nodes.at(0).toElement().text();
          QUrl url(doclink);
          if(url.isRelative()) // if it's relative, we need to store the path for context
            a->setData(QDir::cleanPath(libdir.filePath(doclink)));
          else
            a->setData(doclink); // otherwise just store the link
          menu->addAction(a);
        }
        // TODO add the library's examples to the example menu
      }
    }
  }
}

/*
  Called when an item in the Libraries menu is triggered.
  Add a #include into the current document for the selected library
*/
void MainWindow::onLibrary(QAction *example)
{
  if(example->text() == tr("Import to Current Project"))
  {
    QString includeString = QString("#include \"%1.h\"").arg(example->data().toString());
    // only add if it isn't already in there
    // find() moves the cursor and highlights the found text
    if(!editor->find(includeString) && !editor->find(includeString, QTextDocument::FindBackward))
    {
      editor->moveCursor(QTextCursor::Start);
      editor->insertPlainText(includeString + "\n");
    }
  }
  else if(example->text() == tr("View Documentation"))
    QDesktopServices::openUrl(QUrl::fromLocalFile(example->data().toString()));
}

/*
  Populate the recent projects menu.
*/
void MainWindow::loadRecentProjects( )
{
  QSettings settings("MakingThings", "mcbuilder");
  QStringList projects = settings.value("recentProjects").toStringList();
  projects = projects.mid(0,RECENT_FILES); // just in case there are extras
  foreach(QString project, projects)
  {
    QDir dir(project);
    QAction* a = new QAction(dir.dirName(), menuRecent_Projects); // set the project name as the text
    a->setData(project); // store the full path in data
    menuRecent_Projects->addAction(a);
  }
}

void MainWindow::printOutput(QString text)
{
  outputConsole->addItem(text.trimmed());
  outputConsole->scrollToBottom();
}

void MainWindow::printOutputError(QString text)
{
  if(text.startsWith(tr("Warning")))
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/warning.png"), text.trimmed(), outputConsole));
  else if(text.startsWith(tr("Error")))
    outputConsole->addItem(new QListWidgetItem(QIcon(":/icons/error.png"), text.trimmed(), outputConsole));
  else
    outputConsole->addItem(text.trimmed());
  outputConsole->scrollToBottom();
}

void MainWindow::printOutputError(ConsoleItem *item)
{
  outputConsole->addItem(item);
}

/*
  An item in the output console has been double clicked.
  See if it's an error/warning message, and jump to it in the editor if we can.
*/
void MainWindow::onConsoleDoubleClick(QListWidgetItem *item)
{
  ConsoleItem *citem = dynamic_cast<ConsoleItem*>(item);
  if(citem)
    highlightLine(citem->filePath(), citem->lineNumber(), citem->messageType());
}

/*
  Highlight a line in the editor to indicate either an error or warning.
*/
void MainWindow::highlightLine(QString filepath, int linenumber, ConsoleItem::Type type)
{
  if(QDir::toNativeSeparators(filepath) == QDir::toNativeSeparators(currentFile))
  {
    QTextCursor c(editor->document());
    c.movePosition(QTextCursor::Start);
    while(c.blockNumber() < linenumber-1) // blockNumber is 0 based, lineNumber starts at 1
      c.movePosition(QTextCursor::NextBlock);

    QTextEdit::ExtraSelection es;
    es.cursor = c;
    es.format.setProperty(QTextFormat::FullWidthSelection, true);
    if(type == ConsoleItem::Error)
      es.format.setBackground(QColor("#ED575D")); // light red
    else
      es.format.setBackground(QColor("#FFDE49")); // light yellow

    editor->setExtraSelections( editor->extraSelections() << es );
  }
}

/*
  Open the Make Controller firmware API reference.
*/
void MainWindow::openMCReference( )
{
  QString ref = QDir::current().filePath("resources/reference/makecontroller/html/index.html");
  QDesktopServices::openUrl(QUrl::fromLocalFile(ref));
}

/*
  Open the PDF user manual for mcbuilder.
*/
void MainWindow::openManual( )
{
  QString manual = QDir::current().filePath("resources/reference/manual.pdf");
  QDesktopServices::openUrl(QUrl::fromLocalFile(manual));
}

/*
  The user has clicked "check for updates".
*/
void MainWindow::onUpdate()
{
  updater->checkForUpdates(APPUPDATE_FOREGROUND);
}

void MainWindow::onVisitForum()
{
  QDesktopServices::openUrl(QUrl("http://www.makingthings.com/forum"));
}






