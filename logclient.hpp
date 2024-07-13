#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "loglevel.hpp"

class LogClient {
public:
  LogClient(uint16_t port);

  /**
   * @param level: LogLevel Enum describing the severity of the log
   * @param log: the message to be logged
   * @return:
   *   -  0: Succcess
   *   - -1: Failure
   */
  int writeLog(log_t level, std::string log);

private:
  uint16_t port;
  struct sockaddr_in addr;

  static void format_log(log_t level, std::string &log);
};
