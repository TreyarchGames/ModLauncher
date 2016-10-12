/*
*
* Copyright 2016 Activision Publishing, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
*/

#include "stdafx.h"
#include "mlMainWindow.h"

int main(int argc, char *argv[])
{
	QApplication App(argc, argv);

	QCoreApplication::setOrganizationDomain("treyarch.com");
	QCoreApplication::setOrganizationName("Treyarch");
	QCoreApplication::setApplicationName("ModLauncher");
//	QCoreApplication::setApplicationVersion();

	QSettings settings;
	if (settings.value("UseDarkTheme", false).toBool() == true)
	{
		QString ToolsPath = getenv("TA_TOOLS_PATH");
		QString StyleSheetPath = QString("%1\\radiant\\stylesheet.qss").arg(ToolsPath);

		QFile f(StyleSheetPath);
		if (!f.exists())
		{
			printf("ERROR: Unable to set stylesheet - file not found\n");
		}
		else
		{
			f.open(QFile::ReadOnly | QFile::Text);
			QTextStream ts(&f);
			qApp->setStyleSheet(ts.readAll());
			f.close();
		}
	}

	mlMainWindow MainWindow;
	MainWindow.UpdateDB();
	MainWindow.show();

	return App.exec();
}
