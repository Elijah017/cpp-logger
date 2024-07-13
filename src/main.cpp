#include "logserver.hpp"

port_t get_port(char *port_string) {
  long port = strtol(port_string, NULL, 10);

  assert(errno == 0);
  assert(port >= 0 && port <= (1 << 16) - 1);

  return port;
}

Logger *logger;

void sig_handler(int s) { exit(EXIT_SUCCESS); }

int main(int argc, char **argv) {
  assert(argc == 3);

  std::string name(argv[1]);
  port_t port = get_port(argv[2]);

  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGSEGV, sig_handler);
  logger = new Logger(name, port);

  return 0;
}
