#ifndef FRM_UTILS_HPP
#define FRM_UTILS_HPP

#include <string>

std::wstring Utf8ToWide(const std::string &utf8);
std::string WideToUtf8(const std::wstring &wide);
std::wstring GetFullPath(const std::wstring &path);
std::wstring PathCombine(const std::wstring &path1, const std::wstring &path2);

bool IsRunAsAdmin();
bool SetPrivilege(const std::wstring &privilegeName);

bool CloseRemoteHandle(HANDLE hProcess, HANDLE handle);

#endif // FRM_UTILS_HPP