#ifndef FRM_FORCE_REMOVE_HPP
#define FRM_FORCE_REMOVE_HPP

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>

#include <windows.h>
#include <winternl.h>

#include "logger.hpp"
#include "nt.hpp"
#include "trie.hpp"
#include "utils.hpp"

void DeleteRecursively(const std::wstring &widePath, Trie &inUseFiles, Logger &logger) {
  const std::wstring kernelNameWide = GetKernelName(widePath);
  const std::string kernelNameUtf8 = WideToUtf8(kernelNameWide);

  // Unset read-only attribute if set
  DWORD attributes = GetFileAttributesW(widePath.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    logger.error("Failed to get file attributes for: " + kernelNameUtf8);
    return;
  }

  DWORD originalAttributes = attributes;
  attributes &= ~FILE_ATTRIBUTE_HIDDEN;
  attributes &= ~FILE_ATTRIBUTE_READONLY;
  attributes &= ~FILE_ATTRIBUTE_SYSTEM;

  if (attributes == originalAttributes) {
    logger.debug("No need to change file attributes for: " + kernelNameUtf8);
  } else if (SetFileAttributesW(widePath.c_str(), attributes)) {
    logger.info("Unset read-only attribute for: " + kernelNameUtf8);
  } else {
    logger.error("Failed to unset read-only attribute for: " + kernelNameUtf8);
    return;
  }

  // Detect if the path is a directory or a file
  if (std::filesystem::is_directory(widePath)) {
    logger.debug("Detected directory: " + kernelNameUtf8);
    std::wstring searchPath = PathCombine(widePath, L"*");
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
      logger.error("Failed to list directory contents for: " + kernelNameUtf8);
    else {
      do {
        std::wstring entryName(findData.cFileName);
        if (entryName == L"." || entryName == L"..") continue;
        std::wstring fullEntryPath = PathCombine(widePath, entryName);
        DeleteRecursively(fullEntryPath, inUseFiles, logger);
      } while (FindNextFileW(hFind, &findData));
    }
    CloseHandle(hFind);
    if (RemoveDirectoryW(widePath.c_str()))
      logger.info("Directory deleted successfully: " + kernelNameUtf8);
    else
      logger.warning("Failed to delete directory: " + kernelNameUtf8);
    return;
  }

  // Attempt to delete the file
  if (DeleteFileByNt(kernelNameWide))
    logger.info("File deleted successfully: " + kernelNameUtf8);
  else {
    logger.warning("Failed to delete file: " + kernelNameUtf8);
    inUseFiles.insert(kernelNameWide.c_str(), kernelNameWide.size() * sizeof(wchar_t));
  }
}

void ReleaseHandles(Trie &files, Logger &logger) {
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
    std::wstring handleKernelName = GetKernelName(dupHandle);
    CloseHandle(dupHandle), dupHandle = nullptr;
    if (!files.exists(handleKernelName.c_str(), handleKernelName.size() * sizeof(wchar_t))) continue;

    logger.info("Found handle " + std::to_string(remoteHandle) + " in process " + std::to_string(pid) +
                " for file: " + WideToUtf8(handleKernelName));
    if (!DuplicateHandle(hProcess, (HANDLE)(ULONG_PTR)remoteHandle, GetCurrentProcess(), &dupHandle, 0, FALSE,
                         DUPLICATE_CLOSE_SOURCE)) {
      logger.error("Failed to close handle: " + std::to_string((std::uintptr_t)remoteHandle) +
                   " in process: " + std::to_string(pid));
    } else {
      logger.info("Closed handle: " + std::to_string((std::uintptr_t)remoteHandle) +
                  " in process: " + std::to_string(pid));
      CloseHandle(dupHandle);
    }
  }
  if (hProcess) CloseHandle(hProcess);
  free(handleInfo);
}

void ForceRemove(const std::string &pathname, Logger &logger) {
  // Check if the path exists
  std::filesystem::path path(pathname);
  if (!std::filesystem::exists(path)) {
    logger.error("Path does not exist: " + pathname);
    return;
  }

  auto strip = [](const std::string &str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
  };

  // Attempt to remove the path using std::filesystem::remove_all
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
  if (ec) {
    logger.warning("Failed to remove using std::filesystem::remove_all: " + strip(ec.message()));
  } else {
    logger.info("Path removed successfully using std::filesystem::remove_all.");
    return;
  }

  try {
    if (!IsRunAsAdmin()) {
      logger.error("The program must be run as an administrator to force remove files or directories.");
      return;
    }

    // Set Privileges
    if (!SetPrivilege(L"SeBackupPrivilege") || !SetPrivilege(L"SeRestorePrivilege") ||
        !SetPrivilege(L"SeDebugPrivilege") || !SetPrivilege(L"SeTakeOwnershipPrivilege")) {
      logger.error("Failed to set required privileges.");
      return;
    }

    // Recursively delete the path and track in-use files
    Trie inUseFiles;
    DeleteRecursively(GetFullPath(Utf8ToWide(pathname)), inUseFiles, logger);

    // Release handles for in-use files
    ReleaseHandles(inUseFiles, logger);

    // Retry deleting path after releasing handles
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
      logger.error("Failed to remove: " + strip(ec.message()));
    } else {
      logger.info("Path removed successfully after releasing handles.");
    }
  } catch (const std::bad_alloc &e) {
    logger.error(std::string("Memory allocation failed: ") + strip(e.what()));
  } catch (const std::system_error &e) {
    logger.error(std::string("System error: ") + strip(e.what()));
  } catch (const std::exception &e) { logger.error(std::string("Unexpected error: ") + strip(e.what())); }
}

#endif // FRM_FORCE_REMOVE_HPP