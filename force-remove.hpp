#ifndef FRM_FORCE_REMOVE_HPP
#define FRM_FORCE_REMOVE_HPP

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <windows.h>
#pragma comment(lib, "advapi32.lib")

class Logger {
public:
  static constexpr std::int32_t debug_v = 0;
  static constexpr std::int32_t info_v = 1;
  static constexpr std::int32_t warning_v = 2;
  static constexpr std::int32_t error_v = 3;

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

void _ForceRemove(const std::wstring &widePath, Logger &logger);

void ForceRemove(const std::string &pathname, Logger &logger) {
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

  // Step 2: Convert path to wide string and format it
  _ForceRemove(GetFullPath(Utf8ToWide(pathname)), logger);
}

void _ForceRemove(const std::wstring &widePath, Logger &logger) {
  const std::string name = WideToUtf8(widePath);

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
    logger.debug("File attributes changed successfully for: " + name);
  } else {
    logger.error("Failed to change file attributes for: " + name);
    return;
  }

  // Step 3: Detect if the path is a directory or a file
  if (std::filesystem::is_directory(widePath)) {
    logger.debug("Detected directory: " + name);
    // TODO: Recursively remove directory contents
    logger.warning("Directory removal not implemented yet for: " + name);
    return;
  }

  // Step 4: Attempt to delete the file
  if (DeleteFileW(widePath.c_str())) {
    logger.info("File deleted successfully: " + name);
    return;
  } else
    logger.warning("Failed to delete file: " + name);

  // TODO: Search for the file handle and force close it
  logger.warning("Force closing file handles not implemented yet for: " + name);
}

#endif // FRM_FORCE_REMOVE_HPP