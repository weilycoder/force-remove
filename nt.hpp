#ifndef FRM_NT_HELPERS_HPP
#define FRM_NT_HELPERS_HPP

#include <string>
#include <windows.h>
#include <winternl.h>

std::wstring GetKernelName(HANDLE hFile);
std::wstring GetKernelName(const std::wstring &path);

bool DeleteFileByNt(const std::wstring &kernelName);

PSYSTEM_HANDLE_INFORMATION GetAllHandles();

#endif // FRM_NT_HELPERS_HPP