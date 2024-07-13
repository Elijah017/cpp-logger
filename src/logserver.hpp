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
 * ./logger [name | STDOUT] [port]
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
 *   - level   :- one of INFO, DEBUG, ERROR (will determine colour in STDOUT
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
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

#include "loglevel.hpp"

#define STDOUT STDOUT_FILENO

#define port_t uint16_t
#define fd_t ssize_t

class Logger {
public:
  Logger(std::string name, port_t port);

  void start();

  void processQueue();

  ~Logger();

private:
  ssize_t file;
  fd_t sock;
  struct sockaddr_in addr;
  std::queue<std::pair<uint8_t, std::string>> logqueue;
  std::mutex logqueuelock;

  /**
   * threadsafe method to add an element to logqueue
   */
  void pushQueue(std::pair<uint8_t, std::string> log);

  /**
   * threadsafe method to extract the first element from the queue, then
   * remove it from logqueue..
   */
  std::pair<uint8_t, std::string> popQueue();

  /**
   * writes the log to the designated file
   */
  void commitLog(uint8_t level, std::string message);
};
