#include "force-remove.hpp"

int main(int argc, char *argv[]) {
  Logger logger(Logger::debug_v);
  if (argc < 2) {
    logger.error("Usage: force-remove <path>");
    return -1;
  }
  std::string path = argv[1];
  ForceRemove(path, logger);
  return logger.getMsgFlag() & (1u << Logger::error_v) ? 1 : 0;
}