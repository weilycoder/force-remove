#include "force-remove.hpp"

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 3) {
    std::cout << "Usage: forcedelete <path> [<log_level>]" << std::endl;
    return -1;
  }

  std::string path = argv[1];
  int log_level = Logger::info_v;

  if (argc == 3) {
    try {
      log_level = std::stoi(argv[2]);
      if (log_level < Logger::debug_v || log_level > Logger::error_v) throw std::out_of_range("");
    } catch (const std::invalid_argument &) {
      std::cerr << "Invalid log level: " << argv[2] << std::endl;
      return -1;
    } catch (const std::out_of_range &) {
      std::cerr << "Log level out of range: " << argv[2] << std::endl;
      return -1;
    }
  }

  Logger logger(log_level);
  ForceRemove(path, logger);
  return logger.getMsgFlag() & Logger::error_m ? 1 : 0;
}