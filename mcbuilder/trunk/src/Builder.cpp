
#include <QDir>
#include <QTextStream>
#include <QDate>
#include "Builder.h"

/*
	Builder takes a project and turns it into a binary executable.
	We need to wrap the project into a class, and generate a Makefile
	based on the general Preferences and Properties for this project.
*/
Builder::Builder(MainWindow *mainWindow) : QProcess( 0 )
{
	this->mainWindow = mainWindow;
	connect(this, SIGNAL(readyReadStandardOutput()), this, SLOT(readOutput()));
	connect(this, SIGNAL(readyReadStandardError()), this, SLOT(readError()));
	connect(this, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(nextStep(int, QProcess::ExitStatus)));
  connect(this, SIGNAL(error(QProcess::ProcessError)), this, SLOT(onBuildError(QProcess::ProcessError)));
}

/*
	Prepare the build process:
	- wrap each project file in a class
	- generate lists of C and C++ files to be compiled
	Then fire it off.
*/
void Builder::build(QString projectName)
{
  ensureBuildDirExists(projectName);
  createMakefile(projectName);
  buildStep = BUILD;
  currentProjectPath = projectName;
  setWorkingDirectory(projectName + "/build");
  start("make");
}

/*
  Remove all the object files from the build directory.
*/
void Builder::clean(QString projectName)
{
  ensureBuildDirExists(projectName);
  buildStep = CLEAN;
  currentProjectPath = projectName;
  setWorkingDirectory(projectName + "/build");
  QStringList args = QStringList() << "clean";
  start("make", args);
}

void Builder::ensureBuildDirExists(QString projPath)
{
  QDir dir(projPath);
  if(!dir.exists(projPath+"/build"))
    dir.mkdir("build");
}

void Builder::sizer()
{
  buildStep = SIZER;
  setWorkingDirectory(currentProjectPath + "/build");
  QStringList args = QStringList() << "heavy.elf";
  start("arm-elf-size", args);
}

void Builder::wrapFile(QString filePath)
{
  QDir dir(filePath);
	QFile project(dir.filePath(dir.dirName() + ".cpp"));
	QFile file(dir.filePath("temp.cpp"));
	if(file.open(QIODevice::WriteOnly | QFile::Text) && project.open(QIODevice::ReadOnly | QFile::Text))
	{
		QTextStream out(&file);
		out << QString("class ") + dir.dirName() << endl << "{" << endl;
		out << "  public:" << endl;
		
		QTextStream in(&project);
		QString line = in.readLine();
		while (!line.isNull())
		{
			line.append("\n");
			out << line.prepend("  ");
			line = in.readLine();
		}
		
		out << "};" << endl;
		file.close();
	}
}

/*
	This handles the end of each step of the build process.  Maintain which state we're
	in and fire off the next process as appropriate.
*/
void Builder::nextStep( int exitCode, QProcess::ExitStatus exitStatus )
{
	if( exitCode != 0 || exitStatus != QProcess::NormalExit) // something didn't finish happily
	{
		mainWindow->onBuildComplete(false);
    resetBuildProcess();
		return;
	}
  
  switch(buildStep)
  {
    case BUILD:
      sizer();
      break;
    case CLEAN:
      mainWindow->onCleanComplete();
      resetBuildProcess( );
      break;
    case SIZER:
      mainWindow->onBuildComplete(true);
      resetBuildProcess( );
      break;
  }
}

/*
  Reset the build process.
*/
void Builder::resetBuildProcess()
{
  errMsg.clear();
  outputMsg.clear();
  currentProjectPath.clear();
}

/*
  Create a Makefile for this project.
  Assemble the list of source files, set the name,
  add the template info and we're all set.
*/
void Builder::createMakefile(QString projectPath)
{
  QDir buildDir(projectPath + "/build");
  QFile makefile(buildDir.filePath("Makefile_"));
  if(makefile.open(QIODevice::WriteOnly | QFile::Text))
  {
    QDir dir(projectPath);
    QTextStream tofile(&makefile);
    tofile << "###############################################################" << endl;
    tofile << "#" << endl << "# This file generated automatically by mcbuilder, ";
    tofile << QDate::currentDate().toString("MMM d, yyyy") << endl;
    tofile << "# Any manual changes made to this file will be overwritten the next time mcbuilder builds." << endl << "#" << endl;
    tofile << "###############################################################" << endl << endl;
    
    tofile << "OUTPUT = " + dir.dirName().toLower() << endl << endl;
    tofile << "all: $(OUTPUT).bin" << endl << endl;
    
    dir = QDir::current();
    
    // thumb files here
    tofile << "THUMB_SRC= \\" << endl;
    
    // arm files here
    tofile << "ARM_SRC= \\" << endl;
    
    // include dirs here
    tofile << "INCLUDEDIRS = \\" << endl;
    tofile << "-I.. \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/appboard/makingthings") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/makingthings") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/makingthings/testing") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/lwip/src/include") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/lwip/contrib/port/FreeRTOS/AT91SAM7X") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/freertos/include") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/freertos/portable/GCC/ARM7_AT91SAM7S") + " \\" << endl;
    tofile << "-I" + dir.filePath("resources/cores/makecontroller/controller/lwip/src/include/ipv4") << endl << endl;
    // check for libraries...
    
    // tools
    tofile << "CC=arm-elf-gcc" << endl;
    tofile << "OBJCOPY=arm-elf-objcopy" << endl;
    tofile << "ARCH=arm-elf-ar" << endl;
    tofile << "CRT0=" + dir.filePath("resources/cores/makecontroller/controller/startup/boot.s") << endl;
    tofile << "DEBUG=" << endl;
    tofile << "OPTIM=-O2" << endl;
    tofile << "LDSCRIPT=" + dir.filePath("resources/cores/makecontroller/controller/startup/atmel-rom.ld") << endl << endl;
    
    // flags
    tofile << "CFLAGS= \\" << endl;
    tofile << "$(INCLUDEDIRS) \\" << endl;
    tofile << "-Wall \\" << endl;
    tofile << "-Wextra \\" << endl;
    tofile << "-Wstrict-prototypes \\" << endl;
    tofile << "-Wmissing-prototypes \\" << endl;
    tofile << "-Wmissing-declarations \\" << endl;
    tofile << "-Wno-strict-aliasing \\" << endl;
    tofile << "-D SAM7_GCC \\" << endl;
    tofile << "-D THUMB_INTERWORK \\" << endl;
    tofile << "-mthumb-interwork \\" << endl;
    tofile << "-mcpu=arm7tdmi \\" << endl;
    tofile << "-T$(LDSCRIPT) \\" << endl;
    tofile << "$(DEBUG) \\" << endl;
    tofile << "$(OPTIM)" << endl << endl;
    
    // rules here
    tofile << "$(OUTPUT).bin : $(OUTPUT).elf" << endl;
    tofile << "$(OBJCOPY) $(OUTPUT).elf -O binary $(OUTPUT).bin" << endl << endl;
    
    tofile << "$(OUTPUT).elf : $(ARM_OBJ) $(THUMB_OBJ) $(CRT0)" << endl;
    tofile << "  $(CC) $(CFLAGS) $(ARM_OBJ) $(THUMB_OBJ) -nostartfiles $(CRT0) $(LINKER_FLAGS)" << endl << endl;

    tofile << "$(THUMB_OBJ) : %.o : %.c" << endl;
    tofile << "  $(CC) -c $(THUMB_FLAGS) $(CFLAGS) $< -o $@" << endl << endl;

    tofile << "$(ARM_OBJ) : %.o : %.c" << endl;
    tofile << "  $(CC) -c $(CFLAGS) $< -o $@" << endl << endl;
            
    tofile << "clean :" << endl;
    tofile << "  rm -f $(ARM_OBJ)" << endl;
    tofile << "  rm -f $(THUMB_OBJ)" << endl;
    tofile << "  rm -f $(OUTPUT).elf" << endl;
    tofile << "  rm -f $(OUTPUT).bin" << endl;
    tofile << "  rm -f $(OUTPUT)_o.map" << endl << endl;
  }
}

void Builder::onBuildError(QProcess::ProcessError error)
{
  qDebug("Build error: %d", error);
//  switch(error)
//  {
//    
//  }
}

void Builder::readOutput( )
{
  filterOutput(readAllStandardOutput());
}

/*
  This gets called when the currently running program
  spits out some error info.  The strings can come in chunks, so
  wait till we get an end-of-line before doing anything.
*/
void Builder::readError( )
{
  filterErrorOutput(readAllStandardError());
}

/*
  Filter the output of the build process
  and only show the most important parts to the user.
  
  Strings can come in chunks, so wait till we get an 
  end-of-line before doing anything.
*/
void Builder::filterOutput(QString output)
{
  // switch based on what part of the build we're performing
  switch(buildStep)
  {
    case BUILD:
    {
      outputMsg += output;
      if(outputMsg.endsWith("\n")) // we have a complete message to deal with
      {
        //printf("msg: %s\n", qPrintable(outputMsg));
        QStringList sl = outputMsg.split(" ");
        if(sl.at(0) == "arm-elf-gcc" && sl.at(1) == "-c")
        {
          QStringList sl2 = sl.last().split("/");
          mainWindow->buildingNow(sl2.last());
        }
        outputMsg.clear();
      }
      break;
    }
    case CLEAN:
      break;
    case SIZER:
    {
      QStringList vals = output.split(" ", QString::SkipEmptyParts);
      if(vals.count() == 10) // we expect 10 values back from arm-elf-size
      {
        bool ok = false;
        int total_size = vals.at(8).toInt(&ok);
        if(ok)
        {
          QDir dir(currentProjectPath);
          mainWindow->printOutput(QString("%1.bin is %2 out of a possible 256000 bytes.").arg(dir.dirName().toLower()).arg(total_size));
        }
      }
      break;
    }
  }
}

/*
  Filter the error messages of the build process
  and only show the most important parts to the user.
*/
void Builder::filterErrorOutput(QString errOutput)
{
  switch(buildStep)
  {
    case BUILD:
      errMsg += errOutput;
      if(errMsg.endsWith("\n")) // we have a complete message to deal with
      {
        QStringList sl = errMsg.split(":");
        for(int i = 0; i < sl.count(); i++) // remove any spaces from front and back
          sl[i] = sl.at(i).trimmed();

        //printf("err: %s\n", qPrintable(errMsg));
        if(sl.contains("warning"))
        {
          QString msg;
          QTextStream(&msg) << "Warning - " << sl.last() << endl;
          mainWindow->printOutputError(msg);
        }
        else if(sl.contains("error"))
        {
          QString msg;
          QTextStream(&msg) << "Error - " << sl.last() << endl;
          mainWindow->printOutputError(msg);
        }
        errMsg.clear();
      }
      break;
    case CLEAN:
      mainWindow->printOutputError(errOutput);
      break;
    case SIZER:
      mainWindow->printOutputError(errOutput);
      break;
  }
}




