#include "release-exec.hpp"
#include "nt.hpp"
#include "utils.hpp"

typedef WINBOOL(WINAPI *pFreeLibrary)(HMODULE hLibModule);

static HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
static pFreeLibrary _FreeLibrary = reinterpret_cast<pFreeLibrary>(GetProcAddress(hKernel32, "FreeLibrary"));

#define lpFreeLibrary reinterpret_cast<LPTHREAD_START_ROUTINE>(_FreeLibrary)

void ReleaseExecutingFiles(DWORD processId, Trie &files, Logger &logger) {
  HANDLE hSnapshot;
  for (;;) {
    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (hSnapshot != INVALID_HANDLE_VALUE) break;
    if (GetLastError() != ERROR_BAD_LENGTH) return; // PPL
  }

  MODULEENTRY32W me32;
  me32.dwSize = sizeof(MODULEENTRY32W);
  if (!Module32FirstW(hSnapshot, &me32)) {
    const DWORD lastError = GetLastError();
    CloseHandle(hSnapshot);
    logger.error("Failed to get modules for process " + std::to_string(processId));
    return;
  }

  bool isMainModule = true;
  do {
    std::wstring modulePath(me32.szExePath);
    std::wstring kernelName = GetFileKernelName(modulePath);
    if (!files.exists(kernelName.c_str(), kernelName.size() * sizeof(wchar_t))) continue;
    logger.info("Found module " + WideToUtf8(me32.szModule) + " in process " + std::to_string(processId));

    if (isMainModule) {
      HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
      if (!hProcess) {
        logger.error("Failed to open process " + std::to_string(processId) +
                     " to terminate for releasing module: " + WideToUtf8(me32.szModule));
        continue;
      }
      if (TerminateProcess(hProcess, 0)) {
        WaitForSingleObject(hProcess, INFINITE);
        logger.info("Terminated process " + std::to_string(processId));
      } else {
        logger.error("Failed to terminate process " + std::to_string(processId));
      }
      CloseHandle(hProcess);
      break; // No need to continue after terminating the main module
    } else {
      HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                        PROCESS_VM_WRITE | PROCESS_VM_READ,
                                    FALSE, processId);
      if (!hProcess) {
        logger.error("Failed to open process " + std::to_string(processId) +
                     " to release module: " + WideToUtf8(me32.szModule));
        continue;
      }

      HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, lpFreeLibrary, me32.hModule, 0, NULL);
      if (!hThread) {
        logger.error("Failed to create remote thread in process " + std::to_string(processId) +
                     " to release module: " + WideToUtf8(me32.szModule));
        CloseHandle(hProcess);
        continue;
      }
      WaitForSingleObject(hThread, INFINITE);
      CloseHandle(hThread);
      CloseHandle(hProcess);
      logger.info("Released module " + WideToUtf8(me32.szModule) + " in process " + std::to_string(processId));
    }
  } while (isMainModule = false, Module32NextW(hSnapshot, &me32));
  CloseHandle(hSnapshot);
}
