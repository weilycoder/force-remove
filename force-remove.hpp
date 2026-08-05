#ifndef FRM_FORCE_REMOVE_HPP
#define FRM_FORCE_REMOVE_HPP

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <pathcch.h>
#include <windows.h>
#include <winternl.h>

#pragma comment(lib, "pathcch.lib")

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004UL)

typedef NTSTATUS(NTAPI *pNtQueryObject)(HANDLE Handle, OBJECT_INFORMATION_CLASS ObjectInformationClass,
                                        PVOID ObjectInformation, ULONG ObjectInformationLength,
                                        PULONG ReturnLength);
typedef NTSTATUS(NTAPI *pRtlNtStatusToDosError)(NTSTATUS Status);
typedef NTSTATUS(NTAPI *pNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                                   PVOID SystemInformation, ULONG SystemInformationLength,
                                                   PULONG ReturnLength);

#define NtQueryObject _NtQueryObject
#define RtlNtStatusToDosError _RtlNtStatusToDosError
#define NtQuerySystemInformation _NtQuerySystemInformation

HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");
pNtQueryObject _NtQueryObject = reinterpret_cast<pNtQueryObject>(GetProcAddress(hNtdll, "NtQueryObject"));
pRtlNtStatusToDosError _RtlNtStatusToDosError =
    reinterpret_cast<pRtlNtStatusToDosError>(GetProcAddress(hNtdll, "RtlNtStatusToDosError"));
pNtQuerySystemInformation _NtQuerySystemInformation =
    reinterpret_cast<pNtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));

class Logger {
public:
  static constexpr std::int32_t debug_v = 0;
  static constexpr std::int32_t info_v = 1;
  static constexpr std::int32_t warning_v = 2;
  static constexpr std::int32_t error_v = 3;

  static constexpr std::uint32_t debug_m = 1u << debug_v;
  static constexpr std::uint32_t info_m = 1u << info_v;
  static constexpr std::uint32_t warning_m = 1u << warning_v;
  static constexpr std::uint32_t error_m = 1u << error_v;

private:
  static constexpr const char *debug_s = "[DEBUG]   ";
  static constexpr const char *info_s = "[INFO]    ";
  static constexpr const char *warning_s = "[WARNING] ";
  static constexpr const char *error_s = "[ERROR]   ";

  std::int32_t log_level;
  std::uint32_t msg_flag;
  std::ostream &out;

public:
  Logger() : log_level(0), msg_flag(0), out(std::cout) {}
  Logger(std::int32_t level) : log_level(level), msg_flag(0), out(std::cout) {}
  Logger(std::int32_t level, std::ostream &output_stream)
      : log_level(level), msg_flag(0), out(output_stream) {}

  void setLogLevel(std::int32_t level) { log_level = level; }

  void clearMsgFlag() { msg_flag = 0; }
  std::uint32_t getMsgFlag() const { return msg_flag; }

#define DEF_LOG(level)                                                                                       \
  void level(const std::string &message) {                                                                   \
    msg_flag |= 1u << level##_v;                                                                             \
    if (log_level <= level##_v) out << level##_s << message << std::endl;                                    \
  }

  DEF_LOG(debug);
  DEF_LOG(info);
  DEF_LOG(warning);
  DEF_LOG(error);

#undef DEF_LOG
};

bool IsRunAsAdmin() {
  BOOL isAdmin = FALSE;
  PSID adminGroup = nullptr;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0,
                               0, 0, 0, &adminGroup)) {
    CheckTokenMembership(nullptr, adminGroup, &isAdmin);
    FreeSid(adminGroup);
  }
  return isAdmin != FALSE;
}

bool SetPrivilege(const std::wstring &privilegeName) {
  HANDLE hToken = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;

  TOKEN_PRIVILEGES tp;
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (!LookupPrivilegeValueW(nullptr, privilegeName.c_str(), &tp.Privileges[0].Luid)) {
    CloseHandle(hToken);
    return false;
  }

  BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
  CloseHandle(hToken);

  return result;
}

std::wstring Utf8ToWide(const std::string &utf8) {
  if (utf8.empty()) return std::wstring();
  int utf8Len = static_cast<int>(utf8.size());
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8Len, nullptr, 0);
  if (len == 0)
    throw std::system_error(GetLastError(), std::system_category(), "MultiByteToWideChar (query) failed");
  std::wstring wide(len, L'\0');
  int converted = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8Len, &wide[0], len);
  if (converted != len)
    throw std::system_error(GetLastError(), std::system_category(), "MultiByteToWideChar (query) failed");
  return wide;
}

std::string WideToUtf8(const std::wstring &wide) {
  if (wide.empty()) return std::string();
  int wideLen = static_cast<int>(wide.size());
  int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, nullptr, 0, nullptr, nullptr);
  if (len == 0)
    throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte (query) failed");
  std::string utf8(len, '\0');
  int converted = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, &utf8[0], len, nullptr, nullptr);
  if (converted != len)
    throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte (query) failed");
  return utf8;
}

std::wstring GetFullPath(const std::wstring &path) {
  DWORD bufferSize = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (bufferSize == 0)
    throw std::system_error(GetLastError(), std::system_category(), "GetFullPathNameW failed");

  std::wstring fullPath(bufferSize, L'\0');
  DWORD result = GetFullPathNameW(path.c_str(), bufferSize, &fullPath[0], nullptr);
  if (result != bufferSize - 1)
    throw std::system_error(GetLastError(), std::system_category(), "GetFullPathNameW failed");

  fullPath.resize(result);
  return fullPath;
}

std::wstring PathCombine(const std::wstring &path1, const std::wstring &path2) {
  PWSTR combinedPath = nullptr;
  HRESULT hr = PathAllocCombine(path1.c_str(), path2.c_str(), 1, &combinedPath);
  if (FAILED(hr)) throw std::system_error(hr, std::system_category(), "PathAllocCombine failed");
  std::wstring result(combinedPath);
  LocalFree(combinedPath);
  return result;
}

std::wstring GetKernelName(HANDLE hFile) {
  if (GetFileType(hFile) != FILE_TYPE_DISK) return L""; // Not a disk file

#define name_info reinterpret_cast<POBJECT_NAME_INFORMATION>(buffer)
  void *buffer = nullptr;
  ULONG returnedLength = 0x1000;
  NTSTATUS status = 0;

  while (buffer == nullptr) {
    buffer = malloc(returnedLength);
    if (buffer == nullptr) throw std::bad_alloc();
    status = NtQueryObject(hFile, ObjectNameInformation, name_info, returnedLength, &returnedLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH) {
      free(buffer), buffer = nullptr;
      returnedLength <<= 1; // Double the size considering more handle may be created
    }
  }

  if (!NT_SUCCESS(status)) {
    free(buffer);
    throw std::system_error(RtlNtStatusToDosError(status), std::system_category(), "NtQueryObject failed");
  }

  std::wstring kernelName(name_info->Name.Buffer, name_info->Name.Length / sizeof(WCHAR));
  free(buffer);
#undef name_info

  return kernelName;
}

std::wstring GetKernelName(const std::wstring &path) {
  HANDLE file_handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file_handle == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(), "CreateFileW failed");
  const auto kernelName = GetKernelName(file_handle);
  CloseHandle(file_handle);
  return kernelName;
}

PSYSTEM_HANDLE_INFORMATION GetAllHandles() {
  void *buffer = nullptr;
  ULONG returnedLength = 0x1000;
  NTSTATUS status = 0;

  while (buffer == nullptr) {
    buffer = malloc(returnedLength);
    if (buffer == nullptr) throw std::bad_alloc();
    status = NtQuerySystemInformation(SystemHandleInformation, buffer, returnedLength, &returnedLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH) {
      free(buffer), buffer = nullptr;
      returnedLength <<= 1; // Double the size considering more handle may be created
    }
  }

  if (!NT_SUCCESS(status)) {
    free(buffer);
    throw std::system_error(RtlNtStatusToDosError(status), std::system_category(),
                            "NtQuerySystemInformation failed");
  }

  return reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer);
}

void _ForceRemove(const std::wstring &widePath, Logger &logger);

void ForceRemove(const std::string &pathname, Logger &logger) {
  try {
    if (!IsRunAsAdmin()) {
      logger.error("This program must be run as administrator.");
      return;
    }

    // Step 1: Set Privileges
    if (!SetPrivilege(L"SeBackupPrivilege") || !SetPrivilege(L"SeRestorePrivilege") ||
        !SetPrivilege(L"SeDebugPrivilege") || !SetPrivilege(L"SeTakeOwnershipPrivilege")) {
      logger.error("Failed to set required privileges.");
      return;
    }

    // Step 2: Convert path to wide string
    _ForceRemove(GetFullPath(Utf8ToWide(pathname)), logger);
  } catch (const std::bad_alloc &e) {
    logger.error(std::string("Memory allocation failed: ") + e.what());
  } catch (const std::system_error &e) {
    logger.error(std::string("System error: ") + e.what());
  } catch (const std::exception &e) { logger.error(std::string("Unexpected error: ") + e.what()); }
}

void _ForceRemove(const std::wstring &widePath, Logger &logger) {
  const std::wstring nameW = GetKernelName(widePath);
  const std::string name = WideToUtf8(nameW);

  // Step 3: Unset read-only attribute if set
  DWORD attributes = GetFileAttributesW(widePath.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    logger.error("Failed to get file attributes for: " + name);
    return;
  }

  DWORD originalAttributes = attributes;
  attributes &= ~FILE_ATTRIBUTE_HIDDEN;
  attributes &= ~FILE_ATTRIBUTE_READONLY;
  attributes &= ~FILE_ATTRIBUTE_SYSTEM;

  if (attributes == originalAttributes) {
    logger.debug("No need to change file attributes for: " + name);
  } else if (SetFileAttributesW(widePath.c_str(), attributes)) {
    logger.info("Unset read-only attribute for: " + name);
  } else {
    logger.error("Failed to unset read-only attribute for: " + name);
    return;
  }

  // Step 3: Detect if the path is a directory or a file
  if (std::filesystem::is_directory(widePath)) {
    logger.debug("Detected directory: " + name);
    std::wstring searchPath = PathCombine(widePath, L"*");
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
      logger.error("Failed to list directory contents for: " + name);
    else {
      do {
        std::wstring entryName(findData.cFileName);
        if (entryName == L"." || entryName == L"..") continue;
        std::wstring fullEntryPath = PathCombine(widePath, entryName);
        _ForceRemove(fullEntryPath, logger);
      } while (FindNextFileW(hFind, &findData));
    }
    CloseHandle(hFind);
    if (RemoveDirectoryW(widePath.c_str()))
      logger.info("Directory deleted successfully: " + name);
    else
      logger.error("Failed to delete directory: " + name);
    return;
  }

  // Step 4: Attempt to delete the file
  if (DeleteFileW(widePath.c_str())) {
    logger.info("File deleted successfully: " + name);
    return;
  } else
    logger.warning("Failed to delete file: " + name);

  // Step 5: Search for the file handle and force close it
  logger.info("Searching for handles for: " + name);
  PSYSTEM_HANDLE_INFORMATION handleInfo = GetAllHandles();
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
    if (handleKernelName != nameW) continue;

    logger.info("Found handle: " + std::to_string(remoteHandle) + " in process: " + std::to_string(pid));
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

  // Step 6: Retry deleting the file after closing handles
  if (DeleteFileW(widePath.c_str()))
    logger.info("File deleted successfully: " + name);
  else
    logger.error("Failed to delete file: " + name);
}

#endif // FRM_FORCE_REMOVE_HPP