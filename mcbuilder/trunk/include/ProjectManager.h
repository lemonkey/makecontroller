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

#ifndef PROJECT_MANAGER_H
#define PROJECT_MANAGER_H

#include <QString>
#include <QFileInfo>

class ProjectManager
{
  public:
    ProjectManager( ) { };
    bool createNewFile(QString projectPath, QString filePath);
    bool saveFileAs(QString projectPath, QString existingFilePath, QString newFilePath);
    bool addToProjectFile(QString projectPath, QString newFilePath, QString buildtype);
    bool removeFromProjectFile(QString projectPath, QString filePath);
    QString createNewProject(QString newProjectPath);
    QString saveCurrentProjectAs(QString currentProjectPath, QString newProjectPath);
  
  private:
    void confirmValidFileSuffix(QFileInfo* fi);
    void confirmValidProjectName(QString* name);
};

#endif // PROJECT_MANAGER_H
