//---------------------------------------------------------------------------
#ifndef SessionInfoH
#define SessionInfoH

#include <CoreDefs.hpp>

#include "SessionData.h"
#include "Interface.h"
//---------------------------------------------------------------------------
enum TSessionStatus { ssClosed, ssOpening, ssOpened };
//---------------------------------------------------------------------------
struct TSessionInfo : public TObject
{
  TSessionInfo();

  TDateTime LoginTime;
  UnicodeString ProtocolBaseName;
  UnicodeString ProtocolName;
  UnicodeString SecurityProtocolName;

  UnicodeString CSCipher;
  UnicodeString CSCompression;
  UnicodeString SCCipher;
  UnicodeString SCCompression;

  UnicodeString SshVersionString;
  UnicodeString SshImplementation;
  UnicodeString HostKeyFingerprint;

  UnicodeString CertificateFingerprint;
  UnicodeString Certificate;
};
//---------------------------------------------------------------------------
enum TFSCapability { fcUserGroupListing = 0, fcModeChanging, fcGroupChanging,
  fcOwnerChanging, fcGroupOwnerChangingByID, fcAnyCommand, fcHardLink,
  fcSymbolicLink, fcResolveSymlink,
  fcTextMode, fcRename, fcNativeTextMode, fcNewerOnlyUpload, fcRemoteCopy,
  fcTimestampChanging, fcRemoteMove, fcLoadingAdditionalProperties,
  fcCheckingSpaceAvailable, fcIgnorePermErrors, fcCalculatingChecksum,
  fcModeChangingUpload, fcPreservingTimestampUpload, fcShellAnyCommand,
  fcSecondaryShell, fcRemoveCtrlZUpload, fcRemoveBOMUpload, fcCount };
//---------------------------------------------------------------------------
struct TFileSystemInfo : public TObject
{
  TFileSystemInfo();

  UnicodeString ProtocolBaseName;
  UnicodeString ProtocolName;
  UnicodeString RemoteSystem;
  UnicodeString AdditionalInfo;
  bool IsCapable[fcCount];
};
//---------------------------------------------------------------------------
class TSessionUI
{
public:
  explicit TSessionUI() {}
  virtual ~TSessionUI() {}
  virtual void Information(const UnicodeString & Str, bool Status) = 0;
  virtual uintptr_t QueryUser(const UnicodeString & Query,
    TStrings * MoreMessages, uintptr_t Answers, const TQueryParams * Params,
    TQueryType QueryType = qtConfirmation) = 0;
  virtual uintptr_t QueryUserException(const UnicodeString & Query,
    Exception * E, uintptr_t Answers, const TQueryParams * Params,
    TQueryType QueryType = qtConfirmation) = 0;
  virtual bool PromptUser(TSessionData * Data, TPromptKind Kind,
    const UnicodeString & Name, const UnicodeString & Instructions, TStrings * Prompts,
    TStrings * Results) = 0;
  virtual void DisplayBanner(const UnicodeString & Banner) = 0;
  virtual void FatalError(Exception * E, const UnicodeString & Msg, const UnicodeString & HelpKeyword = L"") = 0;
  virtual void HandleExtendedException(Exception * E) = 0;
  virtual void Closed() = 0;
};
//---------------------------------------------------------------------------
// Duplicated in LogMemo.h for design-time-only purposes
enum TLogLineType { llOutput, llInput, llStdError, llMessage, llException };
enum TLogAction { laUpload, laDownload, laTouch, laChmod, laMkdir, laRm, laMv, laCall, laLs, laStat };
//---------------------------------------------------------------------------
DEFINE_CALLBACK_TYPE2(TCaptureOutputEvent, void,
  const UnicodeString & /* Str */, bool /* StdError */);
DEFINE_CALLBACK_TYPE3(TCalculatedChecksumEvent, void,
  const UnicodeString & /* FileName */, const UnicodeString & /* Alg */, const UnicodeString & /* Hash */);
//---------------------------------------------------------------------------
class TSessionActionRecord;
class TActionLog;
//---------------------------------------------------------------------------
class TSessionAction : public TObject
{
public:
  explicit TSessionAction(TActionLog * Log, TLogAction Action);
  virtual ~TSessionAction();

  void Restart();

  void Commit();
  void Rollback(Exception * E = nullptr);
  void Cancel();

protected:
  TSessionActionRecord * FRecord;

private:
  NB_DISABLE_COPY(TSessionAction)
};
//---------------------------------------------------------------------------
class TFileSessionAction : public TSessionAction
{
public:
  explicit TFileSessionAction(TActionLog * Log, TLogAction Action);
  explicit TFileSessionAction(TActionLog * Log, TLogAction Action, const UnicodeString & FileName);

  void FileName(const UnicodeString & FileName);
};
//---------------------------------------------------------------------------
class TFileLocationSessionAction : public TFileSessionAction
{
public:
  explicit TFileLocationSessionAction(TActionLog * Log, TLogAction Action);
  explicit TFileLocationSessionAction(TActionLog * Log, TLogAction Action, const UnicodeString & FileName);

  void Destination(const UnicodeString & Destination);
};
//---------------------------------------------------------------------------
class TUploadSessionAction : public TFileLocationSessionAction
{
public:
  explicit TUploadSessionAction(TActionLog * Log);
};
//---------------------------------------------------------------------------
class TDownloadSessionAction : public TFileLocationSessionAction
{
public:
  explicit TDownloadSessionAction(TActionLog * Log);
};
//---------------------------------------------------------------------------
class TRights;
//---------------------------------------------------------------------------
class TChmodSessionAction : public TFileSessionAction
{
public:
  explicit TChmodSessionAction(TActionLog * Log, const UnicodeString & FileName);
  explicit TChmodSessionAction(TActionLog * Log, const UnicodeString & FileName,
    const TRights & Rights);

  void Rights(const TRights & Rights);
  void Recursive();
};
//---------------------------------------------------------------------------
class TTouchSessionAction : public TFileSessionAction
{
public:
  explicit TTouchSessionAction(TActionLog * Log, const UnicodeString & FileName,
    const TDateTime & Modification);
};
//---------------------------------------------------------------------------
class TMkdirSessionAction : public TFileSessionAction
{
public:
  explicit TMkdirSessionAction(TActionLog * Log, const UnicodeString & FileName);
};
//---------------------------------------------------------------------------
class TRmSessionAction : public TFileSessionAction
{
public:
  explicit TRmSessionAction(TActionLog * Log, const UnicodeString & FileName);

  void Recursive();
};
//---------------------------------------------------------------------------
class TMvSessionAction : public TFileLocationSessionAction
{
public:
  explicit TMvSessionAction(TActionLog * Log, const UnicodeString & FileName,
    const UnicodeString & Destination);
};
//---------------------------------------------------------------------------
class TCallSessionAction : public TSessionAction
{
public:
  explicit TCallSessionAction(TActionLog * Log, const UnicodeString & Command,
    const UnicodeString & Destination);

  void AddOutput(const UnicodeString & Output, bool StdError);
};
//---------------------------------------------------------------------------
class TLsSessionAction : public TSessionAction
{
public:
  explicit TLsSessionAction(TActionLog * Log, const UnicodeString & Destination);

  void FileList(TRemoteFileList * FileList);
};
//---------------------------------------------------------------------------
class TStatSessionAction : public TFileSessionAction
{
public:
  explicit TStatSessionAction(TActionLog * Log, const UnicodeString & FileName);

  void File(TRemoteFile * File);
};
//---------------------------------------------------------------------------
DEFINE_CALLBACK_TYPE2(TDoAddLogEvent, void,
  TLogLineType, const UnicodeString &);
//---------------------------------------------------------------------------
class TSessionLog : protected TStringList
{
  CUSTOM_MEM_ALLOCATION_IMPL
friend class TSessionAction;
friend class TSessionActionRecord;
public:
  explicit TSessionLog(TSessionUI * UI, TSessionData * SessionData,
    TConfiguration * Configuration);
  virtual ~TSessionLog();
  HIDESBASE void Add(TLogLineType Type, const UnicodeString & Line);
  void AddSystemInfo();
  void AddStartupInfo();
  void AddException(Exception * E);
  void AddSeparator();

  virtual void Clear();
  void ReflectSettings();
  void Lock();
  void Unlock();

  TSessionLog * GetParent();
  void SetParent(TSessionLog * Value);
  bool GetLogging() const;
  TNotifyEvent & GetOnChange();
  void SetOnChange(TNotifyEvent Value);
  TNotifyEvent & GetOnStateChange();
  void SetOnStateChange(TNotifyEvent Value);
  UnicodeString GetCurrentFileName() const;
  intptr_t GetTopIndex() const;
  UnicodeString GetName() const;
  void SetName(const UnicodeString & Value);
  virtual intptr_t GetCount() const;

public:
  UnicodeString GetLine(intptr_t Index) const;
  TLogLineType GetType(intptr_t Index) const;
  void DeleteUnnecessary();
  void StateChange();
  void OpenLogFile();
  intptr_t GetBottomIndex() const;
  UnicodeString GetLogFileName() const;
  bool GetLoggingToFile() const;
  UnicodeString GetSessionName() const;

private:
  void DoAdd(TLogLineType AType, const UnicodeString & Line,
    TDoAddLogEvent Event);
  void DoAddToParent(TLogLineType AType, const UnicodeString & ALine);
  void DoAddToSelf(TLogLineType AType, const UnicodeString & ALine);
  void AddStartupInfo(bool System);
  void DoAddStartupInfo(TSessionData * Data);
  UnicodeString GetTlsVersionName(TTlsVersion TlsVersion);

protected:
  void CloseLogFile();
  bool LogToFile() const;

private:
  TConfiguration * FConfiguration;
  TSessionLog * FParent;
  TCriticalSection * FCriticalSection;
  bool FLogging;
  void * FFile;
  UnicodeString FCurrentLogFileName;
  UnicodeString FCurrentFileName;
  int FLoggedLines;
  intptr_t FTopIndex;
  TSessionUI * FUI;
  TSessionData * FSessionData;
  UnicodeString FName;
  bool FClosed;
  TNotifyEvent FOnStateChange;

private:
  NB_DISABLE_COPY(TSessionLog)
};
//---------------------------------------------------------------------------
class TActionLog : public TObject
{
friend class TSessionAction;
friend class TSessionActionRecord;
public:
  explicit TActionLog(TSessionUI * UI, TSessionData * SessionData,
    TConfiguration * Configuration);
  virtual ~TActionLog();

  void ReflectSettings();
  void AddFailure(Exception * E);
  void AddFailure(TStrings * Messages);
  void BeginGroup(const UnicodeString & Name);
  void EndGroup();

  UnicodeString GetCurrentFileName() const { return FCurrentFileName; }
  bool GetEnabled() const { return FEnabled; }

  UnicodeString GetLogFileName();
  void SetEnabled(bool Value);

protected:
  void CloseLogFile();
  inline void AddPendingAction(TSessionActionRecord * Action);
  void RecordPendingActions();
  void Add(const UnicodeString & Line);
  void AddIndented(const UnicodeString & Line);
  void AddMessages(const UnicodeString & Indent, TStrings * Messages);

private:
  TConfiguration * FConfiguration;
  TCriticalSection * FCriticalSection;
  bool FLogging;
  void * FFile;
  UnicodeString FCurrentLogFileName;
  UnicodeString FCurrentFileName;
  TSessionUI * FUI;
  TSessionData * FSessionData;
  TList * FPendingActions;
  bool FClosed;
  bool FInGroup;
  bool FEnabled;
  UnicodeString FIndent;

  void OpenLogFile();

private:
  NB_DISABLE_COPY(TActionLog)
};
//---------------------------------------------------------------------------
#endif
