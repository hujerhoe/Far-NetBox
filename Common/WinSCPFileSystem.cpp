//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <StrUtils.hpp>
#include "WinSCPFileSystem.h"
#include "WinSCPPlugin.h"
#include "FarDialog.h"
#include "FarTexts.h"
#include "FarConfiguration.h"
#include "farkeys.hpp"
#include <Common.h>
#include <Exceptions.h>
#include <SessionData.h>
#include <CoreMain.h>
#include <SysUtils.hpp>
#include <ScpFileSystem.h>
#include <Bookmarks.h>
#include <GUITools.h>
#include <CompThread.hpp>
// FAR WORKAROUND
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
__fastcall TSessionPanelItem::TSessionPanelItem(TSessionData * ASessionData):
  TCustomFarPanelItem()
{
  assert(ASessionData);
  FSessionData = ASessionData;
}
//---------------------------------------------------------------------------
void __fastcall TSessionPanelItem::SetPanelModes(TFarPanelModes * PanelModes)
{
  assert(FarPlugin);
  TStrings * ColumnTitles = new TStringList();
  try
  {
    ColumnTitles->Add(FarPlugin->GetMsg(SESSION_NAME_COL_TITLE));
    for (int Index = 0; Index < PANEL_MODES_COUNT; Index++)
    {
      PanelModes->SetPanelMode(Index, "N", "0", ColumnTitles, false, false, false);
    }
  }
  __finally
  {
    delete ColumnTitles;
  }
}
//---------------------------------------------------------------------------
void __fastcall TSessionPanelItem::SetKeyBarTitles(TFarKeyBarTitles * KeyBarTitles)
{
  KeyBarTitles->ClearKeyBarTitle(fsNone, 6);
  KeyBarTitles->SetKeyBarTitle(fsNone, 5, FarPlugin->GetMsg(EXPORT_SESSION_KEYBAR));
  KeyBarTitles->ClearKeyBarTitle(fsShift, 1, 3);
  KeyBarTitles->SetKeyBarTitle(fsShift, 4, FarPlugin->GetMsg(NEW_SESSION_KEYBAR));
  KeyBarTitles->SetKeyBarTitle(fsShift, 5, FarPlugin->GetMsg(COPY_SESSION_KEYBAR));
  KeyBarTitles->SetKeyBarTitle(fsShift, 6, FarPlugin->GetMsg(RENAME_SESSION_KEYBAR));
  KeyBarTitles->ClearKeyBarTitle(fsShift, 7, 8);
  KeyBarTitles->ClearKeyBarTitle(fsAlt, 6);
  KeyBarTitles->ClearKeyBarTitle(fsCtrl, 4, 11);
}
//---------------------------------------------------------------------------
void __fastcall TSessionPanelItem::GetData(
  unsigned long & /*Flags*/, AnsiString & FileName, __int64 & /*Size*/,
  unsigned long & /*FileAttributes*/,
  TDateTime & /*LastWriteTime*/, TDateTime & /*LastAccess*/,
  unsigned long & /*NumberOfLinks*/, AnsiString & /*Description*/,
  AnsiString & /*Owner*/, void *& UserData, int & /*CustomColumnNumber*/)
{
  FileName = UnixExtractFileName(FSessionData->Name);
  UserData = FSessionData;
}
//---------------------------------------------------------------------------
__fastcall TSessionFolderPanelItem::TSessionFolderPanelItem(AnsiString Folder):
  TCustomFarPanelItem(),
  FFolder(Folder)
{
}
//---------------------------------------------------------------------------
void __fastcall TSessionFolderPanelItem::GetData(
  unsigned long & /*Flags*/, AnsiString & FileName, __int64 & /*Size*/,
  unsigned long & FileAttributes,
  TDateTime & /*LastWriteTime*/, TDateTime & /*LastAccess*/,
  unsigned long & /*NumberOfLinks*/, AnsiString & /*Description*/,
  AnsiString & /*Owner*/, void *& /*UserData*/, int & /*CustomColumnNumber*/)
{
  FileName = FFolder;
  FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
}
//---------------------------------------------------------------------------
__fastcall TRemoteFilePanelItem::TRemoteFilePanelItem(TRemoteFile * ARemoteFile):
  TCustomFarPanelItem()
{
  assert(ARemoteFile);
  FRemoteFile = ARemoteFile;
}
//---------------------------------------------------------------------------
void __fastcall TRemoteFilePanelItem::GetData(
  unsigned long & /*Flags*/, AnsiString & FileName, __int64 & Size,
  unsigned long & FileAttributes,
  TDateTime & LastWriteTime, TDateTime & LastAccess,
  unsigned long & /*NumberOfLinks*/, AnsiString & /*Description*/,
  AnsiString & Owner, void *& UserData, int & CustomColumnNumber)
{
  FileName = FRemoteFile->FileName;
  Size = FRemoteFile->Size;
  FileAttributes =
    FLAGMASK(FRemoteFile->IsDirectory, FILE_ATTRIBUTE_DIRECTORY) |
    FLAGMASK(FRemoteFile->IsHidden, FILE_ATTRIBUTE_HIDDEN) |
    FLAGMASK(FRemoteFile->Rights->ReadOnly, FILE_ATTRIBUTE_READONLY) |
    FLAGMASK(FRemoteFile->IsSymLink, FILE_ATTRIBUTE_REPARSE_POINT);
  LastWriteTime = FRemoteFile->Modification;
  LastAccess = FRemoteFile->LastAccess;
  Owner = FRemoteFile->Owner;
  UserData = FRemoteFile;
  CustomColumnNumber = 4;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TRemoteFilePanelItem::CustomColumnData(int Column)
{
  switch (Column) {
    case 0: return FRemoteFile->Group;
    case 1: return FRemoteFile->RightsStr;
    case 2: return FRemoteFile->Rights->Octal;
    case 3: return FRemoteFile->LinkTo;
    default: assert(false); return AnsiString();
  }
}
//---------------------------------------------------------------------------
void __fastcall TRemoteFilePanelItem::TranslateColumnTypes(AnsiString & ColumnTypes,
  TStrings * ColumnTitles)
{
  AnsiString AColumnTypes = ColumnTypes;
  ColumnTypes = "";
  AnsiString Column;
  AnsiString Title;
  while (!AColumnTypes.IsEmpty())
  {
    Column = CutToChar(AColumnTypes, ',', false);
    if (Column == "G")
    {
      Column = "C0";
      Title = FarPlugin->GetMsg(GROUP_COL_TITLE);
    }
    else if (Column == "R")
    {
      Column = "C1";
      Title = FarPlugin->GetMsg(RIGHTS_COL_TITLE);
    }
    else if (Column == "RO")
    {
      Column = "C2";
      Title = FarPlugin->GetMsg(RIGHTS_OCTAL_COL_TITLE);
    }
    else if (Column == "L")
    {
      Column = "C3";
      Title = FarPlugin->GetMsg(LINK_TO_COL_TITLE);
    }
    else
    {
      Title = "";
    }
    ColumnTypes += (ColumnTypes.IsEmpty() ? "" : ",") + Column;
    if (ColumnTitles)
    {
      ColumnTitles->Add(Title);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TRemoteFilePanelItem::SetPanelModes(TFarPanelModes * PanelModes)
{
  assert(FarPlugin);
  TStrings * ColumnTitles = new TStringList();
  try
  {
    if (FarConfiguration->CustomPanelModeDetailed)
    {
      AnsiString ColumnTypes = FarConfiguration->ColumnTypesDetailed;
      AnsiString StatusColumnTypes = FarConfiguration->StatusColumnTypesDetailed;

      TranslateColumnTypes(ColumnTypes, ColumnTitles);
      TranslateColumnTypes(StatusColumnTypes, NULL);

      PanelModes->SetPanelMode(5 /*detailed */,
        ColumnTypes, FarConfiguration->ColumnWidthsDetailed,
        ColumnTitles, FarConfiguration->FullScreenDetailed, false, true, false,
        StatusColumnTypes, FarConfiguration->StatusColumnWidthsDetailed);
    }
  }
  __finally
  {
    delete ColumnTitles;
  }
}
//---------------------------------------------------------------------------
void __fastcall TRemoteFilePanelItem::SetKeyBarTitles(TFarKeyBarTitles * KeyBarTitles)
{
  KeyBarTitles->ClearKeyBarTitle(fsShift, 1, 3); // archive commands
  KeyBarTitles->SetKeyBarTitle(fsShift, 5, FarPlugin->GetMsg(COPY_TO_FILE_KEYBAR));
  KeyBarTitles->SetKeyBarTitle(fsShift, 6, FarPlugin->GetMsg(MOVE_TO_FILE_KEYBAR));
  KeyBarTitles->SetKeyBarTitle(fsAltShift, 12,
    FarPlugin->GetMsg(OPEN_DIRECTORY_KEYBAR));
  KeyBarTitles->SetKeyBarTitle(fsAltShift, 6,
    FarPlugin->GetMsg(RENAME_FILE_KEYBAR));
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class TFarInteractiveCustomCommand : public TInteractiveCustomCommand
{
public:
  TFarInteractiveCustomCommand(TCustomFarPlugin * Plugin,
    TCustomCommand * ChildCustomCommand);

protected:
  virtual void __fastcall Prompt(int Index, const AnsiString & Prompt,
    AnsiString & Value);

private:
  TCustomFarPlugin * FPlugin;
};
//---------------------------------------------------------------------------
TFarInteractiveCustomCommand::TFarInteractiveCustomCommand(
  TCustomFarPlugin * Plugin, TCustomCommand * ChildCustomCommand) :
  TInteractiveCustomCommand(ChildCustomCommand)
{
  FPlugin = Plugin;
}
//---------------------------------------------------------------------------
void __fastcall TFarInteractiveCustomCommand::Prompt(int /*Index*/,
  const AnsiString & Prompt, AnsiString & Value)
{
  AnsiString APrompt = Prompt;
  if (APrompt.IsEmpty())
  {
    APrompt = FPlugin->GetMsg(APPLY_COMMAND_PARAM_PROMPT);
  }
  if (!FPlugin->InputBox(FPlugin->GetMsg(APPLY_COMMAND_PARAM_TITLE),
        APrompt, Value, 0, APPLY_COMMAND_PARAM_HISTORY))
  {
    Abort();
  }
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Attempt to allow keepalives from background thread.
// Not finished nor used.
class TKeepaliveThread : public TCompThread
{
public:
  __fastcall TKeepaliveThread(TWinSCPFileSystem * FileSystem, TDateTime Interval);
  virtual void __fastcall Execute();
  virtual void __fastcall Terminate();

private:
  TWinSCPFileSystem * FFileSystem;
  TDateTime FInterval;
  HANDLE FEvent;
};
//---------------------------------------------------------------------------
__fastcall TKeepaliveThread::TKeepaliveThread(TWinSCPFileSystem * FileSystem,
  TDateTime Interval) :
  TCompThread(true)
{
  FEvent = CreateEvent(NULL, false, false, NULL);

  FFileSystem = FileSystem;
  FInterval = Interval;
  Resume();
}
//---------------------------------------------------------------------------
void __fastcall TKeepaliveThread::Terminate()
{
  TCompThread::Terminate();
  SetEvent(FEvent);
}
//---------------------------------------------------------------------------
void __fastcall TKeepaliveThread::Execute()
{
  while (!Terminated)
  {
    static long MillisecondsPerDay = 24 * 60 * 60 * 1000;
    if ((WaitForSingleObject(FEvent, double(FInterval) * MillisecondsPerDay) != WAIT_FAILED) &&
        !Terminated)
    {
      FFileSystem->KeepaliveThreadCallback();
    }
  }
  CloseHandle(FEvent);
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
__fastcall TWinSCPFileSystem::TWinSCPFileSystem(TCustomFarPlugin * APlugin) :
  TCustomFarFileSystem(APlugin)
{
  FReloadDirectory = false;
  FProgressSaveScreenHandle = 0;
  FSynchronizationSaveScreenHandle = 0;
  FAuthenticationSaveScreenHandle = 0;
  FFileList = NULL;
  FPanelItems = NULL;
  FSavedFindFolder = "";
  FTerminal = NULL;
  FQueue = NULL;
  FQueueStatus = NULL;
  FQueueStatusSection = new TCriticalSection();
  FQueueStatusInvalidated = false;
  FQueueItemInvalidated = false;
  FRefreshLocalDirectory = false;
  FRefreshRemoteDirectory = false;
  FQueueEventPending = false;
  FNoProgress = false;
  FNoProgressFinish = false;
  FKeepaliveThread = NULL;
  FSynchronisingBrowse = false;
  FSynchronizeController = NULL;
  FCapturedLog = NULL;
  FAuthenticationLog = NULL;
  FLastEditorID = -1;
  FLoadingSessionList = false;
  FPathHistory = new TStringList;
}
//---------------------------------------------------------------------------
__fastcall TWinSCPFileSystem::~TWinSCPFileSystem()
{
  if (FTerminal)
  {
    SaveSession();
  }
  assert(FSynchronizeController == NULL);
  assert(!FAuthenticationSaveScreenHandle);
  assert(!FProgressSaveScreenHandle);
  assert(!FSynchronizationSaveScreenHandle);
  assert(!FFileList);
  assert(!FPanelItems);
  delete FPathHistory;
  FPathHistory = NULL;
  delete FQueue;
  FQueue = NULL;
  delete FQueueStatus;
  FQueueStatus = NULL;
  delete FQueueStatusSection;
  FQueueStatusSection = NULL;
  if (FTerminal != NULL)
  {
    GUIConfiguration->SynchronizeBrowsing = FSynchronisingBrowse;
  }
  SAFE_DESTROY(FTerminal);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::HandleException(Exception * E, int OpMode)
{
  if ((Terminal != NULL) && E->InheritsFrom(__classid(EFatal)))
  {
    if (!FClosed)
    {
      ClosePlugin();
    }
    Terminal->ShowExtendedException(E);
  }
  else
  {
    TCustomFarFileSystem::HandleException(E, OpMode);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::KeepaliveThreadCallback()
{
  TGuard Guard(FCriticalSection);

  if (Connected())
  {
    FTerminal->Idle();
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::SessionList()
{
  return (FTerminal == NULL);
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::Connected()
{
  // Check for active added to avoid "disconnected" message popup repeatedly
  // from "idle"
  return !SessionList() && FTerminal->Active;
}
//---------------------------------------------------------------------------
TWinSCPPlugin * __fastcall TWinSCPFileSystem::WinSCPPlugin()
{
  return dynamic_cast<TWinSCPPlugin*>(FPlugin);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::Close()
{
  try
  {
    SAFE_DESTROY(FKeepaliveThread);

    if (Connected())
    {
      assert(FQueue != NULL);
      if (!FQueue->IsEmpty &&
          (MoreMessageDialog(GetMsg(PENDING_QUEUE_ITEMS), NULL, qtWarning,
             qaOK | qaCancel) == qaOK))
      {
        QueueShow(true);
      }
    }
  }
  __finally
  {
    TCustomFarFileSystem::Close();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::GetOpenPluginInfoEx(long unsigned & Flags,
  AnsiString & /*HostFile*/, AnsiString & CurDir, AnsiString & Format,
  AnsiString & PanelTitle, TFarPanelModes * PanelModes, int & /*StartPanelMode*/,
  int & /*StartSortMode*/, bool & /*StartSortOrder*/, TFarKeyBarTitles * KeyBarTitles,
  AnsiString & ShortcutData)
{
  if (!SessionList())
  {
    Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING |
      OPIF_SHOWPRESERVECASE | OPIF_COMPAREFATTIME;

    // When slash is added to the end of path, windows style paths
    // (vandyke: c:/windows/system) are displayed correctly on command-line, but
    // leaved subdirectory is not focused, when entering parent directory.
    CurDir = FTerminal->CurrentDirectory;
    Format = FTerminal->SessionData->SessionName;
    if (FarConfiguration->HostNameInTitle)
    {
      PanelTitle = ::FORMAT(" %s:%s ", (Format, CurDir));
    }
    else
    {
      PanelTitle = ::FORMAT(" %s ", (CurDir));
    }
    ShortcutData = ::FORMAT("%s\1%s", (FTerminal->SessionData->SessionUrl, CurDir));

    TRemoteFilePanelItem::SetPanelModes(PanelModes);
    TRemoteFilePanelItem::SetKeyBarTitles(KeyBarTitles);
  }
  else
  {
    CurDir = FSessionsFolder;
    Format = "winscp";
    Flags = OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING | OPIF_ADDDOTS | OPIF_SHOWPRESERVECASE;
    PanelTitle = ::FORMAT(" %s ", (GetMsg(STORED_SESSION_TITLE)));

    TSessionPanelItem::SetPanelModes(PanelModes);
    TSessionPanelItem::SetKeyBarTitles(KeyBarTitles);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::GetFindDataEx(TList * PanelItems, int OpMode)
{
  bool Result;
  if (Connected())
  {
    assert(!FNoProgress);
    // OPM_FIND is used also for calculation of directory size (F3, quick view).
    // However directory is usually read from SetDirectory, so FNoProgress
    // seems to have no effect here.
    // Do not know if OPM_SILENT is even used.
    FNoProgress = FLAGSET(OpMode, OPM_FIND) || FLAGSET(OpMode, OPM_SILENT);
    try
    {
      if (FReloadDirectory && FTerminal->Active)
      {
        FReloadDirectory = false;
        FTerminal->ReloadDirectory();
      }

      TRemoteFile * File;
      for (int Index = 0; Index < FTerminal->Files->Count; Index++)
      {
        File = FTerminal->Files->Files[Index];
        PanelItems->Add(new TRemoteFilePanelItem(File));
      }
    }
    __finally
    {
      FNoProgress = false;
    }
    Result = true;
  }
  else if (SessionList())
  {
    Result = true;
    assert(StoredSessions);
    StoredSessions->Load();

    AnsiString Folder = UnixIncludeTrailingBackslash(FSessionsFolder);
    TSessionData * Data;
    TStringList * ChildPaths = new TStringList();
    try
    {
      ChildPaths->CaseSensitive = false;

      for (int Index = 0; Index < StoredSessions->Count; Index++)
      {
        Data = StoredSessions->Sessions[Index];
        if (Data->Name.SubString(1, Folder.Length()) == Folder)
        {
          AnsiString Name = Data->Name.SubString(
            Folder.Length() + 1, Data->Name.Length() - Folder.Length());
          int Slash = Name.Pos('/');
          if (Slash > 0)
          {
            Name.SetLength(Slash - 1);
            if (ChildPaths->IndexOf(Name) < 0)
            {
              PanelItems->Add(new TSessionFolderPanelItem(Name));
              ChildPaths->Add(Name);
            }
          }
          else
          {
            PanelItems->Add(new TSessionPanelItem(Data));
          }
        }
      }
    }
    __finally
    {
      delete ChildPaths;
    }

    if (!FNewSessionsFolder.IsEmpty())
    {
      PanelItems->Add(new TSessionFolderPanelItem(FNewSessionsFolder));
    }

    if (PanelItems->Count == 0)
    {
      PanelItems->Add(new THintPanelItem(GetMsg(NEW_SESSION_HINT)));
    }

    TWinSCPFileSystem * OppositeFileSystem =
      dynamic_cast<TWinSCPFileSystem *>(GetOppositeFileSystem());
    if ((OppositeFileSystem != NULL) && !OppositeFileSystem->Connected() &&
        !OppositeFileSystem->FLoadingSessionList)
    {
      FLoadingSessionList = true;
      try
      {
        UpdatePanel(false, true);
        RedrawPanel(true);
      }
      __finally
      {
        FLoadingSessionList = false;
      }
    }
  }
  else
  {
    Result = false;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::DuplicateRenameSession(TSessionData * Data,
  bool Duplicate)
{
  assert(Data);
  AnsiString Name = Data->Name;
  if (FPlugin->InputBox(GetMsg(Duplicate ? DUPLICATE_SESSION_TITLE : RENAME_SESSION_TITLE),
        GetMsg(Duplicate ? DUPLICATE_SESSION_PROMPT : RENAME_SESSION_PROMPT),
        Name, 0) &&
      !Name.IsEmpty() && (Name != Data->Name))
  {
    TNamedObject * EData = StoredSessions->FindByName(Name);
    if ((EData != NULL) && (EData != Data))
    {
      throw Exception(FORMAT(GetMsg(SESSION_ALREADY_EXISTS_ERROR), (Name)));
    }
    else
    {
      TSessionData * NData = StoredSessions->NewSession(Name, Data);
      FSessionsFolder = UnixExcludeTrailingBackslash(UnixExtractFilePath(Name));

      // change of letter case during duplication degrades the operation to rename
      if (!Duplicate || (Data == NData))
      {
        Data->Remove();
        if (NData != Data)
        {
          StoredSessions->Remove(Data);
        }
      }

      // modified only, explicit
      StoredSessions->Save(false, true);

      if (UpdatePanel())
      {
        RedrawPanel();

        FocusSession(NData);
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::FocusSession(TSessionData * Data)
{
  TFarPanelItem * SessionItem = PanelInfo->FindUserData(Data);
  if (SessionItem != NULL)
  {
    PanelInfo->FocusedItem = SessionItem;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::EditConnectSession(TSessionData * Data, bool Edit)
{
  TSessionData * OrigData = Data;
  bool NewData = !Data;
  bool FillInConnect = !Edit && !Data->CanLogin;

  if (NewData || FillInConnect)
  {
    Data = new TSessionData("");
  }

  try
  {
    if (FillInConnect)
    {
      Data->Assign(OrigData);
      Data->Name = "";
    }

    TSessionAction Action;
    if (Edit || FillInConnect)
    {
      Action = (FillInConnect ? saConnect : (OrigData == NULL ? saAdd : saEdit));
      if (SessionDialog(Data, Action))
      {
        TSessionData * SelectSession = NULL;
        if ((!NewData && !FillInConnect) || (Action != saConnect))
        {
          if (NewData)
          {
            AnsiString Name =
              UnixIncludeTrailingBackslash(FSessionsFolder) + Data->SessionName;
            if (FPlugin->InputBox(GetMsg(NEW_SESSION_NAME_TITLE),
                  GetMsg(NEW_SESSION_NAME_PROMPT), Name, 0) &&
                !Name.IsEmpty())
            {
              if (StoredSessions->FindByName(Name))
              {
                throw Exception(FORMAT(GetMsg(SESSION_ALREADY_EXISTS_ERROR), (Name)));
              }
              else
              {
                SelectSession = StoredSessions->NewSession(Name, Data);
                FSessionsFolder = UnixExcludeTrailingBackslash(UnixExtractFilePath(Name));
              }
            }
          }
          else if (FillInConnect)
          {
            AnsiString OrigName = OrigData->Name;
            OrigData->Assign(Data);
            OrigData->Name = OrigName;
          }

          // modified only, explicit
          StoredSessions->Save(false, true);
          if (UpdatePanel())
          {
            if (SelectSession != NULL)
            {
              FocusSession(SelectSession);
            }
            // rarely we need to redraw even when new session is created
            // (e.g. when there there were only the focused hint line before)
            RedrawPanel();
          }
        }
      }
    }
    else
    {
      Action = saConnect;
    }

    if ((Action == saConnect) && Connect(Data))
    {
      if (UpdatePanel())
      {
        RedrawPanel();
        if (PanelInfo->ItemCount)
        {
          PanelInfo->FocusedIndex = 0;
        }
      }
    }
  }
  __finally
  {
    if (NewData || FillInConnect)
    {
      delete Data;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::ProcessEventEx(int Event, void * Param)
{
  bool Result = false;
  if (Connected())
  {
    if (Event == FE_COMMAND)
    {
      AnsiString Command = (char *)Param;
      if (!Command.Trim().IsEmpty() &&
          (Command.SubString(1, 3).LowerCase() != "cd "))
      {
        Result = ExecuteCommand(Command);
      }
    }
    else if (Event == FE_IDLE)
    {
      // FAR WORKAROUND
      // Control(FCTL_CLOSEPLUGIN) does not seem to close plugin when called from
      // ProcessEvent(FE_IDLE). So if TTerminal::Idle() causes session to close
      // we must count on having ProcessEvent(FE_IDLE) called again.
      FTerminal->Idle();
      if (FQueue != NULL)
      {
        FQueue->Idle();
      }
      ProcessQueue(true);
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalCaptureLog(
  const AnsiString & AddedLine, bool /*StdError*/)
{
  if (FOutputLog)
  {
    FPlugin->WriteConsole(AddedLine + "\n");
  }
  if (FCapturedLog != NULL)
  {
    FCapturedLog->Add(AddedLine);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::RequireLocalPanel(TFarPanelInfo * Panel, AnsiString Message)
{
  if (Panel->IsPlugin || (Panel->Type != ptFile))
  {
    throw Exception(Message);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::RequireCapability(int Capability)
{
  if (!FTerminal->IsCapable[static_cast<TFSCapability>(Capability)])
  {
    throw Exception(FORMAT(GetMsg(OPERATION_NOT_SUPPORTED),
      (FTerminal->GetFileSystemInfo().ProtocolName)));
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::EnsureCommandSessionFallback(TFSCapability Capability)
{
  bool Result = FTerminal->IsCapable[Capability] ||
    FTerminal->CommandSessionOpened;

  if (!Result)
  {
    if (!GUIConfiguration->ConfirmCommandSession)
    {
      Result = true;
    }
    else
    {
      TMessageParams Params;
      Params.Params = qpNeverAskAgainCheck;
      int Answer = MoreMessageDialog(FORMAT(GetMsg(PERFORM_ON_COMMAND_SESSION),
        (FTerminal->GetFileSystemInfo().ProtocolName,
         FTerminal->GetFileSystemInfo().ProtocolName)), NULL,
        qtConfirmation, qaOK | qaCancel, &Params);
      if (Answer == qaNeverAskAgain)
      {
        GUIConfiguration->ConfirmCommandSession = false;
        Result = true;
      }
      else
      {
        Result = (Answer == qaOK);
      }
    }

    if (Result)
    {
      ConnectTerminal(FTerminal->CommandSession);
    }
  }

  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::ExecuteCommand(const AnsiString Command)
{
  if (FTerminal->AllowedAnyCommand(Command) &&
      EnsureCommandSessionFallback(fcAnyCommand))
  {
    FTerminal->BeginTransaction();
    try
    {
      FarControl(FCTL_SETCMDLINE, NULL);
      FPlugin->ShowConsoleTitle(Command);
      try
      {
        FPlugin->ShowTerminalScreen();

        FOutputLog = true;
        FTerminal->AnyCommand(Command, TerminalCaptureLog);
      }
      __finally
      {
        FPlugin->ScrollTerminalScreen(1);
        FPlugin->SaveTerminalScreen();
        FPlugin->ClearConsoleTitle();
      }
    }
    __finally
    {
      if (FTerminal->Active)
      {
        FTerminal->EndTransaction();
        UpdatePanel();
      }
      else
      {
        RedrawPanel();
        RedrawPanel(true);
      }
    }
  }
  return true;
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::ProcessKeyEx(int Key, unsigned int ControlState)
{
  bool Handled = false;

  TFarPanelItem * Focused = PanelInfo->FocusedItem;

  if ((Key == 'W') && (ControlState & PKF_SHIFT) &&
        (ControlState & PKF_ALT))
  {
    WinSCPPlugin()->CommandsMenu(true);
    Handled = true;
  }
  else if (SessionList())
  {
    TSessionData * Data = NULL;

    if ((Focused != NULL) && Focused->IsFile && Focused->UserData)
    {
      Data = (TSessionData *)Focused->UserData;
    }

    if ((Key == 'F') && FLAGSET(ControlState, PKF_CONTROL))
    {
      InsertSessionNameOnCommandLine();
      Handled = true;
    }

    if ((Key == VK_RETURN) && FLAGSET(ControlState, PKF_CONTROL))
    {
      InsertSessionNameOnCommandLine();
      Handled = true;
    }

    if (Key == VK_RETURN && (ControlState == 0) && Data)
    {
      EditConnectSession(Data, false);
      Handled = true;
    }

    if (Key == VK_F4 && (ControlState == 0))
    {
      if ((Data != NULL) || (StoredSessions->Count == 0))
      {
        EditConnectSession(Data, true);
      }
      Handled = true;
    }

    if (Key == VK_F4 && (ControlState & PKF_SHIFT))
    {
      EditConnectSession(NULL, true);
      Handled = true;
    }

    if (((Key == VK_F5) || (Key == VK_F6)) &&
        (ControlState & PKF_SHIFT))
    {
      if (Data != NULL)
      {
        DuplicateRenameSession(Data, Key == VK_F5);
      }
      Handled = true;
    }
  }
  else if (Connected())
  {
    if ((Key == 'F') && (ControlState & PKF_CONTROL))
    {
      InsertFileNameOnCommandLine(true);
      Handled = true;
    }

    if ((Key == VK_RETURN) && FLAGSET(ControlState, PKF_CONTROL))
    {
      InsertFileNameOnCommandLine(false);
      Handled = true;
    }

    if ((Key == 'R') && (ControlState & PKF_CONTROL))
    {
      FReloadDirectory = true;
    }

    if ((Key == 'A') && (ControlState & PKF_CONTROL))
    {
      FileProperties();
      Handled = true;
    }

    if ((Key == 'G') && (ControlState & PKF_CONTROL))
    {
      ApplyCommand();
      Handled = true;
    }

    if ((Key == 'Q') && (ControlState & PKF_SHIFT) &&
          (ControlState & PKF_ALT))
    {
      QueueShow(false);
      Handled = true;
    }

    if ((Key == 'B') && (ControlState & PKF_CONTROL) &&
          (ControlState & PKF_ALT))
    {
      ToggleSynchronizeBrowsing();
      Handled = true;
    }

    if ((Key == VK_INSERT) &&
        (FLAGSET(ControlState, PKF_ALT | PKF_SHIFT) || FLAGSET(ControlState, PKF_CONTROL | PKF_ALT)))
    {
      CopyFullFileNamesToClipboard();
      Handled = true;
    }

    if ((Key == VK_F6) && ((ControlState & (PKF_ALT | PKF_SHIFT)) == PKF_ALT))
    {
      CreateLink();
      Handled = true;
    }

    if (Focused && ((Key == VK_F5) || (Key == VK_F6)) &&
        ((ControlState & (PKF_ALT | PKF_SHIFT)) == PKF_SHIFT))
    {
      TransferFiles((Key == VK_F6));
      Handled = true;
    }

    if (Focused && (Key == VK_F6) &&
        ((ControlState & (PKF_ALT | PKF_SHIFT)) == (PKF_SHIFT | PKF_ALT)))
    {
      RenameFile();
      Handled = true;
    }

    if ((Key == VK_F12) && (ControlState & PKF_SHIFT) &&
        (ControlState & PKF_ALT))
    {
      OpenDirectory(false);
      Handled = true;
    }

    if ((Key == VK_F4) && (ControlState == 0) &&
         FarConfiguration->EditorMultiple)
    {
      MultipleEdit();
      Handled = true;
    }
  }
  return Handled;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::CreateLink()
{
  RequireCapability(fcResolveSymlink);
  RequireCapability(fcSymbolicLink);

  bool Edit = false;
  TRemoteFile * File = NULL;
  AnsiString FileName;
  AnsiString PointTo;
  bool SymbolicLink = true;

  if (PanelInfo->FocusedItem && PanelInfo->FocusedItem->UserData)
  {
    File = (TRemoteFile *)PanelInfo->FocusedItem->UserData;

    Edit = File->IsSymLink && Terminal->SessionData->ResolveSymlinks;
    if (Edit)
    {
      FileName = File->FileName;
      PointTo = File->LinkTo;
    }
    else
    {
      PointTo = File->FileName;
    }
  }

  if (LinkDialog(FileName, PointTo, SymbolicLink, Edit,
        Terminal->IsCapable[fcHardLink]))
  {
    if (Edit)
    {
      assert(File->FileName == FileName);
      int Params = dfNoRecursive;
      Terminal->ExceptionOnFail = true;
      try
      {
        Terminal->DeleteFile("", File, &Params);
      }
      __finally
      {
        Terminal->ExceptionOnFail = false;
      }
    }
    Terminal->CreateLink(FileName, PointTo, SymbolicLink);
    if (UpdatePanel())
    {
      RedrawPanel();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TemporarilyDownloadFiles(
  TStrings * FileList, TCopyParamType CopyParam, AnsiString & TempDir)
{
  CopyParam.FileNameCase = ncNoChange;
  CopyParam.PreserveReadOnly = false;
  CopyParam.ResumeSupport = rsOff;

  TempDir = FPlugin->TemporaryDir();
  if (TempDir.IsEmpty() || !ForceDirectories(TempDir))
  {
    throw Exception(FMTLOAD(CREATE_TEMP_DIR_ERROR, (TempDir)));
  }

  FTerminal->ExceptionOnFail = true;
  try
  {
    try
    {
      FTerminal->CopyToLocal(FileList, TempDir, &CopyParam, cpTemporary);
    }
    catch(...)
    {
      try
      {
        RecursiveDeleteFile(ExcludeTrailingBackslash(TempDir), false);
      }
      catch(...)
      {
      }
      throw;
    }
  }
  __finally
  {
    FTerminal->ExceptionOnFail = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ApplyCommand()
{
  TStrings * FileList = CreateSelectedFileList(osRemote);
  if (FileList != NULL)
  {
    try
    {
      int Params = FarConfiguration->ApplyCommandParams;
      AnsiString Command = FarConfiguration->ApplyCommandCommand;
      if (ApplyCommandDialog(Command, Params))
      {
        FarConfiguration->ApplyCommandParams = Params;
        FarConfiguration->ApplyCommandCommand = Command;
        if (FLAGCLEAR(Params, ccLocal))
        {
          if (EnsureCommandSessionFallback(fcShellAnyCommand))
          {
            TCustomCommandData Data(Terminal);
            TRemoteCustomCommand RemoteCustomCommand(Data, Terminal->CurrentDirectory);
            TFarInteractiveCustomCommand InteractiveCustomCommand(
              FPlugin, &RemoteCustomCommand);

            Command = InteractiveCustomCommand.Complete(Command, false);

            try
            {
              TCaptureOutputEvent OutputEvent = NULL;
              FOutputLog = false;
              if (FLAGSET(Params, ccShowResults))
              {
                assert(!FNoProgress);
                FNoProgress = true;
                FOutputLog = true;
                OutputEvent = TerminalCaptureLog;
              }

              if (FLAGSET(Params, ccCopyResults))
              {
                assert(FCapturedLog == NULL);
                FCapturedLog = new TStringList();
                OutputEvent = TerminalCaptureLog;
              }

              try
              {
                if (FLAGSET(Params, ccShowResults))
                {
                  FPlugin->ShowTerminalScreen();
                }

                FTerminal->CustomCommandOnFiles(Command, Params, FileList, OutputEvent);
              }
              __finally
              {
                if (FLAGSET(Params, ccShowResults))
                {
                  FNoProgress = false;
                  FPlugin->ScrollTerminalScreen(1);
                  FPlugin->SaveTerminalScreen();
                }

                if (FLAGSET(Params, ccCopyResults))
                {
                  FPlugin->FarCopyToClipboard(FCapturedLog);
                  SAFE_DESTROY(FCapturedLog);
                }
              }
            }
            __finally
            {
              PanelInfo->ApplySelection();
              if (UpdatePanel())
              {
                RedrawPanel();
              }
            }
          }
        }
        else
        {
          TCustomCommandData Data(Terminal);
          TLocalCustomCommand LocalCustomCommand(Data, Terminal->CurrentDirectory);
          TFarInteractiveCustomCommand InteractiveCustomCommand(FPlugin,
            &LocalCustomCommand);

          Command = InteractiveCustomCommand.Complete(Command, false);

          TStrings * LocalFileList = NULL;
          TStrings * RemoteFileList = NULL;
          try
          {
            bool FileListCommand = LocalCustomCommand.IsFileListCommand(Command);
            bool LocalFileCommand = LocalCustomCommand.HasLocalFileName(Command);

            if (LocalFileCommand)
            {
              TFarPanelInfo * AnotherPanel = AnotherPanelInfo;
              RequireLocalPanel(AnotherPanel, GetMsg(APPLY_COMMAND_LOCAL_PATH_REQUIRED));

              LocalFileList = CreateSelectedFileList(osLocal, AnotherPanel);

              if (FileListCommand)
              {
                if ((LocalFileList == NULL) || (LocalFileList->Count != 1))
                {
                  throw Exception(GetMsg(CUSTOM_COMMAND_SELECTED_UNMATCH1));
                }
              }
              else
              {
                if ((LocalFileList == NULL) ||
                    ((LocalFileList->Count != 1) &&
                     (FileList->Count != 1) &&
                     (LocalFileList->Count != FileList->Count)))
                {
                  throw Exception(GetMsg(CUSTOM_COMMAND_SELECTED_UNMATCH));
                }
              }
            }

            AnsiString TempDir;

            TemporarilyDownloadFiles(FileList, GUIConfiguration->DefaultCopyParam, TempDir);

            try
            {
              RemoteFileList = new TStringList();

              TMakeLocalFileListParams MakeFileListParam;
              MakeFileListParam.FileList = RemoteFileList;
              MakeFileListParam.IncludeDirs = FLAGSET(Params, ccApplyToDirectories);
              MakeFileListParam.Recursive =
                FLAGSET(Params, ccRecursive) && !FileListCommand;

              ProcessLocalDirectory(TempDir, &FTerminal->MakeLocalFileList, &MakeFileListParam);

              TFileOperationProgressType Progress(&OperationProgress, &OperationFinished);

              Progress.Start(foCustomCommand, osRemote, FileListCommand ? 1 : FileList->Count);

              try
              {
                if (FileListCommand)
                {
                  AnsiString LocalFile;
                  AnsiString FileList = MakeFileList(RemoteFileList);

                  if (LocalFileCommand)
                  {
                    assert(LocalFileList->Count == 1);
                    LocalFile = LocalFileList->Strings[0];
                  }

                  TCustomCommandData Data(FTerminal);
                  TLocalCustomCommand CustomCommand(Data,
                    Terminal->CurrentDirectory, "", LocalFile, FileList);
                  ExecuteShellAndWait(FPlugin->Handle, CustomCommand.Complete(Command, true),
                    TProcessMessagesEvent(NULL));
                }
                else if (LocalFileCommand)
                {
                  if (LocalFileList->Count == 1)
                  {
                    AnsiString LocalFile = LocalFileList->Strings[0];

                    for (int Index = 0; Index < RemoteFileList->Count; Index++)
                    {
                      AnsiString FileName = RemoteFileList->Strings[Index];
                      TCustomCommandData Data(FTerminal);
                      TLocalCustomCommand CustomCommand(Data,
                        Terminal->CurrentDirectory, FileName, LocalFile, "");
                      ExecuteShellAndWait(FPlugin->Handle,
                        CustomCommand.Complete(Command, true), TProcessMessagesEvent(NULL));
                    }
                  }
                  else if (RemoteFileList->Count == 1)
                  {
                    AnsiString FileName = RemoteFileList->Strings[0];

                    for (int Index = 0; Index < LocalFileList->Count; Index++)
                    {
                      TCustomCommandData Data(FTerminal);
                      TLocalCustomCommand CustomCommand(
                        Data, Terminal->CurrentDirectory,
                        FileName, LocalFileList->Strings[Index], "");
                      ExecuteShellAndWait(FPlugin->Handle,
                        CustomCommand.Complete(Command, true), TProcessMessagesEvent(NULL));
                    }
                  }
                  else
                  {
                    if (LocalFileList->Count != RemoteFileList->Count)
                    {
                      throw Exception(GetMsg(CUSTOM_COMMAND_PAIRS_DOWNLOAD_FAILED));
                    }

                    for (int Index = 0; Index < LocalFileList->Count; Index++)
                    {
                      AnsiString FileName = RemoteFileList->Strings[Index];
                      TCustomCommandData Data(FTerminal);
                      TLocalCustomCommand CustomCommand(
                        Data, Terminal->CurrentDirectory,
                        FileName, LocalFileList->Strings[Index], "");
                      ExecuteShellAndWait(FPlugin->Handle,
                        CustomCommand.Complete(Command, true), TProcessMessagesEvent(NULL));
                    }
                  }
                }
                else
                {
                  for (int Index = 0; Index < RemoteFileList->Count; Index++)
                  {
                    TCustomCommandData Data(FTerminal);
                    TLocalCustomCommand CustomCommand(Data,
                      Terminal->CurrentDirectory, RemoteFileList->Strings[Index], "", "");
                    ExecuteShellAndWait(FPlugin->Handle,
                      CustomCommand.Complete(Command, true), TProcessMessagesEvent(NULL));
                  }
                }
              }
              __finally
              {
                Progress.Stop();
              }
            }
            __finally
            {
              RecursiveDeleteFile(ExcludeTrailingBackslash(TempDir), false);
            }
          }
          __finally
          {
            delete RemoteFileList;
            delete LocalFileList;
          }
        }
      }
    }
    __finally
    {
      delete FileList;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::Synchronize(const AnsiString LocalDirectory,
  const AnsiString RemoteDirectory, TTerminal::TSynchronizeMode Mode,
  const TCopyParamType & CopyParam, int Params, TSynchronizeChecklist ** Checklist,
  TSynchronizeOptions * Options)
{
  TSynchronizeChecklist * AChecklist = NULL;
  try
  {
    FPlugin->SaveScreen(FSynchronizationSaveScreenHandle);
    FPlugin->ShowConsoleTitle(GetMsg(SYNCHRONIZE_PROGRESS_COMPARE_TITLE));
    FSynchronizationStart = Now();
    FSynchronizationCompare = true;
    try
    {
      AChecklist = FTerminal->SynchronizeCollect(LocalDirectory, RemoteDirectory,
        Mode, &CopyParam, Params | TTerminal::spNoConfirmation,
        TerminalSynchronizeDirectory, Options);
    }
    __finally
    {
      FPlugin->ClearConsoleTitle();
      FPlugin->RestoreScreen(FSynchronizationSaveScreenHandle);
    }

    FPlugin->SaveScreen(FSynchronizationSaveScreenHandle);
    FPlugin->ShowConsoleTitle(GetMsg(SYNCHRONIZE_PROGRESS_TITLE));
    FSynchronizationStart = Now();
    FSynchronizationCompare = false;
    try
    {
      FTerminal->SynchronizeApply(AChecklist, LocalDirectory, RemoteDirectory,
        &CopyParam, Params | TTerminal::spNoConfirmation,
        TerminalSynchronizeDirectory);
    }
    __finally
    {
      FPlugin->ClearConsoleTitle();
      FPlugin->RestoreScreen(FSynchronizationSaveScreenHandle);
    }
  }
  __finally
  {
    if (Checklist == NULL)
    {
      delete AChecklist;
    }
    else
    {
      *Checklist = AChecklist;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::SynchronizeAllowSelectedOnly()
{
  return
    (PanelInfo->SelectedCount > 0) ||
    (AnotherPanelInfo->SelectedCount > 0);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::GetSynchronizeOptions(
  int Params, TSynchronizeOptions & Options)
{
  if (FLAGSET(Params, spSelectedOnly) && SynchronizeAllowSelectedOnly())
  {
    Options.Filter = new TStringList();
    Options.Filter->CaseSensitive = false;
    Options.Filter->Duplicates = dupAccept;

    if (PanelInfo->SelectedCount > 0)
    {
      CreateFileList(PanelInfo->Items, osRemote, true, "", true, Options.Filter);
    }
    if (AnotherPanelInfo->SelectedCount > 0)
    {
      CreateFileList(AnotherPanelInfo->Items, osLocal, true, "", true, Options.Filter);
    }
    Options.Filter->Sort();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::FullSynchronize(bool Source)
{
  TFarPanelInfo * AnotherPanel = AnotherPanelInfo;
  RequireLocalPanel(AnotherPanel, GetMsg(SYNCHRONIZE_LOCAL_PATH_REQUIRED));

  AnsiString LocalDirectory = AnotherPanel->CurrentDirectory;
  AnsiString RemoteDirectory = FTerminal->CurrentDirectory;

  bool SaveMode = !(GUIConfiguration->SynchronizeModeAuto < 0);
  TTerminal::TSynchronizeMode Mode =
    (SaveMode ? (TTerminal::TSynchronizeMode)GUIConfiguration->SynchronizeModeAuto :
      (Source ? TTerminal::smLocal : TTerminal::smRemote));
  int Params = GUIConfiguration->SynchronizeParams;
  bool SaveSettings = false;

  TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
  TUsableCopyParamAttrs CopyParamAttrs = Terminal->UsableCopyParamAttrs(0);
  int Options =
    FLAGMASK(!FTerminal->IsCapable[fcTimestampChanging], fsoDisableTimestamp) |
    FLAGMASK(SynchronizeAllowSelectedOnly(), fsoAllowSelectedOnly);
  if (FullSynchronizeDialog(Mode, Params, LocalDirectory, RemoteDirectory,
        &CopyParam, SaveSettings, SaveMode, Options, CopyParamAttrs))
  {
    TSynchronizeOptions SynchronizeOptions;
    GetSynchronizeOptions(Params, SynchronizeOptions);

    if (SaveSettings)
    {
      GUIConfiguration->SynchronizeParams = Params;
      if (SaveMode)
      {
        GUIConfiguration->SynchronizeModeAuto = Mode;
      }
    }

    TSynchronizeChecklist * Checklist = NULL;
    try
    {
      FPlugin->SaveScreen(FSynchronizationSaveScreenHandle);
      FPlugin->ShowConsoleTitle(GetMsg(SYNCHRONIZE_PROGRESS_COMPARE_TITLE));
      FSynchronizationStart = Now();
      FSynchronizationCompare = true;
      try
      {
        Checklist = FTerminal->SynchronizeCollect(LocalDirectory, RemoteDirectory,
          Mode, &CopyParam, Params | TTerminal::spNoConfirmation,
          TerminalSynchronizeDirectory, &SynchronizeOptions);
      }
      __finally
      {
        FPlugin->ClearConsoleTitle();
        FPlugin->RestoreScreen(FSynchronizationSaveScreenHandle);
      }

      if (Checklist->Count == 0)
      {
        MoreMessageDialog(GetMsg(COMPARE_NO_DIFFERENCES), NULL,
           qtInformation, qaOK);
      }
      else if (FLAGCLEAR(Params, TTerminal::spPreviewChanges) ||
               SynchronizeChecklistDialog(Checklist, Mode, Params,
                 LocalDirectory, RemoteDirectory))
      {
        if (FLAGSET(Params, TTerminal::spPreviewChanges))
        {
          FSynchronizationStart = Now();
        }
        FPlugin->SaveScreen(FSynchronizationSaveScreenHandle);
        FPlugin->ShowConsoleTitle(GetMsg(SYNCHRONIZE_PROGRESS_TITLE));
        FSynchronizationStart = Now();
        FSynchronizationCompare = false;
        try
        {
          FTerminal->SynchronizeApply(Checklist, LocalDirectory, RemoteDirectory,
            &CopyParam, Params | TTerminal::spNoConfirmation,
            TerminalSynchronizeDirectory);
        }
        __finally
        {
          FPlugin->ClearConsoleTitle();
          FPlugin->RestoreScreen(FSynchronizationSaveScreenHandle);
        }
      }
    }
    __finally
    {
      delete Checklist;
      if (UpdatePanel())
      {
        RedrawPanel();
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalSynchronizeDirectory(
  const AnsiString LocalDirectory, const AnsiString RemoteDirectory,
  bool & Continue, bool Collect)
{
  static unsigned long LastTicks;
  unsigned long Ticks = GetTickCount();
  if ((LastTicks == 0) || (Ticks - LastTicks > 500))
  {
    LastTicks = Ticks;

    static const int ProgressWidth = 48;
    static AnsiString ProgressTitle;
    static AnsiString ProgressTitleCompare;
    static AnsiString LocalLabel;
    static AnsiString RemoteLabel;
    static AnsiString StartTimeLabel;
    static AnsiString TimeElapsedLabel;

    if (ProgressTitle.IsEmpty())
    {
      ProgressTitle = GetMsg(SYNCHRONIZE_PROGRESS_TITLE);
      ProgressTitleCompare = GetMsg(SYNCHRONIZE_PROGRESS_COMPARE_TITLE);
      LocalLabel = GetMsg(SYNCHRONIZE_PROGRESS_LOCAL);
      RemoteLabel = GetMsg(SYNCHRONIZE_PROGRESS_REMOTE);
      StartTimeLabel = GetMsg(SYNCHRONIZE_PROGRESS_START_TIME);
      TimeElapsedLabel = GetMsg(SYNCHRONIZE_PROGRESS_ELAPSED);
    }

    AnsiString Message;

    Message = LocalLabel + MinimizeName(LocalDirectory,
      ProgressWidth - LocalLabel.Length(), false);
    Message += AnsiString::StringOfChar(' ', ProgressWidth - Message.Length()) + "\n";
    Message += RemoteLabel + MinimizeName(RemoteDirectory,
      ProgressWidth - RemoteLabel.Length(), true) + "\n";
    Message += StartTimeLabel + FSynchronizationStart.TimeString() + "\n";
    Message += TimeElapsedLabel +
      FormatDateTimeSpan(Configuration->TimeFormat, Now() - FSynchronizationStart) + "\n";

    FPlugin->Message(0, (Collect ? ProgressTitleCompare : ProgressTitle), Message);

    if (FPlugin->CheckForEsc() &&
        (MoreMessageDialog(GetMsg(CANCEL_OPERATION), NULL,
          qtConfirmation, qaOK | qaCancel) == qaOK))
    {
      Continue = false;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::Synchronize()
{
  TFarPanelInfo * AnotherPanel = AnotherPanelInfo;
  RequireLocalPanel(AnotherPanel, GetMsg(SYNCHRONIZE_LOCAL_PATH_REQUIRED));

  TSynchronizeParamType Params;
  Params.LocalDirectory = AnotherPanel->CurrentDirectory;
  Params.RemoteDirectory = FTerminal->CurrentDirectory;
  int UnusedParams = (GUIConfiguration->SynchronizeParams &
    (TTerminal::spPreviewChanges | TTerminal::spTimestamp |
     TTerminal::spNotByTime | TTerminal::spBySize));
  Params.Params = GUIConfiguration->SynchronizeParams & ~UnusedParams;
  Params.Options = GUIConfiguration->SynchronizeOptions;
  bool SaveSettings = false;
  TSynchronizeController Controller(&DoSynchronize, &DoSynchronizeInvalid,
    &DoSynchronizeTooManyDirectories);
  assert(FSynchronizeController == NULL);
  FSynchronizeController = &Controller;

  try
  {
    TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
    int CopyParamAttrs = Terminal->UsableCopyParamAttrs(0).Upload;
    int Options =
      FLAGMASK(SynchronizeAllowSelectedOnly(), soAllowSelectedOnly);
    if (SynchronizeDialog(Params, &CopyParam, Controller.StartStop,
          SaveSettings, Options, CopyParamAttrs, GetSynchronizeOptions) &&
        SaveSettings)
    {
      GUIConfiguration->SynchronizeParams = Params.Params | UnusedParams;
      GUIConfiguration->SynchronizeOptions = Params.Options;
    }
  }
  __finally
  {
    FSynchronizeController = NULL;
    // plugin might have been closed during some synchronisation already
    if (!FClosed)
    {
      if (UpdatePanel())
      {
        RedrawPanel();
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::DoSynchronize(
  TSynchronizeController * /*Sender*/, const AnsiString LocalDirectory,
  const AnsiString RemoteDirectory, const TCopyParamType & CopyParam,
  const TSynchronizeParamType & Params, TSynchronizeChecklist ** Checklist,
  TSynchronizeOptions * Options, bool Full)
{
  try
  {
    int PParams = Params.Params;
    if (!Full)
    {
      PParams |= TTerminal::spNoRecurse | TTerminal::spUseCache |
        TTerminal::spDelayProgress | TTerminal::spSubDirs;
    }
    else
    {
      // if keepuptodate is non-recursive,
      // full sync before has to be non-recursive as well
      if (FLAGCLEAR(Params.Options, soRecurse))
      {
        PParams |= TTerminal::spNoRecurse;
      }
    }
    Synchronize(LocalDirectory, RemoteDirectory, TTerminal::smRemote, CopyParam,
      PParams, Checklist, Options);
  }
  catch(Exception & E)
  {
    HandleException(&E);
    throw;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::DoSynchronizeInvalid(
  TSynchronizeController * /*Sender*/, const AnsiString Directory,
  const AnsiString ErrorStr)
{
  AnsiString Message;
  if (!Directory.IsEmpty())
  {
    Message = FORMAT(GetMsg(WATCH_ERROR_DIRECTORY), (Directory));
  }
  else
  {
    Message = GetMsg(WATCH_ERROR_GENERAL);
  }

  MoreMessageDialog(Message, NULL, qtError, qaOK);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::DoSynchronizeTooManyDirectories(
  TSynchronizeController * /*Sender*/, int & MaxDirectories)
{
  if (MaxDirectories < GUIConfiguration->MaxWatchDirectories)
  {
    MaxDirectories = GUIConfiguration->MaxWatchDirectories;
  }
  else
  {
    TMessageParams Params;
    Params.Params = qpNeverAskAgainCheck;
    int Result = MoreMessageDialog(
      FORMAT(GetMsg(TOO_MANY_WATCH_DIRECTORIES), (MaxDirectories, MaxDirectories)), NULL,
      qtConfirmation, qaYes | qaNo, &Params);

    if ((Result == qaYes) || (Result == qaNeverAskAgain))
    {
      MaxDirectories *= 2;
      if (Result == qaNeverAskAgain)
      {
        GUIConfiguration->MaxWatchDirectories = MaxDirectories;
      }
    }
    else
    {
      Abort();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::CustomCommandGetParamValue(
  const AnsiString AName, AnsiString & Value)
{
  AnsiString Name = AName;
  if (Name.IsEmpty())
  {
    Name = GetMsg(APPLY_COMMAND_PARAM_PROMPT);
  }
  if (!FPlugin->InputBox(GetMsg(APPLY_COMMAND_PARAM_TITLE),
        Name, Value, 0, APPLY_COMMAND_PARAM_HISTORY))
  {
    Abort();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TransferFiles(bool Move)
{
  if (Move)
  {
    RequireCapability(fcRemoteMove);
  }

  if (Move || EnsureCommandSessionFallback(fcRemoteCopy))
  {
    TStrings * FileList = CreateSelectedFileList(osRemote);
    if (FileList)
    {
      assert(!FPanelItems);

      try
      {
        AnsiString Target = FTerminal->CurrentDirectory;
        AnsiString FileMask = "*.*";
        if (RemoteTransferDialog(FileList, Target, FileMask, Move))
        {
          try
          {
            if (Move)
            {
              Terminal->MoveFiles(FileList, Target, FileMask);
            }
            else
            {
              Terminal->CopyFiles(FileList, Target, FileMask);
            }
          }
          __finally
          {
            PanelInfo->ApplySelection();
            if (UpdatePanel())
            {
              RedrawPanel();
            }
          }
        }
      }
      __finally
      {
        delete FileList;
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::RenameFile()
{
  TFarPanelItem * PanelItem = PanelInfo->FocusedItem;
  assert(PanelItem != NULL);

  if (!PanelItem->IsParentDirectory)
  {
    RequireCapability(fcRename);

    TRemoteFile * File = static_cast<TRemoteFile *>(PanelItem->UserData);
    AnsiString NewName = File->FileName;
    if (RenameFileDialog(File, NewName))
    {
      try
      {
        Terminal->RenameFile(File, NewName, true);
      }
      __finally
      {
        if (UpdatePanel())
        {
          RedrawPanel();
        }
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::FileProperties()
{
  TStrings * FileList = CreateSelectedFileList(osRemote);
  if (FileList)
  {
    assert(!FPanelItems);

    try
    {
      TRemoteProperties CurrentProperties;

      bool Cont = true;
      if (!Terminal->LoadFilesProperties(FileList))
      {
        if (UpdatePanel())
        {
          RedrawPanel();
        }
        else
        {
          Cont = false;
        }
      }

      if (Cont)
      {
        CurrentProperties = TRemoteProperties::CommonProperties(FileList);

        int Flags = 0;
        if (FTerminal->IsCapable[fcModeChanging]) Flags |= cpMode;
        if (FTerminal->IsCapable[fcOwnerChanging]) Flags |= cpOwner;
        if (FTerminal->IsCapable[fcGroupChanging]) Flags |= cpGroup;

        TRemoteProperties NewProperties = CurrentProperties;
        if (PropertiesDialog(FileList, FTerminal->CurrentDirectory,
            FTerminal->Groups, FTerminal->Users, &NewProperties, Flags))
        {
          NewProperties = TRemoteProperties::ChangedProperties(CurrentProperties,
            NewProperties);
          try
          {
            FTerminal->ChangeFilesProperties(FileList, &NewProperties);
          }
          __finally
          {
            PanelInfo->ApplySelection();
            if (UpdatePanel())
            {
              RedrawPanel();
            }
          }
        }
      }
    }
    __finally
    {
      delete FileList;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::InsertTokenOnCommandLine(AnsiString Token, bool Separate)
{
  if (!Token.IsEmpty())
  {
    if (Token.Pos(" ") > 0)
    {
      Token = FORMAT("\"%s\"", (Token));
    }

    if (Separate)
    {
      Token += " ";
    }

    FarControl(FCTL_INSERTCMDLINE, Token.c_str());
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::InsertSessionNameOnCommandLine()
{
  TFarPanelItem * Focused = PanelInfo->FocusedItem;

  if (Focused != NULL)
  {
    TSessionData * SessionData = reinterpret_cast<TSessionData *>(Focused->UserData);
    AnsiString Name;
    if (SessionData != NULL)
    {
      Name = SessionData->Name;
    }
    else
    {
      Name = UnixIncludeTrailingBackslash(FSessionsFolder);
      if (!Focused->IsParentDirectory)
      {
        Name = UnixIncludeTrailingBackslash(Name + Focused->FileName);
      }
    }
    InsertTokenOnCommandLine(Name, true);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::InsertFileNameOnCommandLine(bool Full)
{
  TFarPanelItem * Focused = PanelInfo->FocusedItem;

  if (Focused != NULL)
  {
    if (!Focused->IsParentDirectory)
    {
      TRemoteFile * File = reinterpret_cast<TRemoteFile *>(Focused->UserData);
      if (File != NULL)
      {
        if (Full)
        {
          InsertTokenOnCommandLine(File->FullFileName, true);
        }
        else
        {
          InsertTokenOnCommandLine(File->FileName, true);
        }
      }
    }
    else
    {
      InsertTokenOnCommandLine(UnixIncludeTrailingBackslash(FTerminal->CurrentDirectory), true);
    }
  }
}
//---------------------------------------------------------------------------
// not used
void __fastcall TWinSCPFileSystem::InsertPathOnCommandLine()
{
  InsertTokenOnCommandLine(FTerminal->CurrentDirectory, false);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::CopyFullFileNamesToClipboard()
{
  TStrings * FileList = CreateSelectedFileList(osRemote);
  TStrings * FileNames = new TStringList();
  try
  {
    if (FileList != NULL)
    {
      for (int Index = 0; Index < FileList->Count; Index++)
      {
        TRemoteFile * File = reinterpret_cast<TRemoteFile *>(FileList->Objects[Index]);
        if (File != NULL)
        {
          FileNames->Add(File->FullFileName);
        }
        else
        {
          assert(false);
        }
      }
    }
    else
    {
      if ((PanelInfo->SelectedCount == 0) &&
          PanelInfo->FocusedItem->IsParentDirectory)
      {
        FileNames->Add(UnixIncludeTrailingBackslash(FTerminal->CurrentDirectory));
      }
    }

    FPlugin->FarCopyToClipboard(FileNames);
  }
  __finally
  {
    delete FileList;
    delete FileNames;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::GetSpaceAvailable(const AnsiString Path,
  TSpaceAvailable & ASpaceAvailable, bool & Close)
{
  // terminal can be already closed (e.g. dropped connection)
  if ((Terminal != NULL) && Terminal->IsCapable[fcCheckingSpaceAvailable])
  {
    try
    {
      Terminal->SpaceAvailable(Path, ASpaceAvailable);
    }
    catch(Exception & E)
    {
      if (!Terminal->Active)
      {
        Close = true;
      }
      HandleException(&E);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ShowInformation()
{
  TSessionInfo SessionInfo = Terminal->GetSessionInfo();
  TFileSystemInfo FileSystemInfo = Terminal->GetFileSystemInfo();
  TGetSpaceAvailable OnGetSpaceAvailable = NULL;
  if (Terminal->IsCapable[fcCheckingSpaceAvailable])
  {
    OnGetSpaceAvailable = GetSpaceAvailable;
  }
  FileSystemInfoDialog(SessionInfo, FileSystemInfo, Terminal->CurrentDirectory,
    OnGetSpaceAvailable);
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::AreCachesEmpty()
{
  assert(Connected());
  return FTerminal->AreCachesEmpty;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ClearCaches()
{
  assert(Connected());
  FTerminal->ClearCaches();
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::OpenSessionInPutty()
{
  assert(Connected());
  ::OpenSessionInPutty(GUIConfiguration->PuttyPath, FTerminal->SessionData,
    GUIConfiguration->PuttyPassword ? Terminal->Password : AnsiString());
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::QueueShow(bool ClosingPlugin)
{
  assert(Connected());
  assert(FQueueStatus != NULL);
  QueueDialog(FQueueStatus, ClosingPlugin);
  ProcessQueue(true);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::OpenDirectory(bool Add)
{
  TBookmarkList * BookmarkList = new TBookmarkList();
  try
  {
    AnsiString Directory = FTerminal->CurrentDirectory;
    AnsiString SessionKey = FTerminal->SessionData->SessionKey;
    TBookmarkList * CurrentBookmarkList;

    CurrentBookmarkList = FarConfiguration->Bookmarks[SessionKey];
    if (CurrentBookmarkList != NULL)
    {
      BookmarkList->Assign(CurrentBookmarkList);
    }

    if (Add)
    {
      TBookmark * Bookmark = new TBookmark;
      Bookmark->Remote = Directory;
      Bookmark->Name = Directory;
      BookmarkList->Add(Bookmark);
      FarConfiguration->Bookmarks[SessionKey] = BookmarkList;
    }

    bool Result = OpenDirectoryDialog(Add, Directory, BookmarkList);

    FarConfiguration->Bookmarks[SessionKey] = BookmarkList;

    if (Result)
    {
      FTerminal->ChangeDirectory(Directory);
      if (UpdatePanel(true))
      {
        RedrawPanel();
      }
    }
  }
  __finally
  {
    delete BookmarkList;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::HomeDirectory()
{
  FTerminal->HomeDirectory();
  if (UpdatePanel(true))
  {
    RedrawPanel();
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::IsSynchronizedBrowsing()
{
  return FSynchronisingBrowse;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ToggleSynchronizeBrowsing()
{
  FSynchronisingBrowse = !FSynchronisingBrowse;

  if (FarConfiguration->ConfirmSynchronizedBrowsing)
  {
    AnsiString Message = FSynchronisingBrowse ?
      GetMsg(SYNCHRONIZE_BROWSING_ON) : GetMsg(SYNCHRONIZE_BROWSING_OFF);
    TMessageParams Params;
    Params.Params = qpNeverAskAgainCheck;
    if (MoreMessageDialog(Message, NULL, qtInformation, qaOK, &Params) ==
          qaNeverAskAgain)
    {
      FarConfiguration->ConfirmSynchronizedBrowsing = false;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::SynchronizeBrowsing(AnsiString NewPath)
{
  bool Result;
  TFarPanelInfo * AnotherPanel = AnotherPanelInfo;
  AnsiString OldPath = AnotherPanel->CurrentDirectory;
  // IncludeTrailingBackslash to expand C: to C:\.
  if (!FarControl(FCTL_SETANOTHERPANELDIR,
        IncludeTrailingBackslash(NewPath).c_str()))
  {
    Result = false;
  }
  else
  {
    ResetCachedInfo();
    AnotherPanel = AnotherPanelInfo;
    if (!ComparePaths(AnotherPanel->CurrentDirectory, NewPath))
    {
      // FAR WORKAROUND
      // If FCTL_SETANOTHERPANELDIR above fails, Far default current
      // directory to initial (?) one. So move this back to
      // previous directory.
      FarControl(FCTL_SETANOTHERPANELDIR, OldPath.c_str());
      Result = false;
    }
    else
    {
      RedrawPanel(true);
      Result = true;
    }
  }

  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::SetDirectoryEx(const AnsiString Dir, int OpMode)
{
  if (!SessionList() && !Connected())
  {
    return false;
  }
  // FAR WORKAROUND
  // workaround to ignore "change to root directory" command issued by FAR,
  // before file is opened for viewing/editing from "find file" dialog
  // when plugin uses UNIX style paths
  else if (OpMode & OPM_FIND && OpMode & OPM_SILENT && Dir == "\\")
  {
    if (FSavedFindFolder.IsEmpty())
    {
      return true;
    }
    else
    {
      bool Result;
      try
      {
        Result = SetDirectoryEx(FSavedFindFolder, OpMode);
      }
      __finally
      {
        FSavedFindFolder = "";
      }
      return Result;
    }
  }
  else
  {
    if (OpMode & OPM_FIND && FSavedFindFolder.IsEmpty())
    {
      FSavedFindFolder = FTerminal->CurrentDirectory;
    }

    if (SessionList())
    {
      FSessionsFolder = AbsolutePath("/" + FSessionsFolder, Dir);
      assert(FSessionsFolder[1] == '/');
      FSessionsFolder.Delete(1, 1);
      FNewSessionsFolder = "";
    }
    else
    {
      assert(!FNoProgress);
      bool Normal = FLAGCLEAR(OpMode, OPM_FIND | OPM_SILENT);
      AnsiString PrevPath = FTerminal->CurrentDirectory;
      FNoProgress = !Normal;
      if (!FNoProgress)
      {
        FPlugin->ShowConsoleTitle(GetMsg(CHANGING_DIRECTORY_TITLE));
      }
      FTerminal->ExceptionOnFail = true;
      try
      {
        if (Dir == "\\")
        {
          FTerminal->ChangeDirectory(ROOTDIRECTORY);
        }
        else if ((Dir == PARENTDIRECTORY) && (FTerminal->CurrentDirectory == ROOTDIRECTORY))
        {
          ClosePlugin();
        }
        else
        {
          FTerminal->ChangeDirectory(Dir);
        }
      }
      __finally
      {
        FTerminal->ExceptionOnFail = false;
        if (!FNoProgress)
        {
          FPlugin->ClearConsoleTitle();
        }
        FNoProgress = false;
      }

      if (Normal && FSynchronisingBrowse &&
          (PrevPath != FTerminal->CurrentDirectory))
      {
        TFarPanelInfo * AnotherPanel = AnotherPanelInfo;
        if (AnotherPanel->IsPlugin || (AnotherPanel->Type != ptFile))
        {
          MoreMessageDialog(GetMsg(SYNCHRONIZE_LOCAL_PATH_REQUIRED), NULL, qtError, qaOK);
        }
        else
        {
          try
          {
            AnsiString RemotePath = UnixIncludeTrailingBackslash(FTerminal->CurrentDirectory);
            AnsiString FullPrevPath = UnixIncludeTrailingBackslash(PrevPath);
            AnsiString ALocalPath;
            if (RemotePath.SubString(1, FullPrevPath.Length()) == FullPrevPath)
            {
              ALocalPath = IncludeTrailingBackslash(AnotherPanel->CurrentDirectory) +
                FromUnixPath(RemotePath.SubString(FullPrevPath.Length() + 1,
                  RemotePath.Length() - FullPrevPath.Length()));
            }
            else if (FullPrevPath.SubString(1, RemotePath.Length()) == RemotePath)
            {
              AnsiString NewLocalPath;
              ALocalPath = ExcludeTrailingBackslash(AnotherPanel->CurrentDirectory);
              while (!UnixComparePaths(FullPrevPath, RemotePath))
              {
                NewLocalPath = ExcludeTrailingBackslash(ExtractFileDir(ALocalPath));
                if (NewLocalPath == ALocalPath)
                {
                  Abort();
                }
                ALocalPath = NewLocalPath;
                FullPrevPath = UnixExtractFilePath(UnixExcludeTrailingBackslash(FullPrevPath));
              }
            }
            else
            {
              Abort();
            }

            if (!SynchronizeBrowsing(ALocalPath))
            {
              if (MoreMessageDialog(FORMAT(GetMsg(SYNC_DIR_BROWSE_CREATE), (ALocalPath)),
                    NULL, qtInformation, qaYes | qaNo) == qaYes)
              {
                if (!ForceDirectories(ALocalPath))
                {
                  RaiseLastOSError();
                }
                else
                {
                  if (!SynchronizeBrowsing(ALocalPath))
                  {
                    Abort();
                  }
                }
              }
              else
              {
                FSynchronisingBrowse = false;
              }
            }
          }
          catch(Exception & E)
          {
            FSynchronisingBrowse = false;
            WinSCPPlugin()->ShowExtendedException(&E);
            MoreMessageDialog(GetMsg(SYNC_DIR_BROWSE_ERROR), NULL, qtInformation, qaOK);
          }
        }
      }
    }

    return true;
  }
}
//---------------------------------------------------------------------------
int __fastcall TWinSCPFileSystem::MakeDirectoryEx(AnsiString & Name, int OpMode)
{
  if (Connected())
  {
    assert(!(OpMode & OPM_SILENT) || !Name.IsEmpty());

    TRemoteProperties Properties = GUIConfiguration->NewDirectoryProperties;
    bool SaveSettings = false;

    if ((OpMode & OPM_SILENT) ||
        CreateDirectoryDialog(Name, &Properties, SaveSettings))
    {
      if (SaveSettings)
      {
        GUIConfiguration->NewDirectoryProperties = Properties;
      }

      FPlugin->ShowConsoleTitle(GetMsg(CREATING_FOLDER));
      try
      {
        FTerminal->CreateDirectory(Name, &Properties);
      }
      __finally
      {
        FPlugin->ClearConsoleTitle();
      }
      return 1;
    }
    else
    {
      Name = "";
      return -1;
    }
  }
  else if (SessionList())
  {
    assert(!(OpMode & OPM_SILENT) || !Name.IsEmpty());

    if (((OpMode & OPM_SILENT) ||
         FPlugin->InputBox(GetMsg(CREATE_FOLDER_TITLE),
           StripHotKey(GetMsg(CREATE_FOLDER_PROMPT)),
           Name, 0, MAKE_SESSION_FOLDER_HISTORY)) &&
        !Name.IsEmpty())
    {
      TSessionData::ValidateName(Name);
      FNewSessionsFolder = Name;
      return 1;
    }
    else
    {
      Name = "";
      return -1;
    }
  }
  else
  {
    Name = "";
    return -1;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::DeleteSession(TSessionData * Data, void * /*Param*/)
{
  Data->Remove();
  StoredSessions->Remove(Data);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ProcessSessions(TList * PanelItems,
  TProcessSessionEvent ProcessSession, void * Param)
{
  for (int Index = 0; Index < PanelItems->Count; Index++)
  {
    TFarPanelItem * PanelItem = (TFarPanelItem *)PanelItems->Items[Index];
    assert(PanelItem);
    if (PanelItem->IsFile)
    {
      if (PanelItem->UserData != NULL)
      {
        ProcessSession(static_cast<TSessionData *>(PanelItem->UserData), Param);
        PanelItem->Selected = false;
      }
      else
      {
        assert(PanelItem->FileName == GetMsg(NEW_SESSION_HINT));
      }
    }
    else
    {
      assert(PanelItem->UserData == NULL);
      AnsiString Folder = UnixIncludeTrailingBackslash(
        UnixIncludeTrailingBackslash(FSessionsFolder) + PanelItem->FileName);
      int Index = 0;
      while (Index < StoredSessions->Count)
      {
        TSessionData * Data = StoredSessions->Sessions[Index];
        if (Data->Name.SubString(1, Folder.Length()) == Folder)
        {
          ProcessSession(Data, Param);
          if (StoredSessions->Sessions[Index] != Data)
          {
            Index--;
          }
        }
        Index++;
      }
      PanelItem->Selected = false;
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::DeleteFilesEx(TList * PanelItems, int OpMode)
{
  if (Connected())
  {
    FFileList = CreateFileList(PanelItems, osRemote);
    FPanelItems = PanelItems;
    try
    {
      AnsiString Query;
      bool Recycle = FTerminal->SessionData->DeleteToRecycleBin &&
        !FTerminal->IsRecycledFile(FFileList->Strings[0]);
      if (PanelItems->Count > 1)
      {
        Query = FORMAT(GetMsg(Recycle ? RECYCLE_FILES_CONFIRM : DELETE_FILES_CONFIRM),
          (PanelItems->Count));
      }
      else
      {
        Query = FORMAT(GetMsg(Recycle ? RECYCLE_FILE_CONFIRM : DELETE_FILE_CONFIRM),
          (((TFarPanelItem *)PanelItems->Items[0])->FileName));
      }

      if ((OpMode & OPM_SILENT) || !FarConfiguration->ConfirmDeleting ||
        (MoreMessageDialog(Query, NULL, qtConfirmation, qaOK | qaCancel) == qaOK))
      {
        FTerminal->DeleteFiles(FFileList);
      }
    }
    __finally
    {
      FPanelItems = NULL;
      SAFE_DESTROY(FFileList);
    }
    return true;
  }
  else if (SessionList())
  {
    if ((OpMode & OPM_SILENT) || !FarConfiguration->ConfirmDeleting ||
      (MoreMessageDialog(GetMsg(DELETE_SESSIONS_CONFIRM), NULL, qtConfirmation, qaOK | qaCancel) == qaOK))
    {
      ProcessSessions(PanelItems, DeleteSession, NULL);
    }
    return true;
  }
  else
  {
    return false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::QueueAddItem(TQueueItem * Item)
{
  FarConfiguration->CacheFarSettings();
  FQueue->AddItem(Item);
}
//---------------------------------------------------------------------------
struct TExportSessionParam
{
  AnsiString DestPath;
};
//---------------------------------------------------------------------------
int __fastcall TWinSCPFileSystem::GetFilesEx(TList * PanelItems, bool Move,
  AnsiString & DestPath, int OpMode)
{
  int Result;
  if (Connected())
  {
    // FAR WORKAROUND
    // is it?
    // Probable reason was that search result window displays files from several
    // directories and the plugin can hold data for one directory only
    if (OpMode & OPM_FIND)
    {
      throw Exception(GetMsg(VIEW_FROM_FIND_NOT_SUPPORTED));
    }

    FFileList = CreateFileList(PanelItems, osRemote);
    try
    {
      bool EditView = (OpMode & (OPM_EDIT | OPM_VIEW)) != 0;
      bool Confirmed =
        (OpMode & OPM_SILENT) &&
        (!EditView || FarConfiguration->EditorDownloadDefaultMode);

      TGUICopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
      if (EditView)
      {
        EditViewCopyParam(CopyParam);
      }

      // these parameters are known in advance
      int Params =
        FLAGMASK(Move, cpDelete);

      if (!Confirmed)
      {
        int CopyParamAttrs =
          Terminal->UsableCopyParamAttrs(Params).Download |
          FLAGMASK(EditView, cpaNoExcludeMask);
        int Options =
          FLAGMASK(EditView, coTempTransfer | coDisableNewerOnly);
        Confirmed = CopyDialog(false, Move, FFileList, DestPath,
          &CopyParam, Options, CopyParamAttrs);

        if (Confirmed && !EditView && CopyParam.Queue)
        {
          // these parameters are known only after transfer dialog
          Params |=
            FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation) |
            FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
          QueueAddItem(new TDownloadQueueItem(FTerminal, FFileList,
            DestPath, &CopyParam, Params));
          Confirmed = false;
        }
      }

      if (Confirmed)
      {
        if ((FFileList->Count == 1) && (OpMode & OPM_EDIT))
        {
          FOriginalEditFile = IncludeTrailingBackslash(DestPath) +
            UnixExtractFileName(FFileList->Strings[0]);
          FLastEditFile = FOriginalEditFile;
          FLastEditCopyParam = CopyParam;
          FLastEditorID = -1;
        }
        else
        {
          FOriginalEditFile = "";
          FLastEditFile = "";
          FLastEditorID = -1;
        }

        FPanelItems = PanelItems;
        // these parameters are known only after transfer dialog
        Params |=
          FLAGMASK(EditView, cpTemporary) |
          FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
        FTerminal->CopyToLocal(FFileList, DestPath, &CopyParam, Params);
        Result = 1;
      }
      else
      {
        Result = -1;
      }
    }
    __finally
    {
      FPanelItems = NULL;
      SAFE_DESTROY(FFileList);
    }
  }
  else if (SessionList())
  {
    AnsiString Title = GetMsg(EXPORT_SESSION_TITLE);
    AnsiString Prompt;
    if (PanelItems->Count == 1)
    {
      Prompt = FORMAT(GetMsg(EXPORT_SESSION_PROMPT),
        (((TFarPanelItem *)PanelItems->Items[0])->FileName));
    }
    else
    {
      Prompt = FORMAT(GetMsg(EXPORT_SESSIONS_PROMPT), (PanelItems->Count));
    }

    bool AResult = (OpMode & OPM_SILENT) ||
      FPlugin->InputBox(Title, Prompt, DestPath, 0, "Copy");
    if (AResult)
    {
      TExportSessionParam Param;
      Param.DestPath = DestPath;
      ProcessSessions(PanelItems, ExportSession, &Param);
      Result = 1;
    }
    else
    {
      Result = -1;
    }
  }
  else
  {
    Result = -1;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ExportSession(TSessionData * Data, void * AParam)
{
  TExportSessionParam & Param = *static_cast<TExportSessionParam *>(AParam);

  THierarchicalStorage * Storage = NULL;
  TSessionData * ExportData = NULL;
  TSessionData * FactoryDefaults = new TSessionData("");
  try
  {
    ExportData = new TSessionData(Data->Name);
    ExportData->Assign(Data);
    ExportData->Modified = true;
    Storage = new TIniFileStorage(IncludeTrailingBackslash(Param.DestPath) +
      GUIConfiguration->DefaultCopyParam.ValidLocalFileName(ExportData->Name) + ".ini");
    if (Storage->OpenSubKey(Configuration->StoredSessionsSubKey, true))
    {
      ExportData->Save(Storage, false, FactoryDefaults);
    }
  }
  __finally
  {
    delete FactoryDefaults;
    delete Storage;
    delete ExportData;
  }
}
//---------------------------------------------------------------------------
int __fastcall TWinSCPFileSystem::UploadFiles(bool Move, int OpMode, bool Edit,
  AnsiString DestPath)
{
  int Result = 1;
  bool Confirmed = (OpMode & OPM_SILENT);
  bool Ask = !Confirmed;

  TGUICopyParamType CopyParam;

  if (Edit)
  {
    CopyParam = FLastEditCopyParam;
    Confirmed = FarConfiguration->EditorUploadSameOptions;
    Ask = false;
  }
  else
  {
    CopyParam = GUIConfiguration->DefaultCopyParam;
  }

  // these parameters are known in advance
  int Params =
    FLAGMASK(Move, cpDelete);

  if (!Confirmed)
  {
    int CopyParamAttrs =
      Terminal->UsableCopyParamAttrs(Params).Upload |
      FLAGMASK(Edit, (cpaNoExcludeMask | cpaNoClearArchive));
    // heurictics: do not ask for target directory when uploaded file
    // was downloaded in edit mode
    int Options =
      FLAGMASK(Edit, coTempTransfer) |
      FLAGMASK(Edit || !Terminal->IsCapable[fcNewerOnlyUpload], coDisableNewerOnly);
    Confirmed = CopyDialog(true, Move, FFileList, DestPath,
      &CopyParam, Options, CopyParamAttrs);

    if (Confirmed && !Edit && CopyParam.Queue)
    {
      // these parameters are known only after transfer dialog
      Params |=
        FLAGMASK(CopyParam.QueueNoConfirmation, cpNoConfirmation) |
        FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
      QueueAddItem(new TUploadQueueItem(FTerminal, FFileList,
        DestPath, &CopyParam, Params));
      Confirmed = false;
    }
  }

  if (Confirmed)
  {
    assert(!FNoProgressFinish);
    // it does not make sense to unselect file being uploaded from editor,
    // moreover we may upload the file under name that does not exist in
    // remote panel
    FNoProgressFinish = Edit;
    try
    {
      // these parameters are known only after transfer dialog
      Params |=
        FLAGMASK(!Ask, cpNoConfirmation) |
        FLAGMASK(Edit, cpTemporary) |
        FLAGMASK(CopyParam.NewerOnly, cpNewerOnly);
      FTerminal->CopyToRemote(FFileList, DestPath, &CopyParam, Params);
    }
    __finally
    {
      FNoProgressFinish = false;
    }
  }
  else
  {
    Result = -1;
  }
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall TWinSCPFileSystem::PutFilesEx(TList * PanelItems, bool Move, int OpMode)
{
  int Result;
  if (Connected())
  {
    FFileList = CreateFileList(PanelItems, osLocal);
    try
    {
      FPanelItems = PanelItems;

      // if file is saved under different name, FAR tries to upload original file,
      // but let's be robust and check for new name, in case it changes.
      // OMP_EDIT is set since 1.70 final, only.
      // When comparing, beware that one path may be long path and the other short
      // (since 1.70 alpha 6, DestPath in GetFiles is short path,
      // while current path in PutFiles is long path)
      if (FLAGCLEAR(OpMode, OPM_SILENT) && (FFileList->Count == 1) &&
          (CompareFileName(FFileList->Strings[0], FOriginalEditFile) ||
           CompareFileName(FFileList->Strings[0], FLastEditFile)))
      {
        // editor should be closed already
        assert(FLastEditorID < 0);

        if (FarConfiguration->EditorUploadOnSave)
        {
          // already uploaded from EE_REDRAW
          Result = -1;
        }
        else
        {
          // just in case file was saved under different name
          FFileList->Strings[0] = FLastEditFile;

          FOriginalEditFile = "";
          FLastEditFile = "";

          Result = UploadFiles(Move, OpMode, true, FTerminal->CurrentDirectory);
        }
      }
      else
      {
        Result = UploadFiles(Move, OpMode, false, FTerminal->CurrentDirectory);
      }
    }
    __finally
    {
      FPanelItems = NULL;
      SAFE_DESTROY(FFileList);
    }
  }
  else if (SessionList())
  {
    if (!ImportSessions(PanelItems, Move, OpMode))
    {
      Result = -1;
    }
    else
    {
      Result = 1;
    }
  }
  else
  {
    Result = -1;
  }
  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::ImportSessions(TList * PanelItems, bool /*Move*/,
  int OpMode)
{
  bool Result = (OpMode & OPM_SILENT) ||
    (MoreMessageDialog(GetMsg(IMPORT_SESSIONS_PROMPT), NULL,
      qtConfirmation, qaOK | qaCancel) == qaOK);

  if (Result)
  {
    AnsiString FileName;
    TFarPanelItem * PanelItem;
    for (int i = 0; i < PanelItems->Count; i++)
    {
      PanelItem = (TFarPanelItem *)PanelItems->Items[i];
      bool AnyData = false;
      FileName = PanelItem->FileName;
      if (PanelItem->IsFile)
      {
        THierarchicalStorage * Storage = NULL;
        try
        {
          Storage = new TIniFileStorage(IncludeTrailingBackslash(GetCurrentDir()) + FileName);
          if (Storage->OpenSubKey(Configuration->StoredSessionsSubKey, false) &&
              Storage->HasSubKeys())
          {
            AnyData = true;
            StoredSessions->Load(Storage, true);
            // modified only, explicit
            StoredSessions->Save(false, true);
          }
        }
        __finally
        {
          delete Storage;
        }
      }
      if (!AnyData)
      {
        throw Exception(FORMAT(GetMsg(IMPORT_SESSIONS_EMPTY), (FileName)));
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
TStrings * __fastcall TWinSCPFileSystem::CreateFocusedFileList(
  TOperationSide Side, TFarPanelInfo * PanelInfo)
{
  if (PanelInfo == NULL)
  {
    PanelInfo = this->PanelInfo;
  }

  TStrings * Result;
  TFarPanelItem * PanelItem = PanelInfo->FocusedItem;
  if (PanelItem->IsParentDirectory)
  {
    Result = NULL;
  }
  else
  {
    Result = new TStringList();
    assert((Side == osLocal) || PanelItem->UserData);
    AnsiString FileName = PanelItem->FileName;
    if (Side == osLocal)
    {
      FileName = IncludeTrailingBackslash(PanelInfo->CurrentDirectory) + FileName;
    }
    Result->AddObject(FileName, (TObject *)PanelItem->UserData);
  }
  return Result;
}
//---------------------------------------------------------------------------
TStrings * __fastcall TWinSCPFileSystem::CreateSelectedFileList(
  TOperationSide Side, TFarPanelInfo * PanelInfo)
{
  if (PanelInfo == NULL)
  {
    PanelInfo = this->PanelInfo;
  }

  TStrings * Result;
  if (PanelInfo->SelectedCount > 0)
  {
    Result = CreateFileList(PanelInfo->Items, Side, true,
      PanelInfo->CurrentDirectory);
  }
  else
  {
    Result = CreateFocusedFileList(Side, PanelInfo);
  }
  return Result;
}
//---------------------------------------------------------------------------
TStrings * __fastcall TWinSCPFileSystem::CreateFileList(TList * PanelItems,
  TOperationSide Side, bool SelectedOnly, AnsiString Directory, bool FileNameOnly,
  TStrings * AFileList)
{
  TStrings * FileList = (AFileList == NULL ? new TStringList() : AFileList);
  try
  {
    AnsiString FileName;
    TFarPanelItem * PanelItem;
    TObject * Data = NULL;
    for (int Index = 0; Index < PanelItems->Count; Index++)
    {
      PanelItem = (TFarPanelItem *)PanelItems->Items[Index];
      assert(PanelItem);
      if ((!SelectedOnly || PanelItem->Selected) &&
          !PanelItem->IsParentDirectory)
      {
        FileName = PanelItem->FileName;
        if (Side == osRemote)
        {
          Data = (TRemoteFile *)PanelItem->UserData;
          assert(Data);
        }
        if (Side == osLocal)
        {
          if (ExtractFilePath(FileName).IsEmpty())
          {
            if (!FileNameOnly)
            {
              if (Directory.IsEmpty())
              {
                Directory = GetCurrentDir();
              }
              FileName = IncludeTrailingBackslash(Directory) + FileName;
            }
          }
          else
          {
            if (FileNameOnly)
            {
              FileName = ExtractFileName(FileName);
            }
          }
        }
        FileList->AddObject(FileName, Data);
      }
    }

    if (FileList->Count == 0)
    {
      Abort();
    }
  }
  catch (...)
  {
    if (AFileList == NULL)
    {
      delete FileList;
    }
    throw;
  }
  return FileList;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::SaveSession()
{
  if (!FTerminal->SessionData->Name.IsEmpty())
  {
    FTerminal->SessionData->RemoteDirectory = FTerminal->CurrentDirectory;

    TSessionData * Data;
    Data = (TSessionData *)StoredSessions->FindByName(FTerminal->SessionData->Name);
    if (Data)
    {
      bool Changed = false;
      if (Terminal->SessionData->UpdateDirectories)
      {
        Data->RemoteDirectory = Terminal->SessionData->RemoteDirectory;
        Changed = true;
      }

      if (Changed)
      {
        // modified only, implicit
        StoredSessions->Save(false, false);
      }
    }
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::Connect(TSessionData * Data)
{
  bool Result = false;
  assert(!FTerminal);
  FTerminal = new TTerminal(Data, Configuration);
  try
  {
    FTerminal->OnQueryUser = TerminalQueryUser;
    FTerminal->OnPromptUser = TerminalPromptUser;
    FTerminal->OnDisplayBanner = TerminalDisplayBanner;
    FTerminal->OnShowExtendedException = TerminalShowExtendedException;
    FTerminal->OnChangeDirectory = TerminalChangeDirectory;
    FTerminal->OnReadDirectory = TerminalReadDirectory;
    FTerminal->OnStartReadDirectory = TerminalStartReadDirectory;
    FTerminal->OnReadDirectoryProgress = TerminalReadDirectoryProgress;
    FTerminal->OnInformation = TerminalInformation;
    FTerminal->OnFinished = OperationFinished;
    FTerminal->OnProgress = OperationProgress;
    FTerminal->OnDeleteLocalFile = TerminalDeleteLocalFile;
    ConnectTerminal(FTerminal);

    FTerminal->OnClose = TerminalClose;

    assert(FQueue == NULL);
    FQueue = new TTerminalQueue(FTerminal, Configuration);
    FQueue->TransfersLimit = GUIConfiguration->QueueTransfersLimit;
    FQueue->OnQueryUser = TerminalQueryUser;
    FQueue->OnPromptUser = TerminalPromptUser;
    FQueue->OnShowExtendedException = TerminalShowExtendedException;
    FQueue->OnListUpdate = QueueListUpdate;
    FQueue->OnQueueItemUpdate = QueueItemUpdate;
    FQueue->OnEvent = QueueEvent;

    assert(FQueueStatus == NULL);
    FQueueStatus = FQueue->CreateStatus(NULL);

    // TODO: Create instance of TKeepaliveThread here, once its implementation
    // is complete

    Result = true;
  }
  catch(Exception &E)
  {
    FTerminal->ShowExtendedException(&E);
    SAFE_DESTROY(FTerminal);
    delete FQueue;
    FQueue = NULL;
  }

  FSynchronisingBrowse = GUIConfiguration->SynchronizeBrowsing;

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ConnectTerminal(TTerminal * Terminal)
{
  Terminal->Open();
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalClose(TObject * /*Sender*/)
{
  // Plugin closure is now invoked from HandleException
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::LogAuthentication(
  TTerminal * Terminal, AnsiString Msg)
{
  assert(FAuthenticationLog != NULL);
  FAuthenticationLog->Add(Msg);
  TStringList * AuthenticationLogLines = new TStringList();
  try
  {
    int Width = 42;
    int Height = 11;
    FarWrapText(FAuthenticationLog->Text.TrimRight(), AuthenticationLogLines, Width);
    int Count;
    AnsiString Message;
    if (AuthenticationLogLines->Count == 0)
    {
      Message = AnsiString::StringOfChar(' ', Width) + "\n";
      Count = 1;
    }
    else
    {
      while (AuthenticationLogLines->Count > Height)
      {
        AuthenticationLogLines->Delete(0);
      }
      AuthenticationLogLines->Strings[0] =
        AuthenticationLogLines->Strings[0] +
          AnsiString::StringOfChar(' ', Width - AuthenticationLogLines->Strings[0].Length());
      Message = AnsiReplaceStr(AuthenticationLogLines->Text, "\r", "");
      Count = AuthenticationLogLines->Count;
    }

    Message += AnsiString::StringOfChar('\n', Height - Count);

    FPlugin->Message(0, Terminal->SessionData->SessionName, Message);
  }
  __finally
  {
    delete AuthenticationLogLines;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalInformation(
  TTerminal * Terminal, const AnsiString & Str, bool /*Status*/, bool Active)
{
  if (Active)
  {
    if (Terminal->Status == ssOpening)
    {
      if (FAuthenticationLog == NULL)
      {
        FAuthenticationLog = new TStringList();
        FPlugin->SaveScreen(FAuthenticationSaveScreenHandle);
        FPlugin->ShowConsoleTitle(Terminal->SessionData->SessionName);
      }

      LogAuthentication(Terminal, Str);
      FPlugin->UpdateConsoleTitle(Str);
    }
  }
  else
  {
    if (FAuthenticationLog != NULL)
    {
      FPlugin->ClearConsoleTitle();
      FPlugin->RestoreScreen(FAuthenticationSaveScreenHandle);
      SAFE_DESTROY(FAuthenticationLog);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalChangeDirectory(TObject * /*Sender*/)
{
  if (!FNoProgress)
  {
    AnsiString Directory = FTerminal->CurrentDirectory;
    int Index = FPathHistory->IndexOf(Directory);
    if (Index >= 0)
    {
      FPathHistory->Delete(Index);
    }

    if (!FLastPath.IsEmpty())
    {
      FPathHistory->Add(FLastPath);
    }

    FLastPath = Directory;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalStartReadDirectory(TObject * /*Sender*/)
{
  if (!FNoProgress)
  {
    FPlugin->ShowConsoleTitle(GetMsg(READING_DIRECTORY_TITLE));
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalReadDirectoryProgress(
  TObject * /*Sender*/, int Progress, bool & Cancel)
{
  if (Progress < 0)
  {
    if (!FNoProgress && (Progress == -2))
    {
      MoreMessageDialog(GetMsg(DIRECTORY_READING_CANCELLED), NULL,
         qtWarning, qaOK);
    }
  }
  else
  {
    if (FPlugin->CheckForEsc())
    {
      Cancel = true;
    }

    if (!FNoProgress)
    {
      FPlugin->UpdateConsoleTitle(
        FORMAT("%s (%d)", (GetMsg(READING_DIRECTORY_TITLE), Progress)));
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalReadDirectory(TObject * /*Sender*/,
  bool /*ReloadOnly*/)
{
  if (!FNoProgress)
  {
    FPlugin->ClearConsoleTitle();
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalDeleteLocalFile(const AnsiString FileName,
  bool Alternative)
{
  if (!RecursiveDeleteFile(FileName,
        (FLAGSET(FPlugin->FarSystemSettings(), FSS_DELETETORECYCLEBIN)) != Alternative))
  {
    throw Exception(FORMAT(GetMsg(DELETE_LOCAL_FILE_ERROR), (FileName)));
  }
}
//---------------------------------------------------------------------------
int __fastcall TWinSCPFileSystem::MoreMessageDialog(AnsiString Str,
  TStrings * MoreMessages, TQueryType Type, int Answers, const TMessageParams * Params)
{
  TMessageParams AParams;

  if ((FProgressSaveScreenHandle != 0) ||
      (FSynchronizationSaveScreenHandle != 0))
  {
    if (Params != NULL)
    {
      AParams = *Params;
    }
    Params = &AParams;
    AParams.Flags |= FMSG_WARNING;
  }

  return WinSCPPlugin()->MoreMessageDialog(Str, MoreMessages, Type,
    Answers, Params);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalQueryUser(TObject * /*Sender*/,
  const AnsiString Query, TStrings * MoreMessages, int Answers,
  const TQueryParams * Params, int & Answer, TQueryType Type, void * /*Arg*/)
{
  TMessageParams AParams;
  AnsiString AQuery = Query;

  if (Params != NULL)
  {
    if (Params->Params & qpFatalAbort)
    {
      AQuery = FORMAT(GetMsg(WARN_FATAL_ERROR), (AQuery));
    }

    AParams.Aliases = Params->Aliases;
    AParams.AliasesCount = Params->AliasesCount;
    AParams.Params = Params->Params & (qpNeverAskAgainCheck | qpAllowContinueOnError);
    AParams.Timer = Params->Timer;
    AParams.TimerEvent = Params->TimerEvent;
    AParams.TimerMessage = Params->TimerMessage;
    AParams.TimerAnswers = Params->TimerAnswers;
    AParams.Timeout = Params->Timeout;
    AParams.TimeoutAnswer = Params->TimeoutAnswer;
  }

  Answer = MoreMessageDialog(AQuery, MoreMessages, Type, Answers, &AParams);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalPromptUser(TTerminal * Terminal,
  TPromptKind Kind, AnsiString Name, AnsiString Instructions,
  TStrings * Prompts, TStrings * Results, bool & Result,
  void * /*Arg*/)
{
  if (Kind == pkPrompt)
  {
    assert(Instructions.IsEmpty());
    assert(Prompts->Count == 1);
    assert(bool(Prompts->Objects[0]));
    AnsiString AResult = Results->Strings[0];

    Result = FPlugin->InputBox(Name, StripHotKey(Prompts->Strings[0]), AResult, FIB_NOUSELASTHISTORY);
    if (Result)
    {
      Results->Strings[0] = AResult;
    }
  }
  else
  {
    Result = PasswordDialog(Terminal->SessionData, Kind, Name, Instructions,
      Prompts, Results, Terminal->StoredCredentialsTried);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalDisplayBanner(
  TTerminal * /*Terminal*/, AnsiString SessionName,
  const AnsiString & Banner, bool & NeverShowAgain, int Options)
{
  BannerDialog(SessionName, Banner, NeverShowAgain, Options);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::TerminalShowExtendedException(
  TTerminal * /*Terminal*/, Exception * E, void * /*Arg*/)
{
  WinSCPPlugin()->ShowExtendedException(E);
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::OperationProgress(
  TFileOperationProgressType & ProgressData, TCancelStatus & /*Cancel*/)
{
  if (FNoProgress)
  {
    return;
  }

  bool First = false;
  if (ProgressData.InProgress && !FProgressSaveScreenHandle)
  {
    FPlugin->SaveScreen(FProgressSaveScreenHandle);
    First = true;
  }

  // operation is finished (or terminated), so we hide progress form
  if (!ProgressData.InProgress && FProgressSaveScreenHandle)
  {
    FPlugin->RestoreScreen(FProgressSaveScreenHandle);
    FPlugin->ClearConsoleTitle();
  }
  else
  {
    ShowOperationProgress(ProgressData, First);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::OperationFinished(TFileOperation Operation,
  TOperationSide Side, bool /*Temp*/, const AnsiString & FileName, bool Success,
  bool & /*DisconnectWhenComplete*/)
{
  USEDPARAM(Side);

  if ((Operation != foCalculateSize) &&
      (Operation != foGetProperties) &&
      (Operation != foCalculateChecksum) &&
      (FSynchronizationSaveScreenHandle == 0) &&
      !FNoProgress && !FNoProgressFinish)
  {
    TFarPanelItem * PanelItem = NULL;;

    if (!FPanelItems)
    {
      TList * PanelItems = PanelInfo->Items;
      for (int Index = 0; Index < PanelItems->Count; Index++)
      {
        if (((TFarPanelItem *)PanelItems->Items[Index])->FileName == FileName)
        {
          PanelItem = (TFarPanelItem *)PanelItems->Items[Index];
          break;
        }
      }
    }
    else
    {
      assert(FFileList);
      assert(FPanelItems->Count == FFileList->Count);
      int Index = FFileList->IndexOf(FileName);
      assert(Index >= 0);
      PanelItem = (TFarPanelItem *)FPanelItems->Items[Index];
    }

    assert(PanelItem->FileName ==
      ((Side == osLocal) ? ExtractFileName(FileName) : FileName));
    if (Success)
    {
      PanelItem->Selected = false;
    }
  }

  if (Success && (FSynchronizeController != NULL))
  {
    if (Operation == foCopy)
    {
      assert(Side == osLocal);
      FSynchronizeController->LogOperation(soUpload, FileName);
    }
    else if (Operation == foDelete)
    {
      assert(Side == osRemote);
      FSynchronizeController->LogOperation(soDelete, FileName);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ShowOperationProgress(
  TFileOperationProgressType & ProgressData, bool First)
{
  static unsigned long LastTicks;
  unsigned long Ticks = GetTickCount();
  if (Ticks - LastTicks > 500 || First)
  {
    LastTicks = Ticks;

    static const int ProgressWidth = 48;
    static const int Captions[] = {PROGRESS_COPY, PROGRESS_MOVE, PROGRESS_DELETE,
      PROGRESS_SETPROPERTIES, 0, 0, PROGRESS_CALCULATE_SIZE,
      PROGRESS_REMOTE_MOVE, PROGRESS_REMOTE_COPY, PROGRESS_GETPROPERTIES,
      PROGRESS_CALCULATE_CHECKSUM };
    static AnsiString ProgressFileLabel;
    static AnsiString TargetDirLabel;
    static AnsiString StartTimeLabel;
    static AnsiString TimeElapsedLabel;
    static AnsiString BytesTransferedLabel;
    static AnsiString CPSLabel;
    static AnsiString TimeLeftLabel;

    if (ProgressFileLabel.IsEmpty())
    {
      ProgressFileLabel = GetMsg(PROGRESS_FILE_LABEL);
      TargetDirLabel = GetMsg(TARGET_DIR_LABEL);
      StartTimeLabel = GetMsg(START_TIME_LABEL);
      TimeElapsedLabel = GetMsg(TIME_ELAPSED_LABEL);
      BytesTransferedLabel = GetMsg(BYTES_TRANSFERED_LABEL);
      CPSLabel = GetMsg(CPS_LABEL);
      TimeLeftLabel = GetMsg(TIME_LEFT_LABEL);
    }

    bool TransferOperation =
      ((ProgressData.Operation == foCopy) || (ProgressData.Operation == foMove));

    AnsiString Message1;
    AnsiString ProgressBar1;
    AnsiString Message2;
    AnsiString ProgressBar2;
    AnsiString Title = GetMsg(Captions[(int)ProgressData.Operation - 1]);
    AnsiString FileName = ProgressData.FileName;
    // for upload from temporary directory,
    // do not show source directory
    if (TransferOperation && (ProgressData.Side == osLocal) && ProgressData.Temp)
    {
      FileName = ExtractFileName(FileName);
    }
    Message1 = ProgressFileLabel + MinimizeName(FileName,
      ProgressWidth - ProgressFileLabel.Length(), ProgressData.Side == osRemote) + "\n";
    // for downloads to temporary directory,
    // do not show target directory
    if (TransferOperation && !((ProgressData.Side == osRemote) && ProgressData.Temp))
    {
      Message1 += TargetDirLabel + MinimizeName(ProgressData.Directory,
        ProgressWidth - TargetDirLabel.Length(), ProgressData.Side == osLocal) + "\n";
    }
    ProgressBar1 = ProgressBar(ProgressData.OverallProgress(), ProgressWidth) + "\n";
    if (TransferOperation)
    {
      Message2 = "\1\n";
      AnsiString StatusLine;
      AnsiString Value;

      Value = FormatDateTimeSpan(Configuration->TimeFormat, ProgressData.TimeElapsed());
      StatusLine = TimeElapsedLabel +
        AnsiString::StringOfChar(' ', ProgressWidth / 2 - 1 - TimeElapsedLabel.Length() - Value.Length()) +
        Value + "  ";

      AnsiString LabelText;
      if (ProgressData.TotalSizeSet)
      {
        Value = FormatDateTimeSpan(Configuration->TimeFormat, ProgressData.TotalTimeLeft());
        LabelText = TimeLeftLabel;
      }
      else
      {
        Value = ProgressData.StartTime.TimeString();
        LabelText = StartTimeLabel;
      }
      StatusLine = StatusLine + LabelText +
        AnsiString::StringOfChar(' ', ProgressWidth - StatusLine.Length() -
        LabelText.Length() - Value.Length()) +
        Value;
      Message2 += StatusLine + "\n";

      Value = FormatBytes(ProgressData.TotalTransfered);
      StatusLine = BytesTransferedLabel +
        AnsiString::StringOfChar(' ', ProgressWidth / 2 - 1 - BytesTransferedLabel.Length() - Value.Length()) +
        Value + "  ";
      Value = FORMAT("%s/s", (FormatBytes(ProgressData.CPS())));
      StatusLine = StatusLine + CPSLabel +
        AnsiString::StringOfChar(' ', ProgressWidth - StatusLine.Length() -
        CPSLabel.Length() - Value.Length()) +
        Value;
      Message2 += StatusLine + "\n";
      ProgressBar2 += ProgressBar(ProgressData.TransferProgress(), ProgressWidth) + "\n";
    }
    AnsiString Message =
      StrToFar(Message1) + ProgressBar1 + StrToFar(Message2) + ProgressBar2;
    FPlugin->Message(0, Title, Message, NULL, NULL, true);

    if (First)
    {
      FPlugin->ShowConsoleTitle(Title);
    }
    FPlugin->UpdateConsoleTitleProgress((short)ProgressData.OverallProgress());

    if (FPlugin->CheckForEsc())
    {
      CancelConfiguration(ProgressData);
    }
  }
}
//---------------------------------------------------------------------------
AnsiString __fastcall TWinSCPFileSystem::ProgressBar(int Percentage, int Width)
{
  AnsiString Result;
  // OEM character set (Ansi does not have the ascii art we need)
  Result = AnsiString::StringOfChar('\xDB', (Width - 5) * Percentage / 100);
  Result += AnsiString::StringOfChar('\xB0', (Width - 5) - Result.Length());
  Result += FORMAT("%4d%%", (Percentage));
  return Result;
}
//---------------------------------------------------------------------------
TTerminalQueueStatus * __fastcall TWinSCPFileSystem::ProcessQueue(bool Hidden)
{
  TTerminalQueueStatus * Result = NULL;
  assert(FQueueStatus != NULL);

  if (FQueueStatusInvalidated || FQueueItemInvalidated)
  {
    if (FQueueStatusInvalidated)
    {
      TGuard Guard(FQueueStatusSection);

      FQueueStatusInvalidated = false;

      assert(FQueue != NULL);
      FQueueStatus = FQueue->CreateStatus(FQueueStatus);
      Result = FQueueStatus;
    }

    FQueueItemInvalidated = false;

    TQueueItemProxy * QueueItem;
    for (int Index = 0; Index < FQueueStatus->ActiveCount; Index++)
    {
      QueueItem = FQueueStatus->Items[Index];
      if ((bool)QueueItem->UserData)
      {
        QueueItem->Update();
        Result = FQueueStatus;
      }

      if (GUIConfiguration->QueueAutoPopup &&
          TQueueItem::IsUserActionStatus(QueueItem->Status))
      {
        QueueItem->ProcessUserAction();
      }
    }
  }

  if (FRefreshRemoteDirectory)
  {
    if ((Terminal != NULL) && Terminal->Active)
    {
      Terminal->RefreshDirectory();
      if (UpdatePanel())
      {
        RedrawPanel();
      }
    }
    FRefreshRemoteDirectory = false;
  }
  if (FRefreshLocalDirectory)
  {
    if (GetOppositeFileSystem() == NULL)
    {
      if (UpdatePanel(false, true))
      {
        RedrawPanel(true);
      }
    }
    FRefreshLocalDirectory = false;
  }

  if (FQueueEventPending)
  {
    TQueueEvent Event;

    {
      TGuard Guard(FQueueStatusSection);
      Event = FQueueEvent;
      FQueueEventPending = false;
    }

    switch (Event)
    {
      case qeEmpty:
        if (Hidden && FarConfiguration->QueueBeep)
        {
          MessageBeep(MB_OK);
        }
        break;

      case qePendingUserAction:
        if (Hidden && !GUIConfiguration->QueueAutoPopup && FarConfiguration->QueueBeep)
        {
          // MB_ICONQUESTION would be more appropriate, but in default Windows Sound
          // schema it has no sound associated
          MessageBeep(MB_OK);
        }
        break;
    }
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::QueueListUpdate(TTerminalQueue * Queue)
{
  if (FQueue == Queue)
  {
    FQueueStatusInvalidated = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::QueueItemUpdate(TTerminalQueue * Queue,
  TQueueItem * Item)
{
  if (FQueue == Queue)
  {
    TGuard Guard(FQueueStatusSection);

    assert(FQueueStatus != NULL);

    TQueueItemProxy * QueueItem = FQueueStatus->FindByQueueItem(Item);

    if ((Item->Status == TQueueItem::qsDone) && (Terminal != NULL))
    {
      FRefreshLocalDirectory = (QueueItem == NULL) ||
        (!QueueItem->Info->ModifiedLocal.IsEmpty());
      FRefreshRemoteDirectory = (QueueItem == NULL) ||
        (!QueueItem->Info->ModifiedRemote.IsEmpty());
    }

    if (QueueItem != NULL)
    {
      QueueItem->UserData = (void*)true;
      FQueueItemInvalidated = true;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::QueueEvent(TTerminalQueue * Queue,
  TQueueEvent Event)
{
  TGuard Guard(FQueueStatusSection);
  if (Queue == FQueue)
  {
    FQueueEventPending = true;
    FQueueEvent = Event;
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::CancelConfiguration(TFileOperationProgressType & ProgressData)
{
  if (!ProgressData.Suspended)
  {
    ProgressData.Suspend();
    try
    {
      TCancelStatus ACancel;
      int Result;
      if (ProgressData.TransferingFile &&
          (ProgressData.TimeExpected() > GUIConfiguration->IgnoreCancelBeforeFinish))
      {
        Result = MoreMessageDialog(GetMsg(CANCEL_OPERATION_FATAL), NULL,
          qtWarning, qaYes | qaNo | qaCancel);
      }
      else
      {
        Result = MoreMessageDialog(GetMsg(CANCEL_OPERATION), NULL,
          qtConfirmation, qaOK | qaCancel);
      }
      switch (Result) {
        case qaYes:
          ACancel = csCancelTransfer; break;
        case qaOK:
        case qaNo:
          ACancel = csCancel; break;
        default:
          ACancel = csContinue; break;
      }

      if (ACancel > ProgressData.Cancel)
      {
        ProgressData.Cancel = ACancel;
      }
    }
    __finally
    {
      ProgressData.Resume();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::UploadFromEditor(bool NoReload, AnsiString FileName,
  AnsiString DestPath)
{
  assert(FFileList == NULL);
  FFileList = new TStringList();
  assert(FTerminal->AutoReadDirectory);
  bool PrevAutoReadDirectory = FTerminal->AutoReadDirectory;
  if (NoReload)
  {
    FTerminal->AutoReadDirectory = false;
    if (UnixComparePaths(DestPath, FTerminal->CurrentDirectory))
    {
      FReloadDirectory = true;
    }
  }

  try
  {
    FFileList->Add(FileName);
    UploadFiles(false, 0, true, DestPath);
  }
  __finally
  {
    FTerminal->AutoReadDirectory = PrevAutoReadDirectory;
    SAFE_DESTROY(FFileList);
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::UploadOnSave(bool NoReload)
{
  TFarEditorInfo * Info = FPlugin->EditorInfo();
  if (Info != NULL)
  {
    try
    {
      bool NativeEdit =
        (FLastEditorID >= 0) &&
        (FLastEditorID == Info->EditorID) &&
        !FLastEditFile.IsEmpty();

      TMultipleEdits::iterator I = FMultipleEdits.find(Info->EditorID);
      bool MultipleEdit = (I != FMultipleEdits.end());

      if (NativeEdit || MultipleEdit)
      {
        // make sure this is reset before any dialog is shown as it may cause recursion
        FEditorPendingSave = false;

        if (NativeEdit)
        {
          assert(FLastEditFile == Info->FileName);
          // always upload under the most recent name
          UploadFromEditor(NoReload, FLastEditFile, FTerminal->CurrentDirectory);
        }

        if (MultipleEdit)
        {
          UploadFromEditor(NoReload, Info->FileName, I->second.Directory);
          // note that panel gets not refreshed upon switch to
          // panel view. but that's intentional
        }
      }
    }
    __finally
    {
      delete Info;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ProcessEditorEvent(int Event, void * /*Param*/)
{
  // EE_REDRAW is the first for optimalisation
  if (Event == EE_REDRAW)
  {
    if (FEditorPendingSave)
    {
      UploadOnSave(true);
    }

    // Whenever editor title is changed (and restored back), it is restored
    // to default FAR text, not to ours (see EE_SAVE). Hence we periodically
    // reset the title.
    static unsigned long LastTicks = 0;
    unsigned long Ticks = GetTickCount();
    if ((LastTicks == 0) || (Ticks - LastTicks > 500))
    {
      LastTicks = Ticks;
      TFarEditorInfo * Info = FPlugin->EditorInfo();
      if (Info != NULL)
      {
        try
        {
          TMultipleEdits::iterator I = FMultipleEdits.find(Info->EditorID);
          if (I != FMultipleEdits.end())
          {
            AnsiString FullFileName = UnixIncludeTrailingBackslash(I->second.Directory) +
              I->second.FileName;
            FPlugin->FarEditorControl(ECTL_SETTITLE, FullFileName.c_str());
          }
        }
        __finally
        {
          delete Info;
        }
      }
    }
  }
  else if (Event == EE_READ)
  {
    // file can be read from active filesystem only anyway. this prevents
    // both filesystems in both panels intercepting the very same file in case the
    // file with the same name is read from both filesystems recently
    if (IsActiveFileSystem())
    {
      TFarEditorInfo * Info = FPlugin->EditorInfo();
      if (Info != NULL)
      {
        try
        {
          if (!FLastEditFile.IsEmpty() &&
              AnsiSameText(FLastEditFile, Info->FileName))
          {
            FLastEditorID = Info->EditorID;
            FEditorPendingSave = false;
          }

          if (!FLastMultipleEditFile.IsEmpty())
          {
            bool IsLastMultipleEditFile = AnsiSameText(FLastMultipleEditFile, Info->FileName);
            assert(IsLastMultipleEditFile);
            if (IsLastMultipleEditFile)
            {
              FLastMultipleEditFile = "";

              TMultipleEdit MultipleEdit;
              MultipleEdit.FileName = ExtractFileName(Info->FileName);
              MultipleEdit.Directory = FLastMultipleEditDirectory;
              MultipleEdit.LocalFileName = Info->FileName;
              MultipleEdit.PendingSave = false;
              FMultipleEdits[Info->EditorID] = MultipleEdit;
              if (FLastMultipleEditReadOnly)
              {
                EditorSetParameter Parameter;
                memset(&Parameter, 0, sizeof(Parameter));
                Parameter.Type = ESPT_LOCKMODE;
                Parameter.Param.iParam = TRUE;
                FPlugin->FarEditorControl(ECTL_SETPARAM, &Parameter);
              }
            }
          }
        }
        __finally
        {
          delete Info;
        }
      }
    }
  }
  else if (Event == EE_CLOSE)
  {
    if (FEditorPendingSave)
    {
      assert(false); // should not happen, but let's be robust
      UploadOnSave(false);
    }

    TFarEditorInfo * Info = FPlugin->EditorInfo();
    if (Info != NULL)
    {
      try
      {
        if (FLastEditorID == Info->EditorID)
        {
          FLastEditorID = -1;
        }

        TMultipleEdits::iterator I = FMultipleEdits.find(Info->EditorID);
        if (I != FMultipleEdits.end())
        {
          if (I->second.PendingSave)
          {
            UploadFromEditor(true, Info->FileName, I->second.Directory);
            // reload panel content (if uploaded to current directory.
            // no need for RefreshPanel as panel is not visible yet.
            UpdatePanel();
          }

          if (DeleteFile(Info->FileName))
          {
            // remove directory only if it is empty
            // (to avoid deleting another directory if user uses "save as")
            RemoveDir(ExcludeTrailingBackslash(ExtractFilePath(Info->FileName)));
          }

          FMultipleEdits.erase(I);
        }
      }
      __finally
      {
        delete Info;
      }
    }
  }
  else if (Event == EE_SAVE)
  {
    TFarEditorInfo * Info = FPlugin->EditorInfo();
    if (Info != NULL)
    {
      try
      {
        if ((FLastEditorID >= 0) && (FLastEditorID == Info->EditorID))
        {
          // if the file is saved under different name ("save as"), we upload
          // the file back under that name
          FLastEditFile = Info->FileName;

          if (FarConfiguration->EditorUploadOnSave)
          {
            FEditorPendingSave = true;
          }
        }

        TMultipleEdits::iterator I = FMultipleEdits.find(Info->EditorID);
        if (I != FMultipleEdits.end())
        {
          if (I->second.LocalFileName != Info->FileName)
          {
            // update file name (after "save as")
            I->second.LocalFileName = Info->FileName;
            I->second.FileName = ExtractFileName(Info->FileName);
            // update editor title
            AnsiString FullFileName = UnixIncludeTrailingBackslash(I->second.Directory) +
              I->second.FileName;
            // note that we need to reset the title periodically (see EE_REDRAW)
            FPlugin->FarEditorControl(ECTL_SETTITLE, FullFileName.c_str());
          }

          if (FarConfiguration->EditorUploadOnSave)
          {
            FEditorPendingSave = true;
          }
          else
          {
            I->second.PendingSave = true;
          }
        }
      }
      __finally
      {
        delete Info;
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::EditViewCopyParam(TCopyParamType & CopyParam)
{
  CopyParam.FileNameCase = ncNoChange;
  CopyParam.PreserveReadOnly = false;
  CopyParam.ResumeSupport = rsOff;
  // we have no way to give FAR back the modified filename, so make sure we
  // fail downloading file not valid on windows
  CopyParam.ReplaceInvalidChars = false;
  CopyParam.FileMask = "";
  CopyParam.ExcludeFileMask = TFileMasks();
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::MultipleEdit()
{
  if ((PanelInfo->FocusedItem != NULL) &&
      PanelInfo->FocusedItem->IsFile &&
      (PanelInfo->FocusedItem->UserData != NULL))
  {
    TStrings * FileList = CreateFocusedFileList(osRemote);
    assert((FileList == NULL) || (FileList->Count == 1));

    if (FileList != NULL)
    {
      try
      {
        if (FileList->Count == 1)
        {
          MultipleEdit(FTerminal->CurrentDirectory, FileList->Strings[0],
            (TRemoteFile*)FileList->Objects[0]);
        }
      }
      __finally
      {
        delete FileList;
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::MultipleEdit(AnsiString Directory,
  AnsiString FileName, TRemoteFile * File)
{
  TEditHistory EditHistory;
  EditHistory.Directory = Directory;
  EditHistory.FileName = FileName;

  TEditHistories::iterator ih = std::find(FEditHistories.begin(), FEditHistories.end(), EditHistory);
  if (ih != FEditHistories.end())
  {
    FEditHistories.erase(ih);
  }
  FEditHistories.push_back(EditHistory);

  AnsiString FullFileName = UnixIncludeTrailingBackslash(Directory) + FileName;

  TMultipleEdits::iterator i = FMultipleEdits.begin();
  while (i != FMultipleEdits.end())
  {
    if (UnixComparePaths(Directory, i->second.Directory) &&
        (FileName == i->second.FileName))
    {
      break;
    }
    ++i;
  }

  FLastMultipleEditReadOnly = false;
  bool Edit = true;
  if (i != FMultipleEdits.end())
  {
    TMessageParams Params;
    TQueryButtonAlias Aliases[3];
    Aliases[0].Button = qaYes;
    Aliases[0].Alias = GetMsg(EDITOR_CURRENT);
    Aliases[1].Button = qaNo;
    Aliases[1].Alias = GetMsg(EDITOR_NEW_INSTANCE);
    Aliases[2].Button = qaOK;
    Aliases[2].Alias = GetMsg(EDITOR_NEW_INSTANCE_RO);
    Params.Aliases = Aliases;
    Params.AliasesCount = LENOF(Aliases);
    switch (MoreMessageDialog(FORMAT(GetMsg(EDITOR_ALREADY_LOADED), (FullFileName)),
          NULL, qtConfirmation, qaYes | qaNo | qaOK | qaCancel, &Params))
    {
      case qaYes:
        Edit = false;
        break;

      case qaNo:
        // noop
        break;

      case qaOK:
        FLastMultipleEditReadOnly = true;
        break;

      case qaCancel:
      default:
        Abort();
        break;
    }
  }

  if (Edit)
  {
    AnsiString TempDir;
    TCopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;
    EditViewCopyParam(CopyParam);

    TStrings * FileList = new TStringList;
    assert(!FNoProgressFinish);
    FNoProgressFinish = true;
    try
    {
      FileList->AddObject(FullFileName, File);
      TemporarilyDownloadFiles(FileList, CopyParam, TempDir);
    }
    __finally
    {
      FNoProgressFinish = false;
      delete FileList;
    }

    FLastMultipleEditFile = IncludeTrailingBackslash(TempDir) + FileName;
    FLastMultipleEditDirectory = Directory;

    if (FarPlugin->Editor(FLastMultipleEditFile,
           EF_NONMODAL | VF_IMMEDIATERETURN | VF_DISABLEHISTORY, FullFileName))
    {
      assert(FLastMultipleEditFile == "");
    }
    FLastMultipleEditFile = "";
  }
  else
  {
    assert(i != FMultipleEdits.end());

    int WindowCount = FarPlugin->FarAdvControl(ACTL_GETWINDOWCOUNT);
    WindowInfo Window;
    Window.Pos = 0;
    while (Window.Pos < WindowCount)
    {
      if (FarPlugin->FarAdvControl(ACTL_GETWINDOWINFO, &Window) != 0)
      {
        if ((Window.Type == WTYPE_EDITOR) &&
            AnsiSameText(Window.Name, i->second.LocalFileName))
        {
          FarPlugin->FarAdvControl(ACTL_SETCURRENTWINDOW, Window.Pos);
          break;
        }
      }
      Window.Pos++;
    }

    assert(Window.Pos < WindowCount);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::IsEditHistoryEmpty()
{
  return FEditHistories.empty();
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::EditHistory()
{
  TFarMenuItems * MenuItems = new TFarMenuItems();
  try
  {
    TEditHistories::const_iterator i = FEditHistories.begin();
    while (i != FEditHistories.end())
    {
      MenuItems->Add(MinimizeName(UnixIncludeTrailingBackslash((*i).Directory) + (*i).FileName,
        FPlugin->MaxMenuItemLength(), true));
      i++;
    }

    MenuItems->Add("");
    MenuItems->ItemFocused = MenuItems->Count - 1;

    const int BreakKeys[] = { VK_F4, 0 };

    int BreakCode;
    int Result = FPlugin->Menu(FMENU_REVERSEAUTOHIGHLIGHT | FMENU_SHOWAMPERSAND | FMENU_WRAPMODE,
      GetMsg(MENU_EDIT_HISTORY), "", MenuItems, BreakKeys, BreakCode);

    if ((Result >= 0) && (Result < int(FEditHistories.size())))
    {
      TRemoteFile * File;
      AnsiString FullFileName =
        UnixIncludeTrailingBackslash(FEditHistories[Result].Directory) + FEditHistories[Result].FileName;
      FTerminal->ReadFile(FullFileName, File);
      try
      {
        if (!File->HaveFullFileName)
        {
          File->FullFileName = FullFileName;
        }
        MultipleEdit(FEditHistories[Result].Directory,
          FEditHistories[Result].FileName, File);
      }
      __finally
      {
        delete File;
      }
    }
  }
  __finally
  {
    delete MenuItems;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TWinSCPFileSystem::IsLogging()
{
  return
    Connected() && FTerminal->Log->LoggingToFile;
}
//---------------------------------------------------------------------------
void __fastcall TWinSCPFileSystem::ShowLog()
{
  assert(Connected() && FTerminal->Log->LoggingToFile);
  FPlugin->Viewer(FTerminal->Log->CurrentFileName, VF_NONMODAL);
}
