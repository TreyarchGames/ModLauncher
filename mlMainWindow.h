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

#pragma once

class mlBuildThread : public QThread
{
	Q_OBJECT

public:
	mlBuildThread(const QList<QPair<QString, QStringList>>& Commands, bool IgnoreErrors);
	void run();
	bool Succeeded() const
	{
		return mSuccess;
	}

	void Cancel()
	{
		mCancel = true;
	}

signals:
	void OutputReady(const QString& Output);

protected:
	QList<QPair<QString, QStringList>> mCommands;
	bool mSuccess;
	bool mCancel;
	bool mIgnoreErrors;
};

class mlConvertThread : public QThread
{
	Q_OBJECT

public:
	mlConvertThread(QStringList& Files, QString& OutputDir, bool IgnoreErrors, bool mOverwrite);
	void run();
	bool Succeeded() const
	{
		return mSuccess;
	}

	void Cancel()
	{
		mCancel = true;
	}

signals:
	void OutputReady(const QString& Output);

protected:
	QStringList mFiles;
	QString mOutputDir;
	bool mOverwrite;

	bool mSuccess;
	bool mCancel;
	bool mIgnoreErrors;
};

class mlMainWindow : public QMainWindow
{
	Q_OBJECT

	friend class Export2BinGroupBox;

public:
	mlMainWindow();
	~mlMainWindow();

	void UpdateDB();

	void OnCreateItemResult(CreateItemResult_t* CreateItemResult, bool IOFailure);
	CCallResult<mlMainWindow, CreateItemResult_t> mSteamCallResultCreateItem;

	void OnUpdateItemResult(SubmitItemUpdateResult_t* UpdateItemResult, bool IOFailure);
	CCallResult<mlMainWindow, SubmitItemUpdateResult_t> mSteamCallResultUpdateItem;

	void OnUGCRequestUGCDetails(SteamUGCRequestUGCDetailsResult_t* RequestDetailsResult, bool IOFailure);
	CCallResult<mlMainWindow, SteamUGCRequestUGCDetailsResult_t> mSteamCallResultRequestDetails;

protected slots:
	void OnFileNew();
	void OnFileAssetEditor();
	void OnFileLevelEditor();
	void OnFileExport2Bin();
	void OnEditBuild();
	void OnEditPublish();
	void OnEditOptions();
	void OnEditDvars();
	void OnHelpAbout();
	void OnOpenZoneFile();
	void OnOpenModRootFolder();
	void OnRunMapOrMod();
	void OnSaveLog() const;
	void OnCleanXPaks();
	void OnDelete();
	void OnExport2BinChooseDirectory();
	void OnExport2BinToggleOverwriteFiles();
	void BuildOutputReady(QString Output);
	void BuildFinished();
	void ContextMenuRequested();
	void SteamUpdate();

protected:
	void closeEvent(QCloseEvent* Event);

	void StartBuildThread(const QList<QPair<QString, QStringList>>& Commands);
	void mlMainWindow::StartConvertThread(QStringList& pathList, QString& outputDir, bool allowOverwrite);

	void PopulateFileList();
	void UpdateWorkshopItem();
	void ShowPublishDialog();
	void UpdateTheme();

	void CreateActions();
	void CreateMenu();
	void CreateToolBar();

	void InitExport2BinGUI();

	QAction* mActionFileNew;
	QAction* mActionFileAssetEditor;
	QAction* mActionFileLevelEditor;
	QAction* mActionFileExport2Bin;
	QAction* mActionFileExit;
	QAction* mActionEditBuild;
	QAction* mActionEditPublish;
	QAction* mActionEditOptions;
	QAction* mActionHelpAbout;

	QTreeWidget* mFileListWidget;
	QPlainTextEdit* mOutputWidget;

	QPushButton* mBuildButton;
	QPushButton* mDvarsButton;
	QPushButton* mLogButton;
	QCheckBox* mCompileEnabledWidget;
	QComboBox* mCompileModeWidget;
	QCheckBox* mLightEnabledWidget;
	QComboBox* mLightQualityWidget;
	QCheckBox* mLinkEnabledWidget;
	QCheckBox* mRunEnabledWidget;
	QLineEdit* mRunOptionsWidget;
	QCheckBox* mIgnoreErrorsWidget;

	mlBuildThread* mBuildThread;
	mlConvertThread* mConvertThread;

	QDockWidget* mExport2BinGUIWidget;
	QCheckBox* mExport2BinOverwriteWidget;
	QLineEdit* mExport2BinTargetDirWidget;

	bool mTreyarchTheme;
	QString mBuildLanguage;

	QStringList mShippedMapList;
	QTimer mTimer;

	quint64 mFileId;
	QString mTitle;
	QString mDescription;
	QString mThumbnail;
	QString mWorkshopFolder;
	QString mFolderName;
	QString mType;
	QStringList mTags;

	QString mGamePath;
	QString mToolsPath;

	QStringList mRunDvars;
};

class Export2BinGroupBox : public QGroupBox
{
private:
	mlMainWindow* parentWindow;

protected:
	void dragEnterEvent(QDragEnterEvent* event);
	void dragLeaveEvent(QDragLeaveEvent* event);
	void dropEvent(QDropEvent *event);

public:
	Export2BinGroupBox(QWidget *parent, mlMainWindow* parent_window);
};
