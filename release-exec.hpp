#ifndef FRM_RELEASE_EXE_HPP
#define FRM_RELEASE_EXE_HPP

#include <windows.h>

#include <tlhelp32.h>

#include "logger.hpp"
#include "trie.hpp"

void ReleaseExecutingFiles(DWORD processId, Trie &files, Logger &logger);

#endif // FRM_RELEASE_EXE_HPP