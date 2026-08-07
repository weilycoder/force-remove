#include "release-inuse.hpp"
#include "nt.hpp"
#include "utils.hpp"

void ReleaseInUseHandles(const Trie &files, Logger &logger) {
  // Search for the file handle and force close it
  logger.info("Searching for handles for files in use...");
  PSYSTEM_HANDLE_INFORMATION handleInfo = GetAllHandles();
  logger.debug("Total handles found: " + std::to_string(handleInfo->Count));
  const HANDLE currentProcess = GetCurrentProcess();
  ULONG lastPid = 0;
  HANDLE hProcess = nullptr;
  for (ULONG i = 0; i < handleInfo->Count; ++i) {
    ULONG &pid = handleInfo->Handle[i].OwnerPid;
    USHORT &remoteHandle = handleInfo->Handle[i].HandleValue;

    if (i == 0 || lastPid != pid) {
      lastPid = pid;
      if (hProcess) CloseHandle(hProcess);
      hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    }
    if (!hProcess) continue;

    HANDLE dupHandle = nullptr;
    if (!DuplicateHandle(hProcess, (HANDLE)(ULONG_PTR)remoteHandle, currentProcess, &dupHandle, 0, 0,
                         DUPLICATE_SAME_ACCESS))
      continue;
    std::wstring handleKernelName = GetFileKernelName(dupHandle);
    CloseHandle(dupHandle);
    if (!files.exists(handleKernelName.c_str(), handleKernelName.size() * sizeof(wchar_t))) continue;

    logger.info("Found handle " + std::to_string(remoteHandle) + " in process " + std::to_string(pid) +
                " for file: " + WideToUtf8(handleKernelName));
    if (!CloseRemoteHandle(hProcess, (HANDLE)(ULONG_PTR)remoteHandle))
      logger.error("Failed to close handle: " + std::to_string((std::uintptr_t)remoteHandle) +
                   " in process: " + std::to_string(pid));
    else
      logger.info("Closed handle: " + std::to_string((std::uintptr_t)remoteHandle) +
                  " in process: " + std::to_string(pid));
  }
  if (hProcess) CloseHandle(hProcess);
  free(handleInfo);
}