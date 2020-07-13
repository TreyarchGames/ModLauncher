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

#include <functional>
#include <iomanip>
#include <sstream>
#include <utility>

#pragma comment(lib, "steam_api64.lib")

const int AppId = 311210;

const char* gLanguages[] = { "english", "french", "italian", "spanish", "german", "portuguese", "russian", "polish", "japanese", "traditionalchinese", "simplifiedchinese", "englisharabic" };
const char* gTags[] = { "Animation", "Audio", "Character", "Map", "Mod", "Mode", "Model", "Multiplayer", "Scorestreak", "Skin", "Specialist", "Texture", "UI", "Vehicle", "Visual Effect", "Weapon", "WIP", "Zombies" };
dvar_s gDvars[] = {
					{"ai_disableSpawn", "Disable AI from spawning", DVAR_VALUE_BOOL},
					{"developer", "Run developer mode", DVAR_VALUE_INT, 0, 2},
					{"g_password", "Password for your server", DVAR_VALUE_STRING},
					{"logfile", "Console log information written to current fs_game", DVAR_VALUE_INT, 0, 2},
					{"scr_mod_enable_devblock", "Developer blocks are executed in mods ", DVAR_VALUE_BOOL},
					{"connect", "Connect to a specific server", DVAR_VALUE_STRING, NULL, NULL, true},
					{"set_gametype", "Set a gametype to load with map", DVAR_VALUE_STRING, NULL, NULL, true},
					{"splitscreen", "Enable splitscreen", DVAR_VALUE_BOOL},
					{"splitscreen_playerCount", "Allocate the number of instances for splitscreen", DVAR_VALUE_INT, 0, 2}
				 };
enum mlItemType
{
	ML_ITEM_UNKNOWN,
	ML_ITEM_MAP,
	ML_ITEM_MOD
};

mlBuildThread::mlBuildThread(const QList<QPair<QString, QStringList>>& Commands, bool IgnoreErrors)
	: mCommands(Commands), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors)
{
}

void mlBuildThread::run()
{
	bool Success = true;

	for (const QPair<QString, QStringList>& Command : mCommands)
	{
		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(QFileInfo(Command.first).absolutePath());
		Process->setProcessChannelMode(QProcess::MergedChannels);

		emit OutputReady(Command.first + ' ' + Command.second.join(' ') + "\n");

		Process->start(Command.first, Command.second);
		for (;;)
		{
			Sleep(100);

			if (Process->waitForReadyRead(0))
				emit OutputReady(Process->readAll());

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}

		if (Process->exitStatus() != QProcess::NormalExit)
			return;

		if (Process->exitCode() != 0)
		{
			Success = false;
			if (!mIgnoreErrors)
				return;
		}
	}

	mSuccess = Success;
}

mlConvertThread::mlConvertThread(QStringList& Files, QString& OutputDir, bool IgnoreErrors, bool OverwriteFiles)
	: mFiles(Files), mOutputDir(OutputDir), mSuccess(false), mCancel(false), mIgnoreErrors(IgnoreErrors), mOverwrite(OverwriteFiles)
{
}

void mlConvertThread::run()
{
	bool Success = true;

	unsigned int convCountSuccess	= 0;
	unsigned int convCountSkipped	= 0;
	unsigned int convCountFailed	= 0;

	for (QString file : mFiles)
	{
		QFileInfo file_info(file);
		QString working_directory = file_info.absolutePath();

		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
		Process->setWorkingDirectory(working_directory);
		Process->setProcessChannelMode(QProcess::MergedChannels);

		file = file_info.baseName();

		QString ToolsPath = QDir::fromNativeSeparators(getenv("TA_TOOLS_PATH"));
		QString ExecutablePath = QString("%1bin/export2bin.exe").arg(ToolsPath);

		QStringList args;
		//args.append("/v"); // Verbose
		args.append("/piped");

		QString filepath = file_info.absoluteFilePath();

		QString ext = file_info.suffix().toUpper();
		if (ext == "XANIM_EXPORT")
			ext = ".XANIM_BIN";
		else if (ext == "XMODEL_EXPORT")
			ext = ".XMODEL_BIN";
		else
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file has invalid extension)\n");
			convCountSkipped++;
			continue;
		}

		QString target_filepath = QDir::cleanPath(mOutputDir) + QDir::separator() + file + ext;

		QFile infile(filepath);
		QFile outfile(target_filepath);

		if (!mOverwrite && outfile.exists())
		{
			emit OutputReady("Export2Bin: Skipping file '" + filepath + "' (file already exists)\n");
			convCountSkipped++;
			continue;
		}

		infile.open(QIODevice::OpenMode::enum_type::ReadOnly);
		if (!infile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + filepath + "' for reading\n");
			convCountFailed++;
			continue;
		}

		emit OutputReady("Export2Bin: Converting '" + file + "'");

		QByteArray buf = infile.readAll();
		infile.close();

		Process->start(ExecutablePath, args);
		Process->write(buf);
		Process->closeWriteChannel();

		QByteArray standardOutputPipeData;
		QByteArray standardErrorPipeData;

		for (;;)
		{
			Sleep(20);

			if (Process->waitForReadyRead(0))
			{
				standardOutputPipeData.append(Process->readAllStandardOutput());
				standardErrorPipeData.append(Process->readAllStandardError());
			}

			QProcess::ProcessState State = Process->state();
			if (State == QProcess::NotRunning)
				break;

			if (mCancel)
				Process->kill();
		}

		if (Process->exitStatus() != QProcess::NormalExit)
		{
			emit OutputReady("ERROR: Process exited abnormally");
			Success = false;
			break;
		}

		if (Process->exitCode() != 0)
		{
			emit OutputReady(standardOutputPipeData);
			emit OutputReady(standardErrorPipeData);

			convCountFailed++;

			if (!mIgnoreErrors)
			{
				Success = false;
				break;
			}

			continue;
		}

		outfile.open(QIODevice::OpenMode::enum_type::WriteOnly);
		if (!outfile.isOpen())
		{
			emit OutputReady("Export2Bin: Could not open '" + target_filepath + "' for writing\n");
			continue;
		}

		outfile.write(standardOutputPipeData);
		outfile.close();

		convCountSuccess++;
	}

	mSuccess = Success;
	if (mSuccess)
	{
		QString msg = QString("Export2Bin: Finished!\n\n"
			"Files Processed: %1\n"
			"Successes: %2\n"
			"Skipped: %3\n"
			"Failures: %4\n").arg(mFiles.count()).arg(convCountSuccess).arg(convCountSkipped).arg(convCountFailed);
		emit OutputReady(msg);
	}
}

mlMainWindow::mlMainWindow()
{
	QSettings Settings;

	mBuildThread = NULL;
	mBuildLanguage = Settings.value("BuildLanguage", "english").toString();
	mTreyarchTheme = Settings.value("UseDarkTheme", false).toBool();

	// Qt prefers '/' over '\\'
	mGamePath = QString(getenv("TA_GAME_PATH")).replace('\\', '/');
	mToolsPath = QString(getenv("TA_TOOLS_PATH")).replace('\\', '/');

	UpdateTheme();

	setWindowIcon(QIcon(":/resources/ModLauncher.png"));
	setWindowTitle("Black Ops III Mod Tools Launcher");
	
	resize(1024, 768);

	CreateActions();
	CreateMenu();
	CreateToolBar();

	mExport2BinGUIWidget = NULL;

	QSplitter* CentralWidget = new QSplitter();
	CentralWidget->setOrientation(Qt::Vertical);

	QWidget* TopWidget = new QWidget();
	CentralWidget->addWidget(TopWidget);

	QHBoxLayout* TopLayout = new QHBoxLayout(TopWidget);
	TopWidget->setLayout(TopLayout);

	mFileListWidget = new QTreeWidget();
	mFileListWidget->setHeaderHidden(true);
	mFileListWidget->setUniformRowHeights(true);
	mFileListWidget->setRootIsDecorated(false);
	mFileListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	TopLayout->addWidget(mFileListWidget);

	connect(mFileListWidget, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(ContextMenuRequested()));

	QVBoxLayout* ActionsLayout = new QVBoxLayout();
	TopLayout->addLayout(ActionsLayout);

	QHBoxLayout* CompileLayout = new QHBoxLayout();
	ActionsLayout->addLayout(CompileLayout);

	mCompileEnabledWidget = new QCheckBox("Compile");
	CompileLayout->addWidget(mCompileEnabledWidget);

	mCompileModeWidget = new QComboBox();
	mCompileModeWidget->addItems(QStringList() << "Ents" << "Full");
	mCompileModeWidget->setCurrentIndex(1);
	CompileLayout->addWidget(mCompileModeWidget);

	QHBoxLayout* LightLayout = new QHBoxLayout();
	ActionsLayout->addLayout(LightLayout);

	mLightEnabledWidget = new QCheckBox("Light");
	LightLayout->addWidget(mLightEnabledWidget);

	mLightQualityWidget = new QComboBox();
	mLightQualityWidget->addItems(QStringList() << "Low" << "Medium" << "High");
	mLightQualityWidget->setCurrentIndex(1);
	mLightQualityWidget->setMinimumWidth(64); // Fix for "Medium" being cut off in the dark theme
	LightLayout->addWidget(mLightQualityWidget);

	mLinkEnabledWidget = new QCheckBox("Link");
	ActionsLayout->addWidget(mLinkEnabledWidget);

	mRunEnabledWidget = new QCheckBox("Run");
	ActionsLayout->addWidget(mRunEnabledWidget);

	mRunOptionsWidget = new QLineEdit();
	mRunOptionsWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	ActionsLayout->addWidget(mRunOptionsWidget);

	mBuildButton = new QPushButton("Build");
	connect(mBuildButton, SIGNAL(clicked()), mActionEditBuild, SLOT(trigger()));
	ActionsLayout->addWidget(mBuildButton);

	mDvarsButton = new QPushButton("Dvars");
	connect(mDvarsButton, SIGNAL(clicked()), this, SLOT(OnEditDvars()));
	ActionsLayout->addWidget(mDvarsButton);

	mLogButton = new QPushButton("Save Log");
	connect(mLogButton, SIGNAL(clicked()), this, SLOT(OnSaveLog()));
	ActionsLayout->addWidget(mLogButton);

	mIgnoreErrorsWidget = new QCheckBox("Ignore Errors");
	ActionsLayout->addWidget(mIgnoreErrorsWidget);

	ActionsLayout->addStretch(1);

	mOutputWidget = new QPlainTextEdit(this);
	mOutputWidget->setReadOnly(true);
	CentralWidget->addWidget(mOutputWidget);

	setCentralWidget(CentralWidget);

	mShippedMapList << "mp_aerospace" <<  "mp_apartments" << "mp_arena" << "mp_banzai" << "mp_biodome" << "mp_chinatown" << "mp_city" << "mp_conduit" << "mp_crucible" << "mp_cryogen" << "mp_ethiopia" << "mp_freerun_01" << "mp_freerun_02" << "mp_freerun_03" << "mp_freerun_04" << "mp_havoc" << "mp_infection" << "mp_kung_fu" << "mp_metro" << "mp_miniature" << "mp_nuketown_x" << "mp_redwood" << "mp_rise" << "mp_rome" << "mp_ruins" << "mp_sector" << "mp_shrine" << "mp_skyjacked" << "mp_spire" << "mp_stronghold" << "mp_veiled" << "mp_waterpark" << "mp_western" << "zm_castle" << "zm_factory" << "zm_genesis" << "zm_island" << "zm_levelcommon" << "zm_stalingrad" << "zm_zod";

	Settings.beginGroup("MainWindow");
	resize(QSize(800, 600));
	move(QPoint(200, 200));
	restoreGeometry(Settings.value("Geometry").toByteArray());
	restoreState(Settings.value("State").toByteArray());
	Settings.endGroup();

	SteamAPI_Init();

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(SteamUpdate()));
	mTimer.start(1000);

	PopulateFileList();
}

mlMainWindow::~mlMainWindow()
{
}

void mlMainWindow::CreateActions()
{
	mActionFileNew = new QAction(QIcon(":/resources/FileNew.png"), "&New...", this);
	mActionFileNew->setShortcut(QKeySequence("Ctrl+N"));
	connect(mActionFileNew, SIGNAL(triggered()), this, SLOT(OnFileNew()));

	mActionFileAssetEditor = new QAction(QIcon(":/resources/AssetEditor.png"), "&Asset Editor", this);
	mActionFileAssetEditor->setShortcut(QKeySequence("Ctrl+A"));
	connect(mActionFileAssetEditor, SIGNAL(triggered()), this, SLOT(OnFileAssetEditor()));

	mActionFileLevelEditor = new QAction(QIcon(":/resources/Radiant.png"), "Open in &Radiant", this);
	mActionFileLevelEditor->setShortcut(QKeySequence("Ctrl+R"));
	mActionFileLevelEditor->setToolTip("Level Editor");
	connect(mActionFileLevelEditor, SIGNAL(triggered()), this, SLOT(OnFileLevelEditor()));

	mActionFileExport2Bin = new QAction(QIcon(":/resources/Export2Bin.png"), "&Export2Bin GUI", this);
	mActionFileExport2Bin->setShortcut(QKeySequence("Ctrl+E"));
	connect(mActionFileExport2Bin, SIGNAL(triggered()), this, SLOT(OnFileExport2Bin()));

	mActionFileExit = new QAction("E&xit", this);
	connect(mActionFileExit, SIGNAL(triggered()), this, SLOT(close()));

	mActionEditBuild = new QAction(QIcon(":/resources/Go.png"), "Build", this);
	mActionEditBuild->setShortcut(QKeySequence("Ctrl+B"));
	connect(mActionEditBuild, SIGNAL(triggered()), this, SLOT(OnEditBuild()));

	mActionEditPublish = new QAction(QIcon(":/resources/upload.png"), "Publish", this);
	mActionEditPublish->setShortcut(QKeySequence("Ctrl+P"));
	connect(mActionEditPublish, SIGNAL(triggered()), this, SLOT(OnEditPublish()));

	mActionEditOptions = new QAction("&Options...", this);
	connect(mActionEditOptions, SIGNAL(triggered()), this, SLOT(OnEditOptions()));

	mActionHelpAbout = new QAction("&About...", this);
	connect(mActionHelpAbout, SIGNAL(triggered()), this, SLOT(OnHelpAbout()));
}

void mlMainWindow::CreateMenu()
{
	QMenuBar* MenuBar = new QMenuBar(this);

	QMenu* FileMenu = new QMenu("&File", MenuBar);
	FileMenu->addAction(mActionFileNew);
	FileMenu->addSeparator();
	FileMenu->addAction(mActionFileAssetEditor);
	FileMenu->addAction(mActionFileLevelEditor);
	FileMenu->addAction(mActionFileExport2Bin);
	FileMenu->addSeparator();
	FileMenu->addAction(mActionFileExit);
	MenuBar->addAction(FileMenu->menuAction());

	QMenu* EditMenu = new QMenu("&Edit", MenuBar);
	EditMenu->addAction(mActionEditBuild);
	EditMenu->addAction(mActionEditPublish);
	EditMenu->addSeparator();
	EditMenu->addAction(mActionEditOptions);
	MenuBar->addAction(EditMenu->menuAction());

	QMenu* HelpMenu = new QMenu("&Help", MenuBar);
	HelpMenu->addAction(mActionHelpAbout);
	MenuBar->addAction(HelpMenu->menuAction());

	setMenuBar(MenuBar);
}

void mlMainWindow::CreateToolBar()
{
	QToolBar* ToolBar = new QToolBar("Standard", this);
	ToolBar->setObjectName(QStringLiteral("StandardToolBar"));

	ToolBar->addAction(mActionFileNew);
	ToolBar->addAction(mActionEditBuild);
	ToolBar->addAction(mActionEditPublish);
	ToolBar->addSeparator();
	ToolBar->addAction(mActionFileAssetEditor);
	ToolBar->addAction(mActionFileLevelEditor);
	ToolBar->addAction(mActionFileExport2Bin);

	addToolBar(Qt::TopToolBarArea, ToolBar);
}

void mlMainWindow::InitExport2BinGUI()
{
	QDockWidget *dock = new QDockWidget(this, NULL);
	dock->setWindowTitle("Export2Bin");
	dock->setFloating(true);

	QWidget* widget = new QWidget(dock);
	QGridLayout* gridLayout = new QGridLayout();
	widget->setLayout(gridLayout);
	dock->setWidget(widget);

	Export2BinGroupBox* groupBox = new Export2BinGroupBox(dock, this);
	gridLayout->addWidget(groupBox, 0, 0);

	QLabel* label = new QLabel("Drag Files Here", groupBox);
	label->setAlignment(Qt::AlignCenter);
	QVBoxLayout* groupBoxLayout = new QVBoxLayout(groupBox);
	groupBoxLayout->addWidget(label);
	groupBox->setLayout(groupBoxLayout);

	mExport2BinOverwriteWidget = new QCheckBox("&Overwrite Existing Files", widget);
	gridLayout->addWidget(mExport2BinOverwriteWidget, 1, 0);
	
	QSettings Settings;
	mExport2BinOverwriteWidget->setChecked(Settings.value("Export2Bin_OverwriteFiles", true).toBool());

	QHBoxLayout* dirLayout = new QHBoxLayout();
	QLabel* dirLabel = new QLabel("Ouput Directory:", widget);
	mExport2BinTargetDirWidget = new QLineEdit(widget);
	QToolButton* dirBrowseButton = new QToolButton(widget);
	dirBrowseButton->setText("...");

	const QDir defaultPath = QString("%1/model_export/export2bin/").arg(mToolsPath);
	mExport2BinTargetDirWidget->setText(Settings.value("Export2Bin_TargetDir", defaultPath.absolutePath()).toString());

	connect(dirBrowseButton, SIGNAL(clicked()), this, SLOT(OnExport2BinChooseDirectory()));
	connect(mExport2BinOverwriteWidget, SIGNAL(clicked()), this, SLOT(OnExport2BinToggleOverwriteFiles()));

	dirBrowseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	dirLayout->addWidget(dirLabel);
	dirLayout->addWidget(mExport2BinTargetDirWidget);
	dirLayout->addWidget(dirBrowseButton);

	gridLayout->addLayout(dirLayout, 2, 0);

	groupBox->setAcceptDrops(true);

	dock->resize(QSize(256, 256));

	mExport2BinGUIWidget = dock;
}

void mlMainWindow::closeEvent(QCloseEvent* Event)
{
	QSettings Settings;
	Settings.beginGroup("MainWindow");
	Settings.setValue("Geometry", saveGeometry());
	Settings.setValue("State", saveState());
	Settings.endGroup();

	Event->accept();
}

void mlMainWindow::SteamUpdate()
{
	SteamAPI_RunCallbacks();
}

void mlMainWindow::UpdateDB()
{
	if (mBuildThread)
		return;

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));

	StartBuildThread(Commands);
}

void mlMainWindow::StartBuildThread(const QList<QPair<QString, QStringList>>& Commands)
{
	mBuildButton->setText("Cancel");
	mOutputWidget->clear();

	mBuildThread = new mlBuildThread(Commands, mIgnoreErrorsWidget->isChecked());
	connect(mBuildThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mBuildThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mBuildThread->start();
}

void mlMainWindow::StartConvertThread(QStringList& pathList, QString& outputDir, bool allowOverwrite)
{
	mConvertThread = new mlConvertThread(pathList, outputDir, true, allowOverwrite);
	connect(mConvertThread, SIGNAL(OutputReady(QString)), this, SLOT(BuildOutputReady(QString)));
	connect(mConvertThread, SIGNAL(finished()), this, SLOT(BuildFinished()));
	mConvertThread->start();
}

void mlMainWindow::PopulateFileList()
{
	mFileListWidget->clear();

	QString UserMapsFolder = QDir::cleanPath(QString("%1/usermaps/").arg(mGamePath));
	QStringList UserMaps = QDir(UserMapsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	QTreeWidgetItem* MapsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Maps");

	QFont Font = MapsRootItem->font(0);
	Font.setBold(true);
	MapsRootItem->setFont(0, Font);

	for (QString MapName : UserMaps)
	{
		QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(UserMapsFolder, MapName, MapName);

		if (QFileInfo(ZoneFileName).isFile())
		{
			QTreeWidgetItem* MapItem = new QTreeWidgetItem(MapsRootItem, QStringList() << MapName);
			MapItem->setCheckState(0, Qt::Unchecked);
			MapItem->setData(0, Qt::UserRole, ML_ITEM_MAP);
		}
	}

	QString ModsFolder = QDir::cleanPath(QString("%1/mods/").arg(mGamePath));
	QStringList Mods = QDir(ModsFolder).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
	QTreeWidgetItem* ModsRootItem = new QTreeWidgetItem(mFileListWidget, QStringList() << "Mods");
	ModsRootItem->setFont(0, Font);
	const char* Files[4] = { "core_mod", "mp_mod", "cp_mod", "zm_mod" };

	for (QString ModName : Mods)
	{
		QTreeWidgetItem* ParentItem = NULL;

		for (int FileIdx = 0; FileIdx < 4; FileIdx++)
		{
			QString ZoneFileName = QString("%1/%2/zone_source/%3.zone").arg(ModsFolder, ModName, Files[FileIdx]);

			if (QFileInfo(ZoneFileName).isFile())
			{
				if (!ParentItem)
					ParentItem = new QTreeWidgetItem(ModsRootItem, QStringList() << ModName);

				QTreeWidgetItem* ModItem = new QTreeWidgetItem(ParentItem, QStringList() << Files[FileIdx]);
				ModItem->setCheckState(0, Qt::Unchecked);
				ModItem->setData(0, Qt::UserRole, ML_ITEM_MOD);
			}
		}
	}

	mFileListWidget->expandAll();
}

void mlMainWindow::ContextMenuRequested()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString ItemType = (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP) ? "Map" : "Mod";

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_UNKNOWN)
		return;

	QIcon GameIcon(":/resources/BlackOps3.png");

	QMenu* Menu = new QMenu;
	Menu->addAction(GameIcon, QString("Run %1").arg(ItemType), this, SLOT(OnRunMapOrMod()));

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		Menu->addAction(mActionFileLevelEditor);

	Menu->addAction("Edit Zone File", this, SLOT(OnOpenZoneFile()));
	Menu->addAction(QString("Open %1 Folder").arg(ItemType), this, SLOT(OnOpenModRootFolder()));

	Menu->addSeparator();
	Menu->addAction("Delete", this, SLOT(OnDelete()));
	Menu->addAction("Clean XPaks", this, SLOT(OnCleanXPaks()));

	Menu->exec(QCursor::pos());
}

void mlMainWindow::OnFileAssetEditor()
{
	QProcess* Process = new QProcess();
	connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));
	Process->start(QString("%1/bin/AssetEditor_modtools.exe").arg(mToolsPath), QStringList());
}

void mlMainWindow::OnFileLevelEditor()
{
	QProcess* Process = new QProcess();
	connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));

	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.count() && ItemList[0]->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = ItemList[0]->text(0);
		Process->start(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), QStringList() << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName));
	}
	else
	{
		Process->start(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), QStringList());
	}
}

void mlMainWindow::OnFileExport2Bin()
{
	if (mExport2BinGUIWidget == NULL)
	{
		InitExport2BinGUI();
		mExport2BinGUIWidget->hide(); // Ensure the window is hidden (just in case)
	}

	mExport2BinGUIWidget->isVisible() ? mExport2BinGUIWidget->hide() : mExport2BinGUIWidget->show();
}

void mlMainWindow::OnFileNew()
{
	QDir TemplatesFolder(QString("%1/rex/templates").arg(mToolsPath));
	QStringList Templates = TemplatesFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

	if (Templates.isEmpty())
	{
		QMessageBox::information(this, "Error", "Could not find any map templates.");
		return;
	}

	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("New Map or Mod");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QFormLayout* FormLayout = new QFormLayout();
	Layout->addLayout(FormLayout);

	QLineEdit* NameWidget = new QLineEdit();
	NameWidget->setValidator(new QRegularExpressionValidator(QRegularExpression("[a-zA-Z0-9_]*"), this));
	FormLayout->addRow("Name:", NameWidget);

	QComboBox* TemplateWidget = new QComboBox();
	TemplateWidget->addItems(Templates);
	FormLayout->addRow("Template:", TemplateWidget);

	QFrame* Frame = new QFrame();
	Frame->setFrameShape(QFrame::HLine);
	Frame->setFrameShadow(QFrame::Raised);
	Layout->addWidget(Frame);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	QString Name = NameWidget->text();

	if (Name.isEmpty())
	{
		QMessageBox::information(this, "Error", "Map name cannot be empty.");
		return;
	}

	if (mShippedMapList.contains(Name, Qt::CaseInsensitive))
	{
		QMessageBox::information(this, "Error", "Map name cannot be the same as a built-in map.");
		return;
	}

	QByteArray MapName = NameWidget->text().toLatin1().toLower();
	QString Output;

	QString Template = Templates[TemplateWidget->currentIndex()];

	if ((Template == "MP Mod Level" && !MapName.startsWith("mp_")) || (Template == "ZM Mod Level" && !MapName.startsWith("zm_")))
	{
		QMessageBox::information(this, "Error", "Map name must start with 'mp_' or 'zm_'.");
		return;
	}

	std::function<bool(const QString&, const QString&)> RecursiveCopy=[&](const QString& SourcePath, const QString& DestPath) -> bool
	{
		QDir Dir(SourcePath);
		if (!Dir.exists())
			return false;

		foreach (QString DirEntry, Dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		{
			QString NewPath = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			Dir.mkpath(NewPath);
			if (!RecursiveCopy(SourcePath + QDir::separator() + DirEntry, NewPath))
				return false;
		}

		foreach (QString DirEntry, Dir.entryList(QDir::Files))
		{
			QFile SourceFile(SourcePath + QDir::separator() + DirEntry);
			QString DestFileName = QString(DestPath + QDir::separator() + DirEntry).replace(QString("template"), MapName);
			QFile DestFile(DestFileName);

			if (!SourceFile.open(QFile::ReadOnly) || !DestFile.open(QFile::WriteOnly))
				return false;

			while (!SourceFile.atEnd())
			{
				QByteArray Line = SourceFile.readLine();

				if (Line.contains("guid"))
				{
					QString LineString(Line);
					LineString.replace(QRegExp("guid \"\\{(.*)\\}\""), QString("guid \"%1\"").arg(QUuid::createUuid().toString()));
					Line = LineString.toLatin1();
				}
				else
					Line.replace("template", MapName);

				DestFile.write(Line);
			}

			Output += DestFileName + "\n";
		}

		return true;
	};

	if (RecursiveCopy(TemplatesFolder.absolutePath() + QDir::separator() + Templates[TemplateWidget->currentIndex()], QDir::cleanPath(mGamePath)))
	{
		PopulateFileList();

		QMessageBox::information(this, "New Map Created", QString("Files created:\n") + Output);
	}
	else
		QMessageBox::information(this, "Error", "Error creating map files.");
}

void mlMainWindow::OnEditBuild()
{
	if (mBuildThread)
	{
		mBuildThread->Cancel();
		return;
	}

	QList<QPair<QString, QStringList>> Commands;
	bool UpdateAdded = false;

	auto AddUpdateDBCommand = [&]()
	{
		if (!UpdateAdded)
		{
			Commands.append(QPair<QString, QStringList>(QString("%1/gdtdb/gdtdb.exe").arg(mToolsPath), QStringList() << "/update"));
			UpdateAdded = true;
		}
	};

	QList<QTreeWidgetItem*> CheckedItems;

	std::function<void (QTreeWidgetItem*)> SearchCheckedItems=[&](QTreeWidgetItem* ParentItem) -> void
	{
		for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = ParentItem->child(ChildIdx);
			if (Child->checkState(0) == Qt::Checked)
				CheckedItems.append(Child);
			else
				SearchCheckedItems(Child);
		}
	};

	SearchCheckedItems(mFileListWidget->invisibleRootItem());
	QString LastMap, LastMod;

	QStringList LanguageArgs;
	LanguageArgs;

	if (mBuildLanguage != "All")
		LanguageArgs << "-language" << mBuildLanguage;
	else for (const QString& Language : gLanguages)
		LanguageArgs << "-language" << Language;

	for (QTreeWidgetItem* Item : CheckedItems)
	{
		if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
		{
			QString MapName = Item->text(0);

			if (mCompileEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-platform" << "pc";

				if (mCompileModeWidget->currentIndex() == 0)
					Args << "-onlyents";
				else
					Args << "-navmesh" << "-navvolume";

				Args << "-loadFrom" << QString("%1\\map_source\\%2\\%3.map").arg(mGamePath, MapName.left(2), MapName);
				Args << QString("%1\\share\\raw\\maps\\%2\\%3.d3dbsp").arg(mGamePath, MapName.left(2), MapName);

				Commands.append(QPair<QString, QStringList>(QString("%1\\bin\\cod2map64.exe").arg(mToolsPath), Args));
			}

			if (mLightEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				QStringList Args;
				Args << "-ledSilent";

				switch (mLightQualityWidget->currentIndex())
				{
				case 0:
					Args << "+low";
					break;

				default:
				case 1:
					Args << "+medium";
					break;

				case 2:
					Args << "+high";
					break;
				}

				Args << "+localprobes" << "+forceclean" << "+recompute" << QString("%1/map_source/%2/%3.map").arg(mGamePath, MapName.left(2), MapName);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/radiant_modtools.exe").arg(mToolsPath), Args));
			}

			if (mLinkEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-modsource" << MapName));
			}

			LastMap = MapName;
		}
		else
		{
			QString ModName = Item->parent()->text(0);

			if (mLinkEnabledWidget->isChecked())
			{
				AddUpdateDBCommand();

				QString ZoneName = Item->text(0);
				Commands.append(QPair<QString, QStringList>(QString("%1/bin/linker_modtools.exe").arg(mToolsPath), QStringList() << LanguageArgs << "-fs_game" << ModName << "-modsource" << ZoneName));
			}

			LastMod = ModName;
		}
	}

	if (mRunEnabledWidget->isChecked() && (!LastMod.isEmpty() || !LastMap.isEmpty()))
	{
		QStringList Args;

		if(!mRunDvars.isEmpty())
			Args << mRunDvars;

		Args << "+set" << "fs_game" << (LastMod.isEmpty() ? LastMap : LastMod);

		if (!LastMap.isEmpty())
			Args << "+devmap" << LastMap;

		QString ExtraOptions = mRunOptionsWidget->text();
		if (!ExtraOptions.isEmpty())
			Args << ExtraOptions.split(' ');

		Commands.append(QPair<QString, QStringList>(QString("%1/BlackOps3.exe").arg(mGamePath), Args));
	}

	if (Commands.size() == 0 && !UpdateAdded)
	{
		QMessageBox::information(this, "No Tasks", "Please selected at least one file from the list and one action to be performed.");
		return;
	}

	StartBuildThread(Commands);
}

void mlMainWindow::OnEditPublish()
{
	std::function<QTreeWidgetItem* (QTreeWidgetItem*)> SearchCheckedItem=[&](QTreeWidgetItem* ParentItem) -> QTreeWidgetItem*
	{
		for (int ChildIdx = 0; ChildIdx < ParentItem->childCount(); ChildIdx++)
		{
			QTreeWidgetItem* Child = ParentItem->child(ChildIdx);
			if (Child->checkState(0) == Qt::Checked)
				return Child;

			QTreeWidgetItem* Checked = SearchCheckedItem(Child);
			if (Checked)
				return Checked;
		}

		return nullptr;
	};

	QTreeWidgetItem* Item = SearchCheckedItem(mFileListWidget->invisibleRootItem());
	if (!Item)
	{
		QMessageBox::warning(this, "Error", "No maps or mods checked.");
		return;
	}

	QString Folder;
	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		Folder = "usermaps/" + Item->text(0);
		mType = "map";
		mFolderName = Item->text(0);
	}
	else
	{
		Folder = "mods/" + Item->parent()->text(0);
		mType = "mod";
		mFolderName = Item->parent()->text(0);
	}

	mWorkshopFolder = QString("%1/%2/zone").arg(mGamePath, Folder);
	QFile File(mWorkshopFolder + "/workshop.json");

	if (!QFileInfo(mWorkshopFolder).isDir())
	{
		QMessageBox::information(this, "Error", QString("The folder '%1' does not exist.").arg(mWorkshopFolder));
		return;
	}

	mFileId = 0;
	mTitle.clear();
	mDescription.clear();
	mThumbnail.clear();
	mTags.clear();

	if (File.open(QIODevice::ReadOnly))
	{
		QJsonDocument Document = QJsonDocument::fromJson(File.readAll());
		QJsonObject Root = Document.object();

		mFileId = Root["PublisherID"].toString().toULongLong();
		mTitle = Root["Title"].toString();
		mDescription = Root["Description"].toString();
		mThumbnail = Root["Thumbnail"].toString();
		mTags = Root["Tags"].toString().split(',');
	}

	if (mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->RequestUGCDetails(mFileId, 10);
		mSteamCallResultRequestDetails.Set(SteamAPICall, this, &mlMainWindow::OnUGCRequestUGCDetails);
	}
	else
		ShowPublishDialog();
}

void mlMainWindow::OnUGCRequestUGCDetails(SteamUGCRequestUGCDetailsResult_t* RequestDetailsResult, bool IOFailure)
{
	if (IOFailure || RequestDetailsResult->m_details.m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", "Error retrieving item data from the Steam Workshop.");
		return;
	}

	SteamUGCDetails_t* Details = &RequestDetailsResult->m_details;

	mTitle = Details->m_rgchTitle;
	mDescription = Details->m_rgchDescription;
	mTags = QString(Details->m_rgchTags).split(',');

	ShowPublishDialog();
}

void mlMainWindow::ShowPublishDialog()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Publish Mod");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QFormLayout* FormLayout = new QFormLayout();
	Layout->addLayout(FormLayout);

	QLineEdit* TitleWidget = new QLineEdit();
	TitleWidget->setText(mTitle);
	FormLayout->addRow("Title:", TitleWidget);

	QLineEdit* DescriptionWidget = new QLineEdit();
	DescriptionWidget->setText(mDescription);
	FormLayout->addRow("Description:", DescriptionWidget);

	QLineEdit* ThumbnailEdit = new QLineEdit();
	ThumbnailEdit->setText(mThumbnail);

	QToolButton* ThumbnailButton = new QToolButton();
	ThumbnailButton->setText("...");

	QHBoxLayout* ThumbnailLayout = new QHBoxLayout();
	ThumbnailLayout->setContentsMargins(0, 0, 0, 0);
	ThumbnailLayout->addWidget(ThumbnailEdit);
	ThumbnailLayout->addWidget(ThumbnailButton);

	QWidget* ThumbnailWidget = new QWidget();
	ThumbnailWidget->setLayout(ThumbnailLayout);

	FormLayout->addRow("Thumbnail:", ThumbnailWidget);

	QTreeWidget* TagsTree = new QTreeWidget(&Dialog);
	TagsTree->setHeaderHidden(true);
	TagsTree->setUniformRowHeights(true);
	TagsTree->setRootIsDecorated(false);
	FormLayout->addRow("Tags:", TagsTree);

	for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
	{
		const char* Tag = gTags[TagIdx];
		QTreeWidgetItem* Item = new QTreeWidgetItem(TagsTree, QStringList() << Tag);
		Item->setCheckState(0, mTags.contains(Tag) ? Qt::Checked : Qt::Unchecked);
	}

	QFrame* Frame = new QFrame();
	Frame->setFrameShape(QFrame::HLine);
	Frame->setFrameShadow(QFrame::Raised);
	Layout->addWidget(Frame);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	auto ThumbnailBrowse = [=]()
	{
		QString FileName = QFileDialog::getOpenFileName(this, "Open Thumbnail", QString(), "All Files (*.*)");
		if (!FileName.isEmpty())
			ThumbnailEdit->setText(FileName);
	};

	connect(ThumbnailButton, &QToolButton::clicked, ThumbnailBrowse);
	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mTitle = TitleWidget->text();
	mDescription = DescriptionWidget->text();
	mThumbnail = ThumbnailEdit->text();
	mTags.clear();

	for (int ChildIdx = 0; ChildIdx < TagsTree->topLevelItemCount(); ChildIdx++)
	{
		QTreeWidgetItem* Child = TagsTree->topLevelItem(ChildIdx);
		if (Child->checkState(0) == Qt::Checked)
			mTags.append(Child->text(0));
	}

	if (!SteamUGC())
	{
		QMessageBox::information(this, "Error", "Could not initialize Steam, make sure you're running the launcher from the Steam client.");
		return;
	}

	if (!mFileId)
	{
		SteamAPICall_t SteamAPICall = SteamUGC()->CreateItem(AppId, k_EWorkshopFileTypeCommunity);
		mSteamCallResultCreateItem.Set(SteamAPICall, this, &mlMainWindow::OnCreateItemResult);
	}
	else
		UpdateWorkshopItem();
}

void mlMainWindow::OnEditOptions()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Options");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QSettings Settings;
	QCheckBox* Checkbox = new QCheckBox("Use Treyarch Theme");
	Checkbox->setToolTip("Toggle between the dark grey Treyarch colors and the default Windows colors");
	Checkbox->setChecked(Settings.value("UseDarkTheme", false).toBool());
	Layout->addWidget(Checkbox);

	QHBoxLayout* LanguageLayout = new QHBoxLayout();
	LanguageLayout->addWidget(new QLabel("Build Language:"));

	QStringList Languages;
	Languages << "All";
	for (int LanguageIdx = 0; LanguageIdx < ARRAYSIZE(gLanguages); LanguageIdx++)
		Languages << gLanguages[LanguageIdx];

	QComboBox* LanguageCombo = new QComboBox();
	LanguageCombo->addItems(Languages);
	LanguageCombo->setCurrentText(mBuildLanguage);
	LanguageLayout->addWidget(LanguageCombo);

	Layout->addLayout(LanguageLayout);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	mBuildLanguage = LanguageCombo->currentText();
	mTreyarchTheme = Checkbox->isChecked();

	Settings.setValue("BuildLanguage", mBuildLanguage);
	Settings.setValue("UseDarkTheme", mTreyarchTheme);

	UpdateTheme();
}

void mlMainWindow::UpdateTheme()
{
	if (mTreyarchTheme)
	{
		qApp->setStyle("plastique");
		QFile file(QString("%1/radiant/stylesheet.qss").arg(mToolsPath));
		file.open(QFile::ReadOnly);
		QString styleSheet = QLatin1String(file.readAll());
		file.close();
		qApp->setStyleSheet(styleSheet);
	}
	else
	{
		qApp->setStyle("WindowsVista");
		qApp->setStyleSheet("");
	}
}

void mlMainWindow::OnEditDvars()
{
	QDialog Dialog(this, Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
	Dialog.setWindowTitle("Dvar Options");

	QVBoxLayout* Layout = new QVBoxLayout(&Dialog);

	QLabel* Label = new QLabel(&Dialog);
	Label->setText("Dvars that are to be used when you run the game.\nMust press \"OK\" in order to save the values!");
	Layout->addWidget(Label);

	QTreeWidget* DvarTree = new QTreeWidget(&Dialog);
	DvarTree->setColumnCount(2);
	DvarTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	DvarTree->setHeaderLabels(QStringList() << "Dvar" << "Value");
	DvarTree->setUniformRowHeights(true);
	DvarTree->setRootIsDecorated(false);
	Layout->addWidget(DvarTree);

	QDialogButtonBox* ButtonBox = new QDialogButtonBox(&Dialog);
	ButtonBox->setOrientation(Qt::Horizontal);
	ButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	ButtonBox->setCenterButtons(true);

	Layout->addWidget(ButtonBox);

	for(int DvarIdx = 0; DvarIdx < ARRAYSIZE(gDvars); DvarIdx++)
		Dvar(gDvars[DvarIdx], DvarTree);

	connect(ButtonBox, SIGNAL(accepted()), &Dialog, SLOT(accept()));
	connect(ButtonBox, SIGNAL(rejected()), &Dialog, SLOT(reject()));

	if (Dialog.exec() != QDialog::Accepted)
		return;

	int size = 0;
	QSettings settings;
	QString dvarName, dvarValue;
	QTreeWidgetItemIterator it(DvarTree);

	mRunDvars.clear();
	while (*it && size < ARRAYSIZE(gDvars))
	{
		QWidget* widget = DvarTree->itemWidget(*it, 1);
		dvarName = (*it)->data(0, 0).toString();
		dvar_s dvar = Dvar::findDvar(dvarName, DvarTree, gDvars, ARRAYSIZE(gDvars));
		switch(dvar.type)
		{
		case DVAR_VALUE_BOOL:
			dvarValue = Dvar::setDvarSetting(dvar, (QCheckBox*)widget);
			break;
		case DVAR_VALUE_INT:
			dvarValue = Dvar::setDvarSetting(dvar, (QSpinBox*)widget);
			break;
		case DVAR_VALUE_STRING:
			dvarValue = Dvar::setDvarSetting(dvar, (QLineEdit*)widget);
			break;
		}

		if(!dvarValue.toLatin1().isEmpty())
		{
			if(!dvar.isCmd)
				mRunDvars << "+set" << dvarName;
			else			// hack for cmds
				mRunDvars << QString("+%1").arg(dvarName);
			mRunDvars << dvarValue;
		}
		size++;
		++it;
	}
}

void mlMainWindow::UpdateWorkshopItem()
{
	QJsonObject Root;

	Root["PublisherID"] = QString::number(mFileId);
	Root["Title"] = mTitle;
	Root["Description"] = mDescription;
	Root["Thumbnail"] = mThumbnail;
	Root["Type"] = mType;
	Root["FolderName"] = mFolderName;
	Root["Tags"] = mTags.join(',');

	QString WorkshopFile(mWorkshopFolder + "/workshop.json");
	QFile File(WorkshopFile);

	if (!File.open(QIODevice::WriteOnly))
	{
		QMessageBox::warning(this, "Error", QString("Error writing to file '%1'.").arg(WorkshopFile));
		return;
	}

	File.write(QJsonDocument(Root).toJson());
	File.close();

	UGCUpdateHandle_t UpdateHandle = SteamUGC()->StartItemUpdate(AppId, mFileId);
	SteamUGC()->SetItemTitle(UpdateHandle, mTitle.toLatin1().constData());
	SteamUGC()->SetItemDescription(UpdateHandle, mDescription.toLatin1().constData());
	SteamUGC()->SetItemPreview(UpdateHandle, mThumbnail.toLatin1().constData());
	SteamUGC()->SetItemContent(UpdateHandle, mWorkshopFolder.toLatin1().constData());

	const char* TagList[ARRAYSIZE(gTags)];
	SteamParamStringArray_t Tags;
	Tags.m_ppStrings = TagList;
	Tags.m_nNumStrings = 0;

	for (const QString& Tag : mTags)
	{
		QByteArray TagStr = Tag.toLatin1();

		for (int TagIdx = 0; TagIdx < ARRAYSIZE(gTags); TagIdx++)
		{
			if (TagStr == gTags[TagIdx])
			{
				TagList[Tags.m_nNumStrings++] = gTags[TagIdx];
				if (Tags.m_nNumStrings == ARRAYSIZE(TagList))
					break;
			}
		}
	}

	SteamUGC()->SetItemTags(UpdateHandle, &Tags);

	SteamAPICall_t SteamAPICall = SteamUGC()->SubmitItemUpdate(UpdateHandle, "");
	mSteamCallResultUpdateItem.Set(SteamAPICall, this, &mlMainWindow::OnUpdateItemResult);

	QProgressDialog Dialog(this);
	Dialog.setLabelText(QString("Uploading workshop item '%1'...").arg(QString::number(mFileId)));
	Dialog.setCancelButton(NULL);
	Dialog.setWindowModality(Qt::WindowModal);
	Dialog.show();

	for (;;)
	{
		uint64 Processed, Total;

		const auto Status = SteamUGC()->GetItemUpdateProgress(SteamAPICall, &Processed, &Total);
		// if we get an invalid status exit out, it could mean we're finished or there's an actual problem
		if (Status == k_EItemUpdateStatusInvalid)
		{
			break;
		}

		switch (Status)
		{
		case EItemUpdateStatus::k_EItemUpdateStatusInvalid:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Invalid" )));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingConfig:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Config")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusPreparingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Preparing Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingContent:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Content")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusUploadingPreviewFile:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Uploading Preview file")));
			break;
		case EItemUpdateStatus::k_EItemUpdateStatusCommittingChanges:
			Dialog.setLabelText(
				QString("Uploading workshop item '%1': %2").arg(QString::number(mFileId), QString("Committing Changes")));
			break;
		}

		Dialog.setMaximum(Total);
		Dialog.setValue(Processed);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		Sleep(100);
	}
}

void mlMainWindow::OnCreateItemResult(CreateItemResult_t* CreateItemResult, bool IOFailure)
{
	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (CreateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error creating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(CreateItemResult->m_eResult));
		return;
	}

	mFileId = CreateItemResult->m_nPublishedFileId;

	UpdateWorkshopItem();
}

void mlMainWindow::OnUpdateItemResult(SubmitItemUpdateResult_t* UpdateItemResult, bool IOFailure)
{
	if (IOFailure)
	{
		QMessageBox::warning(this, "Error", "Disk Read error.");
		return;
	}

	if (UpdateItemResult->m_eResult != k_EResultOK)
	{
		QMessageBox::warning(this, "Error", QString("Error updating Steam Workshop item. Error code: %1\nVisit https://steamerrors.com/ for more information.").arg(UpdateItemResult->m_eResult));
		return;
	}

	if (QMessageBox::question(this, "Update", "Workshop item successfully updated. Do you want to visit the Workshop page for this item now?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
		ShellExecute(NULL, "open", QString("steam://url/CommunityFilePage/%1").arg(QString::number(mFileId)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
}

void mlMainWindow::OnHelpAbout()
{
	QMessageBox::about(this, "About Modtools Launcher", "Treyarch Modtools Launcher\nCopyright 2016 Treyarch");
}

void mlMainWindow::OnOpenZoneFile()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;
	
	QTreeWidgetItem* Item = ItemList[0];

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		ShellExecute(NULL, "open", QString("\"%1/usermaps/%2/zone_source/%3.zone\"").arg(mGamePath, MapName, MapName).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
	else
	{
		QString ModName = Item->parent()->text(0);
		QString ZoneName = Item->text(0);
		ShellExecute(NULL, "open", (QString("\"%1/mods/%2/zone_source/%3.zone\"").arg(mGamePath, ModName, ZoneName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
}

void mlMainWindow::OnOpenModRootFolder()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		ShellExecute(NULL, "open", (QString("\"%1/usermaps/%2\"").arg(mGamePath, MapName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
	else
	{
		QString ModName = Item->parent() ? Item->parent()->text(0) : Item->text(0);
		ShellExecute(NULL, "open", (QString("\"%1/mods/%2\"").arg(mGamePath, ModName)).toLatin1().constData(), "", NULL, SW_SHOWDEFAULT);
	}
}

void mlMainWindow::OnRunMapOrMod()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];

	QStringList Args;

	if(!mRunDvars.isEmpty())
		Args << mRunDvars;

	Args << "+set" << "fs_game";

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		Args << MapName;
		Args << "+devmap" << MapName;
	}
	else
	{
		QString ModName = Item->parent() ? Item->parent()->text(0) : Item->text(0);
		Args << ModName;
	}

	QString ExtraOptions = mRunOptionsWidget->text();
	if (!ExtraOptions.isEmpty())
		Args << ExtraOptions.split(' ');

	QList<QPair<QString, QStringList>> Commands;
	Commands.append(QPair<QString, QStringList>(QString("%1/BlackOps3.exe").arg(mGamePath), Args));
	StartBuildThread(Commands);
}

void mlMainWindow::OnSaveLog() const
{
	// want to make a logs directory for easy management of launcher logs (exe_dir/logs)
	const auto dir = QDir{};
	if (!dir.exists("logs"))
	{
		const auto result = dir.mkdir("logs");
		if (!result)
		{
			QMessageBox::warning(nullptr, "Error", QString("Could not create the \"logs\" directory"));
			return;
		}
	}

	const auto time = std::time(nullptr);
	auto ss = std::stringstream{};
	const auto timeStr = std::put_time(std::localtime(&time), "%F_%T");

	ss << timeStr;

	auto dateStr = ss.str();
	std::replace(dateStr.begin(), dateStr.end(), ':', '_');

	QFile log(QString{ "logs/modlog_%1.txt" }.arg(dateStr.c_str()));

	if (!log.open(QIODevice::WriteOnly))
		return;

	QTextStream stream(&log);
	stream << mOutputWidget->toPlainText();

	QMessageBox::information(nullptr, QString("Save Log"), QString("The console log has been saved to %1").arg(log.fileName()));
}

void mlMainWindow::OnCleanXPaks()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = Item->parent() ? Item->parent()->text(0) : Item->text(0);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	QString fileListString;
	QStringList fileList;
	QDirIterator it(Folder, QStringList() << "*.xpak", QDir::Files, QDirIterator::Subdirectories);
	while (it.hasNext())
	{
		QString filepath = it.next();
		fileList.append(filepath);
		fileListString.append("\n" + QDir(Folder).relativeFilePath(filepath));
	}

	QString relativeFolder = QDir(mGamePath).relativeFilePath(Folder);

	if (fileList.count() == 0)
	{
		QMessageBox::information(this, QString("Clean XPaks (%1)").arg(relativeFolder), QString("There are no XPak's to clean!"));
		return;
	}

	if (QMessageBox::question(this, QString("Clean XPaks (%1)").arg(relativeFolder), QString("Are you sure you want to delete the following files?" + fileListString), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	for (auto file : fileList)
	{
		qDebug() << file;
		QFile(file).remove();
	}
}

void mlMainWindow::OnDelete()
{
	QList<QTreeWidgetItem*> ItemList = mFileListWidget->selectedItems();
	if (ItemList.isEmpty())
		return;

	QTreeWidgetItem* Item = ItemList[0];
	QString Folder;

	if (Item->data(0, Qt::UserRole).toInt() == ML_ITEM_MAP)
	{
		QString MapName = Item->text(0);
		Folder = QString("%1/usermaps/%2").arg(mGamePath, MapName);
	}
	else
	{
		QString ModName = Item->parent() ? Item->parent()->text(0) : Item->text(0);
		Folder = QString("%1/mods/%2").arg(mGamePath, ModName);
	}

	if (QMessageBox::question(this, "Delete Folder", QString("Are you sure you want to delete the folder '%1' and all of its contents?").arg(Folder), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QDir(Folder).removeRecursively();
	PopulateFileList();
}

void mlMainWindow::OnExport2BinChooseDirectory()
{
	const QString dir = QFileDialog::getExistingDirectory(mExport2BinGUIWidget, tr("Open Directory"), mToolsPath, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	this->mExport2BinTargetDirWidget->setText(dir);

	QSettings Settings;
	Settings.setValue("Export2Bin_TargetDir", dir);
}

void mlMainWindow::OnExport2BinToggleOverwriteFiles()
{
	QSettings Settings;
	Settings.setValue("Export2Bin_OverwriteFiles", mExport2BinOverwriteWidget->isChecked());
}

void mlMainWindow::BuildOutputReady(QString Output)
{
	mOutputWidget->appendPlainText(Output);
}

void mlMainWindow::BuildFinished()
{
	mBuildButton->setText("Build");
	mBuildThread->deleteLater();
	mBuildThread = NULL;
}

Export2BinGroupBox::Export2BinGroupBox(QWidget* parent, mlMainWindow* parent_window) : QGroupBox(parent), parentWindow(parent_window)
{
	this->setAcceptDrops(true);
}

void Export2BinGroupBox::dragEnterEvent(QDragEnterEvent* event)
{
	event->acceptProposedAction();
}

void Export2BinGroupBox::dropEvent(QDropEvent* event)
{
	const QMimeData* mimeData = event->mimeData();

	if (parentWindow == NULL)
	{
		return;
	}

	if (mimeData->hasUrls())
	{
		QStringList pathList;
		QList<QUrl> urlList = mimeData->urls();

		QDir working_dir(parentWindow->mToolsPath);
		for (int i = 0; i < urlList.size(); i++)
		{
			pathList.append(urlList.at(i).toLocalFile());
		}
		
		QProcess* Process = new QProcess();
		connect(Process, SIGNAL(finished(int)), Process, SLOT(deleteLater()));

		bool allowOverwrite = this->parentWindow->mExport2BinOverwriteWidget->isChecked();

		QString outputDir = parentWindow->mExport2BinTargetDirWidget->text();
		parentWindow->StartConvertThread(pathList, outputDir, allowOverwrite);
		
		event->acceptProposedAction();
	}
}

void Export2BinGroupBox::dragLeaveEvent(QDragLeaveEvent* event)
{
	event->accept();
}
