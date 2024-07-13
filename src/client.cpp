#include "logclient.hpp"

LogClient::LogClient(uint16_t port) : port(port) {
  addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
}

void LogClient::format_log(log_t level, std::string &log) {
  log = std::to_string(level) + ":" + log;
}

int LogClient::writeLog(log_t level, std::string log) {
  ssize_t fd = socket(AF_INET, SOCK_STREAM, 0);
  if (errno != 0) {
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    close(fd);
    return -1;
  }

  LogClient::format_log(level, log);
  if (write(fd, log.c_str(), log.size() + 1) < 0) {
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}
