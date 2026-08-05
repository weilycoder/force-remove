#include <string>
#include <system_error>

#include <pathcch.h>
#include <windows.h>

#include "utils.hpp"

#pragma comment(lib, "pathcch.lib")

std::wstring Utf8ToWide(const std::string &utf8) {
  if (utf8.empty()) return std::wstring();
  int utf8Len = static_cast<int>(utf8.size());
  int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), utf8Len, nullptr, 0);
  if (len == 0) throw std::system_error(GetLastError(), std::system_category(), "MultiByteToWideChar (query) failed");
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
  if (len == 0) throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte (query) failed");
  std::string utf8(len, '\0');
  int converted = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), wideLen, &utf8[0], len, nullptr, nullptr);
  if (converted != len)
    throw std::system_error(GetLastError(), std::system_category(), "WideCharToMultiByte (query) failed");
  return utf8;
}
std::wstring GetFullPath(const std::wstring &path) {
  DWORD bufferSize = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
  if (bufferSize == 0) throw std::system_error(GetLastError(), std::system_category(), "GetFullPathNameW failed");

  std::wstring fullPath(bufferSize, L'\0');
  DWORD result = GetFullPathNameW(path.c_str(), bufferSize, &fullPath[0], nullptr);
  fullPath.resize(result);
  if (result == 0)
    throw std::system_error(GetLastError(), std::system_category(), "GetFullPathNameW failed");
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

bool IsRunAsAdmin() {
  BOOL isAdmin = FALSE;
  PSID adminGroup = nullptr;
  SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                               &adminGroup)) {
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
