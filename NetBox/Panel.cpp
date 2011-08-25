/**************************************************************************
 *  NetBox plugin for FAR 2.0 (http://code.google.com/p/farplugs)         *
 *  Copyright (C) 2011 by Artem Senichev <artemsen@gmail.com>             *
 *  Copyright (C) 2011 by Michael Lukashov <michael.lukashov@gmail.com>   *
 *                                                                        *
 *  This program is free software: you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation, either version 3 of the License, or     *
 *  (at your option) any later version.                                   *
 *                                                                        *
 *  This program is distributed in the hope that it will be useful,       *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *  GNU General Public License for more details.                          *
 *                                                                        *
 *  You should have received a copy of the GNU General Public License     *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "stdafx.h"
#include "Panel.h"
#include "Logging.h"
#include "ProgressWindow.h"

#define IS_SILENT(op) (op & (OPM_SILENT | OPM_FIND))


CPanel::CPanel(const bool exitToSessionMgr) :
    m_ProtoClient(NULL), m_Title(CFarPlugin::GetString(StringTitle)),
    m_ExitToSessionMgr(exitToSessionMgr), m_AbortTask(NULL)
{
}


CPanel::~CPanel()
{
    CloseConnection();
    if (m_AbortTask)
    {
        CloseHandle(m_AbortTask);
    }
}


bool CPanel::OpenConnection(IProtocol *protoImpl)
{
    assert(protoImpl);

    CloseConnection();
    m_ProtoClient = protoImpl;

    ResetAbortTask();

    bool connectionEstablished = false;
    const std::wstring connectURL = m_ProtoClient->GetURL();

    if (IsSessionManager())
    {
        connectionEstablished = true;
    }
    else
    {
        CNotificationWindow notifyWnd(CFarPlugin::GetString(StringTitle),
                                      CFarPlugin::GetFormattedString(StringPrgConnect, connectURL.c_str()).c_str());
        notifyWnd.Show();

        Log1(L"connecting to %s", connectURL.c_str());
        std::wstring errorMsg;
        while (!m_ProtoClient->Connect(m_AbortTask, errorMsg))
        {
            notifyWnd.Hide();
            if (!m_ProtoClient->TryToResolveConnectionProblem())
            {
                std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrEstablish, connectURL.c_str());
                taskErrorMsg += L'\n';
                taskErrorMsg += errorMsg;
                Log1(L"error: %s", errorMsg.c_str());
                const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_RETRYCANCEL | FMSG_WARNING);
                errorMsg.clear();
                m_ProtoClient->Close();
                if (retCode != 0)
                {
                    //I am not owner of the protoImpl, so don't delete it
                    m_ProtoClient = NULL;
                    break;
                }
            }
            notifyWnd.Show();
            ResetEvent(m_AbortTask);
        }
        connectionEstablished = (m_ProtoClient != NULL);
    }

    if (connectionEstablished)
    {
        if (!connectURL.empty())
            Log1(L"connected to %s", connectURL.c_str());
        UpdateTitle();
    }
    return connectionEstablished;
}


void CPanel::CloseConnection()
{
    if (m_ProtoClient)
    {
        m_ProtoClient->Close();
        delete m_ProtoClient;
        m_ProtoClient = NULL;
    }
    UpdateTitle();
}


void CPanel::GetOpenPluginInfo(OpenPluginInfo *pluginInfo)
{
    assert(pluginInfo);

    pluginInfo->StructSize = sizeof(OpenPluginInfo);
    pluginInfo->PanelTitle = GetTitle();
    pluginInfo->Format = CFarPlugin::GetString(StringTitle);
    pluginInfo->CurDir = m_ProtoClient ? m_ProtoClient->GetCurrentDirectory() : NULL;
    pluginInfo->Flags = OPIF_USEHIGHLIGHTING | OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_ADDDOTS;

    if (IsSessionManager())
    {
        static_cast<CSessionManager *>(m_ProtoClient)->GetOpenPluginInfo(pluginInfo);
    }
}


int CPanel::ProcessKey(const int key, const unsigned int controlState)
{
    if (IsSessionManager())
    {
        if (key == VK_RETURN && controlState == 0)      //Open new session
        {
            //Get file name
            PanelInfo pi;
            if (!CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELINFO, sizeof(pi), reinterpret_cast<LONG_PTR>(&pi)))
            {
                return 1;
            }
            const size_t ppiBufferLength = CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, static_cast<LONG_PTR>(NULL));
            if (ppiBufferLength == 0)
            {
                return 1;
            }
            vector<unsigned char> ppiBuffer(ppiBufferLength);
            PluginPanelItem *ppi = reinterpret_cast<PluginPanelItem *>(&ppiBuffer.front());
            if (!CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, reinterpret_cast<LONG_PTR>(ppi)))
            {
                return 1;
            }
            if ((ppi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                PSession session = static_cast<CSessionManager *>(m_ProtoClient)->LoadSession(ppi->FindData.lpwszFileName);
                if (!session.get())
                {
                    return 1;
                }
                PProtocol proto = session->CreateClient();

                //TODO: Save current session manager path - may be we fail on next operation
                const std::wstring smPath = m_ProtoClient->GetCurrentDirectory();
                if (proto.get() && OpenConnection(proto.get()))
                {
                    proto.release();    //From now we are the only owner!
                    CFarPlugin::GetPSI()->Control(this, FCTL_UPDATEPANEL, 0, NULL);
                    PanelRedrawInfo pri;
                    pri.CurrentItem = pri.TopPanelItem = 0;
                    CFarPlugin::GetPSI()->Control(this, FCTL_REDRAWPANEL, 0, reinterpret_cast<LONG_PTR>(&pri));
                    return 1;
                }
                OpenConnection(new CSessionManager);
                ChangeDirectory(smPath.c_str(), 0);
                return 1;
            }
        }

        return static_cast<CSessionManager *>(m_ProtoClient)->ProcessKey(this, key, controlState);
    }


    //Ctrl+Alt+Ins:  Copy URL to clipboard (include user name/password)
    //Shift+Alt+Ins: Copy URL to clipboard (without user name/password)
    if (key == VK_INSERT && (controlState & PKF_ALT) && ((controlState & PKF_CONTROL) || (controlState & PKF_SHIFT)))
    {
        //Get file name
        PanelInfo pi;
        if (!CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELINFO, sizeof(pi), reinterpret_cast<LONG_PTR>(&pi)))
        {
            return 0;
        }
        const size_t ppiBufferLength = CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, static_cast<LONG_PTR>(NULL));
        if (ppiBufferLength == 0)
        {
            return 0;
        }
        vector<unsigned char> ppiBuffer(ppiBufferLength);
        PluginPanelItem *ppi = reinterpret_cast<PluginPanelItem *>(&ppiBuffer.front());
        if (!CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELITEM, pi.CurrentItem, reinterpret_cast<LONG_PTR>(ppi)))
        {
            return 0;
        }

        assert(m_ProtoClient);
        std::wstring cbData = m_ProtoClient->GetURL((controlState & PKF_CONTROL) && !(controlState & PKF_SHIFT));
        cbData += m_ProtoClient->GetCurrentDirectory();
        ::AppendWChar(cbData, L'/');
        cbData += ppi->FindData.lpwszFileName;

        CFarPlugin::GetPSI()->FSF->CopyToClipboard(cbData.c_str());
        return 1;
    }
    if (!IsSessionManager() && (key == VK_F6) && (controlState & PKF_SHIFT))
    {
        // Переименование на сервере
        // DEBUG_PRINTF(L"NetBox: ProcessKey: ShiftF6");
        // TransferFiles(Key == VK_F6);
        // DEBUG_PRINTF(L"NetBox: ProcessKey: destPath = %s", destPath);
        const bool deleteSource = true;
        const int opMode = OPM_TOPLEVEL;

        // Получим текущий элемент на панели плагина
        HANDLE plugin = reinterpret_cast<HANDLE>(this);
        const size_t ppiBufferLength = CFarPlugin::GetPSI()->Control(plugin, FCTL_GETCURRENTPANELITEM, 0, static_cast<LONG_PTR>(NULL));
        if (!ppiBufferLength)
        {
            // DEBUG_PRINTF(L"NetBox: ProcessKey: 1: ppiBufferLength = %d", ppiBufferLength);
            return 0;
        }
        vector<unsigned char> ppiBuffer(ppiBufferLength);
        PluginPanelItem *ppi = reinterpret_cast<PluginPanelItem *>(&ppiBuffer.front());
        if (!CFarPlugin::GetPSI()->Control(plugin, FCTL_GETCURRENTPANELITEM, 0, reinterpret_cast<LONG_PTR>(ppi)))
        {
            // DEBUG_PRINTF(L"NetBox: ProcessKey: 2");
            return 0;
        }
        // DEBUG_PRINTF(L"NetBox: ppi->FindData.lpwszFileName = %s", ppi->FindData.lpwszFileName);
        std::wstring dstPath = m_ProtoClient->GetCurrentDirectory();
        ::AppendWChar(dstPath, L'/');
        dstPath += ppi->FindData.lpwszFileName;
        const wchar_t *destPath = dstPath.c_str();
        GetFiles(ppi, 1, &destPath, deleteSource, opMode);

        // Перерисовываем панель
        PanelInfo pi;
        if (!CFarPlugin::GetPSI()->Control(PANEL_ACTIVE, FCTL_GETPANELINFO, sizeof(pi), reinterpret_cast<LONG_PTR>(&pi)))
        {
            // DEBUG_PRINTF(L"NetBox: ProcessKey: 3");
            return 0;
        }
        CFarPlugin::GetPSI()->Control(this, FCTL_UPDATEPANEL, 0, NULL);
        PanelRedrawInfo pri;
        pri.CurrentItem = pi.CurrentItem;
        pri.TopPanelItem = pi.TopPanelItem;
        CFarPlugin::GetPSI()->Control(this, FCTL_REDRAWPANEL, 0, reinterpret_cast<LONG_PTR>(&pri));
        return 1;
    }

    return 0;
}


int CPanel::ChangeDirectory(const wchar_t *dir, const int opMode)
{
    assert(dir);
    assert(m_ProtoClient);
    // DEBUG_PRINTF(L"NetBox: ChangeDirectory: dir = %s, opMode = %u", dir, opMode);
    // DEBUG_PRINTF(L"NetBox: ChangeDirectory: m_ProtoClient->GetCurrentDirectory = %s", m_ProtoClient->GetCurrentDirectory());

    const bool topDirectory = (wcscmp(L"/", m_ProtoClient->GetCurrentDirectory()) == 0);
    const bool moveUp = (wcscmp(L"..", dir) == 0);

    std::wstring errInfo;
    bool retStatus = false;

    if (topDirectory && moveUp)
    {
        if (!m_ExitToSessionMgr)
        {
            CFarPlugin::GetPSI()->Control(this, FCTL_CLOSEPLUGIN, NULL, NULL);
            return 0;
        }
        OpenConnection(new CSessionManager);
        retStatus = true;
    }
    else
    {
        if (!IS_SILENT(opMode))
        {
            CNotificationWindow notifyWnd(CFarPlugin::GetString(StringTitle), CFarPlugin::GetFormattedString(StringPrgChangeDir, dir).c_str());
            notifyWnd.Show();
        }
        if (m_ProtoClient->Aborted())
        {
            ResetAbortTask();
            // CloseConnection();
            // std::wstring errorMsg;
            // if (!m_ProtoClient->Connect(m_AbortTask, errorMsg))
            // {
                // return FALSE;
            // }
        }
        retStatus = m_ProtoClient->ChangeDirectory(dir, errInfo);
    }

    if (!retStatus && !IS_SILENT(opMode))
    {
        // DEBUG_PRINTF(L"NetBox: dir = %s, OpMode = %u", dir, opMode);
        ShowErrorDialog(0, CFarPlugin::GetFormattedString(StringErrChangeDir, dir), errInfo.c_str());
    }
    else if (!IS_SILENT(opMode))
    {
        UpdateTitle();
    }

    return retStatus ? TRUE : FALSE;
}


int CPanel::MakeDirectory(const wchar_t **name, const int opMode)
{
    assert(m_ProtoClient);

    if (!IS_SILENT(opMode))
    {
        CFarDialog dlg(60, 8, CFarPlugin::GetString(StringMKDirTitle));
        dlg.CreateText(dlg.GetLeft(), dlg.GetTop() + 0, CFarPlugin::GetString(StringMKDirName));
        const int idName = dlg.CreateEdit(dlg.GetLeft(), dlg.GetTop() + 1, MAX_SIZE, *name);
        dlg.CreateSeparator(dlg.GetHeight() - 2);
        dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(StringOK), DIF_CENTERGROUP);
        const int idBtnCancel = dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(StringCancel), DIF_CENTERGROUP);
        const int retCode = dlg.DoModal();
        if (retCode < 0 || retCode == idBtnCancel)
        {
            return -1;
        }
        m_LastDirName = dlg.GetText(idName);
        *name = m_LastDirName.c_str();
    }

    std::wstring path(m_ProtoClient->GetCurrentDirectory());
    if (path.compare(L"/") != 0)
    {
        path += L'/';
    }
    path += *name;

    std::wstring errInfo;
    if (!m_ProtoClient->MakeDirectory(path.c_str(), errInfo) && !IS_SILENT(opMode))
    {
        ShowErrorDialog(0, CFarPlugin::GetFormattedString(StringErrCreateDir, *name), errInfo.c_str());
        return -1;
    }

    return 1;
}


int CPanel::GetItemList(PluginPanelItem **panelItem, int *itemsNumber, const int opMode)
{
    assert(m_ProtoClient);
    // DEBUG_PRINTF(L"NetBox: GetItemList: begin");

    // CNotificationWindow notifyWnd(CFarPlugin::GetString(StringTitle), CFarPlugin::GetFormattedString(StringPrgGetList, m_ProtoClient->GetCurrentDirectory()).c_str());
    // CProgressWindow progressWnd(m_AbortTask, CProgressWindow::Scan, CProgressWindow::List, 1, m_ProtoClient);
    if (!IsSessionManager() && !IS_SILENT(opMode))
    {
        // progressWnd.Show();
        // notifyWnd.Show();
    }

    std::wstring errInfo;
    if (!m_ProtoClient->GetList(panelItem, itemsNumber, errInfo) && !IS_SILENT(opMode))
    {
        // progressWnd.Destroy();
        // notifyWnd.Hide();
        ShowErrorDialog(0, CFarPlugin::GetFormattedString(StringErrListDir, m_ProtoClient->GetCurrentDirectory()), errInfo.c_str());
        if (IsSessionManager())
        {
            return 0;
        }
        else
        {
            OpenConnection(new CSessionManager); //Return to session manager panel
            // DEBUG_PRINTF(L"NetBox: before CPanel::GetItemList");
            return GetItemList(panelItem, itemsNumber, opMode);
        }
    }
    // progressWnd.Destroy();
    // DEBUG_PRINTF(L"NetBox: GetItemList: end");
    return 1;
}


void CPanel::FreeItemList(PluginPanelItem *panelItem, int itemsNumber)
{
    assert(m_ProtoClient);
    m_ProtoClient->FreeList(panelItem, itemsNumber);
}


int CPanel::GetFiles(PluginPanelItem *panelItem, const int itemsNumber, const wchar_t **destPath, const bool deleteSource, const int opMode)
{
    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: begin: destPath = %s, deleteSource = %d, opMode = %d", *destPath, deleteSource, opMode);
    assert(m_ProtoClient);
    assert(!IsSessionManager());

    if (itemsNumber == 1 && wcscmp(panelItem->FindData.lpwszFileName, L"..") == 0)
    {
        return 0;
    }

    if (IsSessionManager())
    {
        return 0;
    }

    if (!IS_SILENT(opMode) && panelItem)
    {
        CFarDialog dlg(70, 8, CFarPlugin::GetString(deleteSource ? StringMoveTitle : StringCopyTitle));
        std::wstring fileName = panelItem->FindData.lpwszFileName;
        if (fileName.length() > 46)
        {
            fileName = L"..." + fileName.substr(fileName.length() - 43);
        }
        wchar_t copyPath[MAX_PATH];
        swprintf_s(copyPath, CFarPlugin::GetString(deleteSource ? StringMovePath : StringCopyPath),
            itemsNumber > 1 ? CFarPlugin::GetString(deleteSource ? StringMoveSelected : StringCopySelected) : fileName.c_str());
        // DEBUG_PRINTF(L"NetBox: copyPath = %s, destPath = %s", copyPath, *destPath);
        dlg.CreateText(dlg.GetLeft(), dlg.GetTop(), copyPath);
        const int idPath = dlg.CreateEdit(dlg.GetLeft(), dlg.GetTop() + 1, MAX_SIZE, *destPath);
        dlg.CreateSeparator(dlg.GetHeight() - 2);
        dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(deleteSource ? StringMoveBtnCopy : StringCopyBtnCopy), DIF_CENTERGROUP);
        const int idBtnCancel = dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(StringCancel), DIF_CENTERGROUP);
        const int retCode = dlg.DoModal();
        if (retCode < 0 || retCode == idBtnCancel)
        {
            return -1;
        }
        m_LastDirName = dlg.GetText(idPath);
        *destPath = m_LastDirName.c_str();
    }

    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: deleteSource = %d, itemsNumber = %d, destPath = %s", deleteSource, itemsNumber, *destPath);
    if (deleteSource)
    {
        //Check for rename/move inside server command
        if (!(*destPath && (wcslen(*destPath) > 2) && (*destPath)[1] == L':'))
        {
            // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: 1");
            std::wstring errInfo;
            if (itemsNumber > 1)
            {
                // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: 2");
                //Move operation
                for (int i = 0; i < itemsNumber; ++i)
                {
                    PluginPanelItem *pi = &panelItem[i];
                    std::wstring srcPath = m_ProtoClient->GetCurrentDirectory();
                    if (srcPath.compare(L"/") != 0)
                    {
                        srcPath += L'/';
                    }
                    srcPath += pi->FindData.lpwszFileName;

                    std::wstring dstPath;
                    if (*destPath[0] == L'/') //Full path specified by user
                    {
                        dstPath = *destPath;
                    }
                    else
                    {
                        dstPath = m_ProtoClient->GetCurrentDirectory();
                        if (dstPath.compare(L"/") != 0)
                        {
                            dstPath += L'/';
                        }
                        dstPath += *destPath;
                    }
                    ::AppendWChar(dstPath, L'/');
                    dstPath += pi->FindData.lpwszFileName;
                    std::wstring errInfo;
                    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: Rename 1");
                    if (!m_ProtoClient->Rename(srcPath.c_str(), dstPath.c_str(),
                        pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ?
                            IProtocol::ItemDirectory : IProtocol::ItemFile, errInfo)
                                && !IS_SILENT(opMode))
                    {
                        ShowErrorDialog(0, CFarPlugin::GetFormattedString(StringErrRenameMove, srcPath.c_str(), dstPath.c_str()), errInfo.c_str());
                        return 0;
                    }
                }
            }
            else if (itemsNumber == 1)
            {
                // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: 3");
                //Move/rename operation
                std::wstring srcPath = m_ProtoClient->GetCurrentDirectory();
                if (srcPath.compare(L"/") != 0)
                {
                    srcPath += L'/';
                }
                srcPath += panelItem->FindData.lpwszFileName;
                std::wstring dstPath;
                if (*destPath[0] == L'/') //Full path specified by user
                {
                    dstPath = *destPath;
                }
                else
                {
                    dstPath = m_ProtoClient->GetCurrentDirectory();
                    if (dstPath.compare(L"/") != 0)
                    {
                        dstPath += L'/';
                    }
                    dstPath += *destPath;
                }
                const IProtocol::ItemType itemType = (panelItem->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? IProtocol::ItemDirectory : IProtocol::ItemFile;
                bool isExist = false;
                // DEBUG_PRINTF(L"NetBox: GetFiles: dstPath = %s", dstPath.c_str());
                if (m_ProtoClient->CheckExisting(dstPath.c_str(), IProtocol::ItemDirectory, isExist, errInfo) && isExist)
                {
                    //Move
                    ::AppendWChar(dstPath, L'/');
                    dstPath += panelItem->FindData.lpwszFileName;
                }
                // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: Rename 2");
                if (!m_ProtoClient->Rename(srcPath.c_str(), dstPath.c_str(),
                    itemType, errInfo) && !IS_SILENT(opMode))
                {
                    ShowErrorDialog(0, CFarPlugin::GetFormattedString(StringErrRenameMove, srcPath.c_str(), dstPath.c_str()), errInfo.c_str());
                    return 0;
                }
            }
            return 1;
        }
        else
        {
            // TODO: Копирование на локальный диск и удаление с сервера
        }
    }

    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: itemsNumber = %d", itemsNumber);
    //Full copied content (include subdirectories)
    vector< pair<int, PluginPanelItem *> > subDirContent;

    //Get full copied content
    subDirContent.push_back(make_pair(itemsNumber, panelItem));
    for (int i = 0; i < itemsNumber; ++i)
    {
        PluginPanelItem *pi = &panelItem[i];
        if (pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            PluginPanelItem *subItems = NULL;
            int subItemsNum = 0;
            if (!CFarPlugin::GetPSI()->GetPluginDirList(CFarPlugin::GetPSI()->ModuleNumber, this, pi->FindData.lpwszFileName, &subItems, &subItemsNum))
            {
                return 0;
            }
            subDirContent.push_back(make_pair(subItemsNum, subItems));
        }
    }

    // Calculate total count/size and get content
    size_t totalFileCount = 0;
    unsigned __int64 totalFileSize = 0;
    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: subDirContent.size = %u", subDirContent.size());
    for (vector< pair<int, PluginPanelItem *> >::const_iterator it = subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: it->first = %u", it->first);
        for (int i = 0; i < it->first; ++i)
        {
            PluginPanelItem *pi = &it->second[i];
            if (!(pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                ++totalFileCount;
                totalFileSize += pi->FindData.nFileSize;
            }
        }
    }

    //Directory to remove list
    vector<wstring> dirsToRemove;

    CProgressWindow progressWnd(m_AbortTask, deleteSource ? CProgressWindow::Move : CProgressWindow::Copy, CProgressWindow::Receive, 1, m_ProtoClient);
    progressWnd.Show();

    //Copy content
    for (vector< pair<int, PluginPanelItem *> >::const_iterator it = subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        for (int i = 0; i < it->first; ++i)
        {
            PluginPanelItem *pi = &it->second[i];
            const bool isDirectory = (pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            std::wstring localPath = *destPath;
            ::AppendWChar(localPath, L'\\');
            localPath += pi->FindData.lpwszFileName;

            std::wstring remotePath = m_ProtoClient->GetCurrentDirectory();
            if (remotePath.compare(L"/") != 0)
            {
                remotePath += L'/';
            }
            remotePath += pi->FindData.lpwszFileName;
            // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: remotePath = %s", remotePath);
            size_t slash;
            while((slash = remotePath.find(L'\\')) != string::npos)
            {
                remotePath[slash] = L'/';
            }

            bool success = true;
            // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: isDirectory = %d", isDirectory);
            if (isDirectory)
            {
                //Create destination directory
                while(!CreateDirectory(localPath.c_str(), NULL))
                {
                    const int errCode = GetLastError();
                    if (errCode == ERROR_ALREADY_EXISTS)
                    {
                        break;
                    }
                    std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrCreateDir, localPath.c_str());
                    const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_RETRYCANCEL | FMSG_WARNING | FMSG_ERRORTYPE);
                    if (retCode != 0)
                    {
                        return -1;    //Abort
                    }
                }
            }
            else
            {
                //Copy file
                progressWnd.SetFileNames(remotePath.c_str(), localPath.c_str());
                std::wstring errInfo;
                // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: remotePath = %s", remotePath.c_str());
                while (!m_ProtoClient->GetFile(remotePath.c_str(), localPath.c_str(), pi->FindData.nFileSize, errInfo))
                {
                    if (WaitForSingleObject(m_AbortTask, 0) == WAIT_OBJECT_0)
                    {
                        return -1;
                    }

                    std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrCopyFile, remotePath.c_str(), localPath.c_str());
                    taskErrorMsg += L'\n';
                    taskErrorMsg += errInfo;
                    const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING);
                    if (retCode <= 0)
                    {
                        return -1;    //Abort
                    }
                    if (retCode == 2)   //Ignore
                    {
                        success = false;
                        break;
                    }
                    //Retry
                }
                // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: after GetFile: success = %u", success);
            }

            if (success && deleteSource)
            {
                if (isDirectory)
                {
                    dirsToRemove.push_back(remotePath);    //Delay removing
                }
                else
                {
                    //Remove source (remote) file
                    CNotificationWindow progressWndDel(CFarPlugin::GetString(StringTitle), CFarPlugin::GetString(StringPrgDelete));
                    progressWndDel.Show();

                    std::wstring errInfo;
                    while (!m_ProtoClient->Delete(remotePath.c_str(), IProtocol::ItemFile, errInfo))
                    {
                        progressWndDel.Hide();
                        std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrDeleteFile, remotePath.c_str());
                        taskErrorMsg += L'\n';
                        taskErrorMsg += errInfo;
                        const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING);
                        if (retCode <= 0)
                        {
                            return -1;    //Abort
                        }
                        if (retCode == 2)   //Ignore
                        {
                            success = false;
                            break;
                        }
                        progressWndDel.Show();
                    }
                }
            }

            if (success)
            {
                pi->Flags ^= PPIF_SELECTED;
            }
        }
    }

    //Delay directory removing
    sort(dirsToRemove.begin(), dirsToRemove.end());
    CNotificationWindow progressWndDel(CFarPlugin::GetString(StringTitle), CFarPlugin::GetString(StringPrgDelete));
    for (vector<wstring>::const_reverse_iterator it = dirsToRemove.rbegin(); it != dirsToRemove.rend(); ++it)
    {
        std::wstring errInfo;
        progressWndDel.Show();
        while (!m_ProtoClient->Delete(it->c_str(), IProtocol::ItemDirectory, errInfo))
        {
            progressWndDel.Hide();
            std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrDeleteDir, it->c_str());
            taskErrorMsg += L'\n';
            taskErrorMsg += errInfo;
            const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING);
            if (retCode <= 0)
            {
                return -1;    //Abort
            }
            if (retCode == 2)   //Ignore
            {
                break;
            }
            progressWndDel.Show();
        }
    }

    //Free content
    for (vector< pair<int, PluginPanelItem *> >::const_iterator it = ++subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        if (it->first > 0)
        {
            // CFarPlugin::GetPSI()->FreePluginDirList(it->second, it->first);
        }
    }
    // DEBUG_PRINTF(L"NetBox: CPanel::GetFiles: end");
    return 1;
}


int CPanel::PutFiles(const wchar_t *sourcePath, PluginPanelItem *panelItem, const int itemsNumber, const bool deleteSource, const int /*opMode*/)
{
    // DEBUG_PRINTF(L"NetBox: CPanel::PutFiles: begin: sourcePath = %s", sourcePath);
    assert(m_ProtoClient);

    if (itemsNumber == 1 && wcscmp(panelItem->FindData.lpwszFileName, L"..") == 0)
    {
        return 0;
    }

    //Full copied content (include subdirectories)
    vector< pair<int, FAR_FIND_DATA *> > subDirContent;

    //Get full copied content (top folder)
    vector<FAR_FIND_DATA> topItems;
    for (int i = 0; i < itemsNumber; ++i)
    {
        PluginPanelItem *pi = &panelItem[i];
        topItems.push_back(pi->FindData);
    }
    subDirContent.push_back(make_pair(static_cast<int>(topItems.size()), topItems.empty() ? NULL : &topItems.front()));

    //Get full copied content (sub folder)
    for (int i = 0; i < itemsNumber; ++i)
    {
        PluginPanelItem *pi = &panelItem[i];
        if (pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            FAR_FIND_DATA *subItems = NULL;
            int subItemsNum = 0;
            std::wstring localPath = sourcePath;
            ::AppendWChar(localPath, L'\\');
            localPath += pi->FindData.lpwszFileName;
            if (!CFarPlugin::GetPSI()->GetDirList(localPath.c_str(), &subItems, &subItemsNum))
            {
                return 0;
            }
            subDirContent.push_back(make_pair(subItemsNum, subItems));
        }
    }

    //Calculate total count/size and get content
    size_t totalFileCount = 0;
    unsigned __int64 totalFileSize = 0;
    for (vector< pair<int, FAR_FIND_DATA *> >::const_iterator it = subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        for (int i = 0; i < it->first; ++i)
        {
            FAR_FIND_DATA *fd = &it->second[i];
            if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                ++totalFileCount;
                totalFileSize += fd->nFileSize;
            }
        }
    }

    std::wstring localRelativePath = sourcePath;
    ::AppendWChar(localRelativePath, L'\\');

    //Directory to remove list
    vector<wstring> dirsToRemove;

    CProgressWindow progressWnd(m_AbortTask, deleteSource ? CProgressWindow::Move : CProgressWindow::Copy, CProgressWindow::Send, 1, m_ProtoClient);
    progressWnd.Show();

    //Copy content
    for (vector< pair<int, FAR_FIND_DATA *> >::const_iterator it = subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        for (int i = 0; i != it->first; ++i)
        {
            FAR_FIND_DATA *fd = &it->second[i];
            const bool isDirectory = (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            std::wstring localPath;
            if (wcschr(fd->lpwszFileName, L'\\'))
            {
                localPath = fd->lpwszFileName;
            }
            else
            {
                localPath = localRelativePath;
                localPath += fd->lpwszFileName;
            }

            std::wstring remotePath = m_ProtoClient->GetCurrentDirectory();
            if (remotePath.compare(L"/") != 0)
            {
                remotePath += L'/';
            }
            remotePath += localPath.substr(localRelativePath.length());
            size_t slash;
            while((slash = remotePath.find(L'\\')) != string::npos)
            {
                remotePath[slash] = L'/';
            }

            bool success = true;

            // DEBUG_PRINTF(L"NetBox: CPanel::PutFiles: isDirectory = %d", isDirectory);
            if (isDirectory)
            {
                //Create destination directory
                bool dirExist = false;
                std::wstring errInfo;
                // DEBUG_PRINTF(L"NetBox: PutFiles: remotePath = %s", remotePath.c_str());
                if (!m_ProtoClient->CheckExisting(remotePath.c_str(), IProtocol::ItemDirectory, dirExist, errInfo))
                {
                    return -1;
                }
                // DEBUG_PRINTF(L"NetBox: CPanel::PutFiles: dirExist = %d", dirExist);
                if (!dirExist)
                {
                    while (!m_ProtoClient->MakeDirectory(remotePath.c_str(), errInfo))
                    {
                        std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrCreateDir, remotePath.c_str());
                        taskErrorMsg += L'\n';
                        taskErrorMsg += errInfo;
                        const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_RETRYCANCEL | FMSG_WARNING);
                        if (retCode != 0)
                        {
                            return -1;    //Abort
                        }
                    }
                }
            }
            else
            {
                //Copy file
                progressWnd.SetFileNames(localPath.c_str(), remotePath.c_str());
                std::wstring errInfo;
                while (!m_ProtoClient->PutFile(remotePath.c_str(), localPath.c_str(), fd->nFileSize, errInfo))
                {
                    if (WaitForSingleObject(m_AbortTask, 0) == WAIT_OBJECT_0)
                    {
                        return -1;
                    }
                    std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrCopyFile, localPath.c_str(), remotePath.c_str());
                    taskErrorMsg += L'\n';
                    taskErrorMsg += errInfo;
                    const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING);
                    if (retCode <= 0)
                    {
                        return -1;    //Abort
                    }
                    if (retCode == 2)   //Ignore
                    {
                        success = false;
                        break;
                    }
                    //Retry
                }
            }

            if (success && deleteSource)
            {
                if (isDirectory)
                {
                    dirsToRemove.push_back(localPath);    //Delay removing
                }
                else
                {
                    //Remove source (local) file
                    while(!DeleteFile(localPath.c_str()))
                    {
                        const int errCode = GetLastError();
                        success = (errCode == ERROR_FILE_NOT_FOUND);
                        if (!success)
                        {
                            const std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrDeleteFile, localPath.c_str());
                            const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING | FMSG_ERRORTYPE);
                            if (retCode <= 0)
                            {
                                return -1;    //Abort
                            }
                            if (retCode == 2)   //Ignore
                            {
                                success = true;
                                break;
                            }
                        }
                    }
                }
            }

            for (int i = 0; success && i < itemsNumber; ++i)
            {
                PluginPanelItem *pi = &panelItem[i];
                if (&pi->FindData == fd)
                {
                    pi->Flags ^= PPIF_SELECTED;
                }
            }
        }
    }

    //Delay directory removing
    sort(dirsToRemove.begin(), dirsToRemove.end());
    for (vector<wstring>::const_reverse_iterator it = dirsToRemove.rbegin(); it != dirsToRemove.rend(); ++it)
    {
        while(!RemoveDirectory(it->c_str()))
        {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                break;
            }
            const std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(StringErrDeleteDir, it->c_str());
            const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING | FMSG_ERRORTYPE);
            if (retCode <= 0)
            {
                return -1;    //Abort
            }
            if (retCode == 2)   //Ignore
            {
                break;
            }
        }
    }

    //Free content
    for (vector< pair<int, FAR_FIND_DATA *> >::const_iterator it = ++subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        CFarPlugin::GetPSI()->FreeDirList(it->second, it->first);
    }
    // DEBUG_PRINTF(L"NetBox: CPanel::PutFiles: end");
    return 1;
}


int CPanel::DeleteFiles(PluginPanelItem *panelItem, int itemsNumber, const int opMode)
{
    assert(m_ProtoClient);
    // DEBUG_PRINTF(L"NetBox: DeleteFiles: begin");
    if (itemsNumber == 1 && wcscmp(panelItem->FindData.lpwszFileName, L"..") == 0)
    {
        return 0;
    }

    if (!IS_SILENT(opMode))
    {
        CFarDialog dlg(50, itemsNumber > 1 ? 7 : 8, CFarPlugin::GetString(StringDelTitle));
        std::wstring question(CFarPlugin::GetString(StringDelQuestion));
        std::wstring fileName;
        question += L' ';
        if (itemsNumber > 1)
        {
            question += CFarPlugin::GetString(StringDelSelected);
        }
        else
        {
            const bool isDirectory = (panelItem->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (!isDirectory && IsSessionManager())
            {
                question += CFarPlugin::GetString(StringDelQuestSession);
            }
            else
            {
                question += CFarPlugin::GetString(isDirectory ? StringDelQuestFolder : StringDelQuestFile);
            }
            fileName = panelItem->FindData.lpwszFileName;
            if (fileName.length() > 38)
            {
                fileName = L"..." + fileName.substr(fileName.length() - 35);
            }
            dlg.CreateText(dlg.GetTop() + 1, fileName.c_str());
        }
        dlg.CreateText(dlg.GetTop() + 0, question.c_str());
        dlg.CreateSeparator(dlg.GetHeight() - 2);
        dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(StringDelBtnDelete), DIF_CENTERGROUP);
        const int idBtnCancel = dlg.CreateButton(0, dlg.GetHeight() - 1, CFarPlugin::GetString(StringCancel), DIF_CENTERGROUP);
        const int retCode = dlg.DoModal();
        if (retCode < 0 || retCode == idBtnCancel)
        {
            return 0;
        }
    }

    //Full removed content (include subdirectories)
    vector< pair<int, PluginPanelItem *> > subDirContent;

    //Get full removed content
    subDirContent.push_back(make_pair(itemsNumber, panelItem));
    for (int i = 0; i < itemsNumber; ++i)
    {
        PluginPanelItem *pi = &panelItem[i];
        if (pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            PluginPanelItem *subItems = NULL;
            int subItemsNum = 0;
            if (!CFarPlugin::GetPSI()->GetPluginDirList(CFarPlugin::GetPSI()->ModuleNumber, this, pi->FindData.lpwszFileName, &subItems, &subItemsNum))
            {
                return 0;
            }
            subDirContent.push_back(make_pair(subItemsNum, subItems));
        }
    }

    //Remove content
    CNotificationWindow progressWnd(CFarPlugin::GetString(StringTitle), CFarPlugin::GetString(StringPrgDelete));
    progressWnd.Show();
    for (vector< pair<int, PluginPanelItem *> >::const_reverse_iterator it = subDirContent.rbegin(); it != subDirContent.rend(); ++it)
    {
        for (int i = it->first - 1; i >= 0; --i)
        {
            PluginPanelItem *pi = &it->second[i];

            std::wstring remotePath = m_ProtoClient->GetCurrentDirectory();
            if (remotePath.compare(L"/") != 0)
            {
                remotePath += L'/';
            }
            remotePath += pi->FindData.lpwszFileName;
            size_t slash;
            while((slash = remotePath.find(L'\\')) != string::npos)
            {
                remotePath[slash] = L'/';
            }

            const bool isDirectory = (pi->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

            bool removed = true;
            std::wstring errInfo;
            while (!m_ProtoClient->Delete(remotePath.c_str(), isDirectory ? IProtocol::ItemDirectory : IProtocol::ItemFile, errInfo))
            {
                progressWnd.Hide();
                removed = false;
                std::wstring taskErrorMsg = CFarPlugin::GetFormattedString(isDirectory ? StringErrDeleteDir : StringErrDeleteFile, remotePath.c_str());
                taskErrorMsg += L'\n';
                taskErrorMsg += errInfo;
                const int retCode = CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), taskErrorMsg.c_str(), FMSG_MB_ABORTRETRYIGNORE | FMSG_WARNING);
                if (retCode <= 0)
                {
                    return 0;    //Abort
                }
                if (retCode == 2)   //Ignore
                {
                    progressWnd.Show();
                    break;
                }
                removed = true;
                progressWnd.Show();
            }
            if (removed)
            {
                pi->Flags ^= PPIF_SELECTED;
            }
        }
    }

    //Free content
    for (vector< pair<int, PluginPanelItem *> >::const_iterator it = ++subDirContent.begin(); it != subDirContent.end(); ++it)
    {
        if (it->first > 0)
        {
            // CFarPlugin::GetPSI()->FreePluginDirList(it->second, it->first);
        }
    }
    // DEBUG_PRINTF(L"NetBox: DeleteFiles: end");
    return 1;
}


void CPanel::UpdateTitle()
{
    m_Title = CFarPlugin::GetString(StringTitle);
    if (m_ProtoClient)
    {
        m_Title += L": ";
        m_Title += m_ProtoClient->GetURL();
        m_Title += m_ProtoClient->GetCurrentDirectory();
    }
}


void CPanel::ShowErrorDialog(const DWORD errCode, const std::wstring &title, const wchar_t *info /*= NULL*/) const
{
    std::wstring errInfo;
    if (!title.empty())
    {
        errInfo = title;
    }

    if (errCode)
    {
        if (!errInfo.empty())
        {
            errInfo += L'\n';
        }
        errInfo += GetSystemErrorMessage(errCode);
    }

    if (info && wcslen(info))
    {
        if (!errInfo.empty())
        {
            errInfo += L'\n';
        }
        errInfo += info;
    }

    //Replace all '\r\n' to '\n'
    size_t rnPos;
    while ((rnPos = errInfo.find('\r')) != string::npos)
    {
        errInfo[rnPos] = ' ';
    }

    CFarPlugin::MessageBox(CFarPlugin::GetString(StringTitle), errInfo.c_str(), FMSG_MB_OK | FMSG_WARNING);
}

void CPanel::ResetAbortTask()
{
    if (!m_AbortTask)
    {
        m_AbortTask = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!m_AbortTask)
        {
            ShowErrorDialog(GetLastError(), L"Create event failed");
        }
    }
    ResetEvent(m_AbortTask);
}
