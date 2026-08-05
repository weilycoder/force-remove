#ifndef FRM_LOGGER_HPP
#define FRM_LOGGER_HPP

#include <cstdint>
#include <iostream>
#include <string>

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
  Logger(std::int32_t level, std::ostream &output_stream) : log_level(level), msg_flag(0), out(output_stream) {}

  void setLogLevel(std::int32_t level) { log_level = level; }
  void clearMsgFlag() { msg_flag = 0; }
  std::uint32_t getMsgFlag() const { return msg_flag; }

#define DEF_LOG(level)                                                                                                 \
  void level(const std::string &message) {                                                                             \
    msg_flag |= 1u << level##_v;                                                                                       \
    if (log_level <= level##_v) out << level##_s << message << std::endl;                                              \
  }

  DEF_LOG(debug)
  DEF_LOG(info)
  DEF_LOG(warning)
  DEF_LOG(error)

#undef DEF_LOG
};

#endif // FRM_LOGGER_HPP