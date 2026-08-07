#ifndef FRM_RELEASE_INUSE_HPP
#define FRM_RELEASE_INUSE_HPP

#include "logger.hpp"
#include "trie.hpp"

void ReleaseInUseHandles(Trie &files, Logger &logger);

#endif // FRM_RELEASE_INUSE_HPP