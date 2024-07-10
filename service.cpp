/*
 * Author: Elijah Mullens
 * Date: 09/07/2024
 *
 * [Description]
 * To provide logging functionality to a multiprocess program. This will be
 * performed by exposing a TCP socket to accept and process messages without
 * corrupting the log.
 *
 * [Format]
 * ./logger [name | stdout] [port]
 *
 * [Specification]
 * - name :- The name of the file to which the log should be output
 * - port :- The port which will be accepting log requests
 *
 * [Warnings]
 * This program is built to terminate upon any undefined situation, so it is
 * critical that the the process is called properlly and that the port is
 * available.
 *
 * [Use]
 *   [Log Format]
 *   <level>:<message>
 *   - level   :- one of INFO, DEBUG, ERROR (will determine colour in stdout
 * [blue, yellow, red])
 *   - message :- the arbitrary message to be printed
 */

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "loglevel.hpp"

#define port_t uint16_t
#define fd_t ssize_t

void close_file(FILE *file) {
  if (stdout != file) {
    fclose(file);
  }
}

class Logger {
public:
  Logger(std::string name, port_t port) {
    if (name == "stdout") {
      file = stdout;
    } else {
      if ((file = fopen(name.c_str(), "a")) == NULL) {
        perror("couldn't create file in append mode");
        exit(EXIT_FAILURE);
      }
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (errno != 0) {
      perror("couldn't create socket");
      close_file(file);
      exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                   sizeof(opt)) < 0) {
      perror("couldn't set sock opts");
      close(sock);
      close_file(file);
      exit(EXIT_FAILURE);
    }

    addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      perror("failed to bind socket");
      close(sock);
      close_file(file);
      exit(EXIT_FAILURE);
    }

    if (listen(sock, SOMAXCONN) < 0) {
      perror("failed to listen");
      close(sock);
      fclose(file);
      exit(EXIT_FAILURE);
    }

    std::string header("\
-------------------------------------------------------------------------------\n\
                                  New Log\n\
-------------------------------------------------------------------------------\n\
");
    pushQueue(std::make_pair(HEADER, header));
    std::thread qp_thread(&Logger::processQueue, this);

    start();
  }

  void start() {
    while (true) {
      socklen_t len = sizeof(addr);
      fd_t msg_d = accept(sock, (struct sockaddr *)&addr, &len);
      if (errno != 0) {
        perror("couldn't accept message");
        close(sock);
        close_file(file);
        exit(EXIT_FAILURE);
      }

      bool first_read = true;
      std::string msg = "";
      uint8_t level;
      char buf[LINE_MAX] = {0};
      ssize_t bytes;
      while ((bytes = read(msg_d, buf, LINE_MAX - 1)) > 0) {
        if (bytes < 1) {
          break;
        }
        size_t buf_start = 0;
        if (first_read) {
          for (; buf_start < LINE_MAX && buf[buf_start] != ':'; buf_start++)
            ;
          if (buf_start == LINE_MAX) {
            printf("invalid message");
            exit(EXIT_FAILURE);
          }

          buf[buf_start] = '\0';
          long l = strtol(buf, NULL, 10);
          assert(errno == 0);
          assert(l >= 0 && l <= 3);

          level = l;
          buf_start++;
          first_read = false;
        }

        msg += std::string(&(buf[buf_start]));
        memset(buf, 0, LINE_MAX);
      }
      close(msg_d);

      pushQueue(std::make_pair(level, msg));
    }
  }

  void processQueue() {
    while (true) {
      while (logqueue.empty()) {
      }

      std::pair<uint8_t, std::string> log = popQueue();
      commitLog(std::move(log.first), std::move(log.second));
    }
  }

  ~Logger() {
    close_file(file);
    close(sock);
  }

private:
  FILE *file;
  fd_t sock;
  struct sockaddr_in addr;
  std::queue<std::pair<uint8_t, std::string>> logqueue;
  std::mutex logqueuelock;

  /**
   * threadsafe method to add an element to logqueue
   */
  void pushQueue(std::pair<uint8_t, std::string> log) {
    logqueuelock.lock();

    logqueue.push(std::move(log));

    logqueuelock.unlock();
  }

  /**
   * threadsafe method to extract the first element from the queue, then
   * remove it from logqueue..
   */
  std::pair<uint8_t, std::string> popQueue() {
    logqueuelock.lock();

    std::pair<uint8_t, std::string> _ret = logqueue.front();
    logqueue.pop();

    logqueuelock.unlock();

    return std::move(_ret);
  }

  void commitLog(uint8_t level, std::string message) {
    auto apply = [&message](std::string colour) {
      message = colour + message + "\e[0m";
    };

    switch (level) {
    case HEADER: {
      if (file == stdout) {
        apply("\e[0m");
      }
      break;
    }
    case INFO: {
      message = "Info: " + message;
      if (file == stdout) {
        apply("\e[0;36m");
      }
      break;
    }
    case DEBUG: {
      message = "Debug: " + message;
      if (file == stdout) {
        apply("\e[0;93m");
      }
      break;
    }
    case ERROR: {
      message = "Error: " + message;
      if (file == stdout) {
        apply("\e[0;91m");
      }
      break;
    }
    default: {
      printf("invalid log level when commiting log");
      exit(EXIT_FAILURE);
    }
    }

    if (file == stdout) {
      message += "\e[0m";
    }

    char const *msg = message.c_str();
    fwrite(msg, sizeof(char), message.size(), file);
  }
};

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
