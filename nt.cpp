#include <string>
#include <system_error>

#include <windows.h>
#include <winternl.h>

#include "nt.hpp"

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004UL)

typedef NTSTATUS(NTAPI *pNtDeleteFile)(POBJECT_ATTRIBUTES ObjectAttributes);
typedef NTSTATUS(NTAPI *pNtQueryObject)(HANDLE Handle, OBJECT_INFORMATION_CLASS ObjectInformationClass,
                                        PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength);
typedef NTSTATUS(NTAPI *pRtlNtStatusToDosError)(NTSTATUS Status);
typedef NTSTATUS(NTAPI *pNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                                   PVOID SystemInformation, ULONG SystemInformationLength,
                                                   PULONG ReturnLength);

#define NtDeleteFile _NtDeleteFile
#define NtQueryObject _NtQueryObject
#define RtlNtStatusToDosError _RtlNtStatusToDosError
#define NtQuerySystemInformation _NtQuerySystemInformation

HMODULE hNtdll = LoadLibraryW(L"ntdll.dll");
pNtDeleteFile _NtDeleteFile = reinterpret_cast<pNtDeleteFile>(GetProcAddress(hNtdll, "NtDeleteFile"));
pNtQueryObject _NtQueryObject = reinterpret_cast<pNtQueryObject>(GetProcAddress(hNtdll, "NtQueryObject"));
pRtlNtStatusToDosError _RtlNtStatusToDosError =
    reinterpret_cast<pRtlNtStatusToDosError>(GetProcAddress(hNtdll, "RtlNtStatusToDosError"));
pNtQuerySystemInformation _NtQuerySystemInformation =
    reinterpret_cast<pNtQuerySystemInformation>(GetProcAddress(hNtdll, "NtQuerySystemInformation"));

std::wstring GetKernelName(HANDLE hFile) {
#define name_info reinterpret_cast<POBJECT_NAME_INFORMATION>(buffer)
  void *buffer = nullptr;
  ULONG returnedLength = 0x1000;
  NTSTATUS status = 0;

  while (buffer == nullptr) {
    buffer = malloc(returnedLength);
    if (buffer == nullptr) throw std::bad_alloc();
    status = NtQueryObject(hFile, ObjectNameInformation, name_info, returnedLength, &returnedLength);
    if (status == STATUS_INFO_LENGTH_MISMATCH) free(buffer), buffer = nullptr;
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

std::wstring GetFileKernelName(HANDLE hFile) {
  if (GetFileType(hFile) != FILE_TYPE_DISK) return L""; // Not a disk file
  return GetKernelName(hFile);
}

std::wstring GetFileKernelName(const std::wstring &path) {
  HANDLE file_handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (file_handle == INVALID_HANDLE_VALUE)
    throw std::system_error(GetLastError(), std::system_category(), "CreateFileW failed");
  const auto kernelName = GetFileKernelName(file_handle);
  CloseHandle(file_handle);
  return kernelName;
}

bool DeleteFileByNt(const std::wstring &kernelName) {
  UNICODE_STRING uniName;
  OBJECT_ATTRIBUTES objAttr;

  uniName.Buffer = (PWSTR)kernelName.c_str();
  uniName.Length = (USHORT)(kernelName.length() * sizeof(wchar_t));
  uniName.MaximumLength = (USHORT)((kernelName.length() + 1) * sizeof(wchar_t));

  objAttr.Length = sizeof(OBJECT_ATTRIBUTES);
  objAttr.RootDirectory = NULL;
  objAttr.ObjectName = &uniName;
  objAttr.Attributes = OBJ_CASE_INSENSITIVE;
  objAttr.SecurityDescriptor = NULL;
  objAttr.SecurityQualityOfService = NULL;

  NTSTATUS status = NtDeleteFile(&objAttr);
  return NT_SUCCESS(status);
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
    throw std::system_error(RtlNtStatusToDosError(status), std::system_category(), "NtQuerySystemInformation failed");
  }

  return reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer);
}