#include "utils.h"

#include <Windows.h>

void execProc(const fs_str_t& cmdLine)
{
    auto cmdCopy = cmdLine + L"\0";

    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION procInfo;

    ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    ZeroMemory(&procInfo, sizeof(PROCESS_INFORMATION));

    CreateProcessW(
        NULL,
        cmdCopy.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startupInfo,
        &procInfo
    );

    WaitForSingleObject(procInfo.hProcess, INFINITE);
    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);
}