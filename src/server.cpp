/*
 * Author: Elijah Mullens
 * Date: 09/07/2024
 */

#include "logserver.hpp"

/**
 * ensures that file is not STDOUT before closing, otherwise leaving untouched
 *
 * parameters:
 * - file (FILE *): file to be checked then closed
 */
void close_file(fd_t file) {
  if (STDOUT != file) {
    close(file);
  }
}

Logger::Logger(std::string name, port_t port) {
  if (name == "stdout") {
    file = STDOUT;
  } else {
    if ((file = open(name.c_str(), O_WRONLY | O_APPEND | O_CREAT,
                     S_IRUSR | S_IWUSR)) < 0) {
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
    close_file(file);
    exit(EXIT_FAILURE);
  }

  std::string header("\
-------------------------------------------------------------------------------\n\
                                  New Log\n\
-------------------------------------------------------------------------------\n\
");
  pushQueue(std::make_pair(HEADER, header));
  std::thread qp_thread(&Logger::processQueue, this);

  this->start();
}

void Logger::start() {
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

    this->pushQueue(std::make_pair(level, msg));
  }
}

void Logger::processQueue() {
  while (true) {
    while (logqueue.empty()) {
    }

    std::pair<uint8_t, std::string> log = this->popQueue();
    this->commitLog(std::move(log.first), std::move(log.second));
  }
}

Logger::~Logger() {
  close_file(file);
  close(sock);
}

/**
 * threadsafe method to add an element to logqueue
 */
void Logger::pushQueue(std::pair<uint8_t, std::string> log) {
  logqueuelock.lock();

  logqueue.push(std::move(log));

  logqueuelock.unlock();
}

/**
 * threadsafe method to extract the first element from the queue, then
 * remove it from logqueue..
 */
std::pair<uint8_t, std::string> Logger::popQueue() {
  logqueuelock.lock();

  std::pair<uint8_t, std::string> _ret = logqueue.front();
  logqueue.pop();

  logqueuelock.unlock();

  return _ret;
}

void Logger::commitLog(uint8_t level, std::string message) {
  auto apply = [&message](std::string colour) {
    message = colour + message + "\e[0m";
  };

  switch (level) {
  case HEADER: {
    if (file == STDOUT) {
      apply("\e[0m");
    }
    break;
  }
  case INFO: {
    message = "Info: " + message;
    if (file == STDOUT) {
      apply("\e[0;36m");
    }
    break;
  }
  case DEBUG: {
    message = "Debug: " + message;
    if (file == STDOUT) {
      apply("\e[0;93m");
    }
    break;
  }
  case ERROR: {
    message = "Error: " + message;
    if (file == STDOUT) {
      apply("\e[0;91m");
    }
    break;
  }
  default: {
    printf("invalid log level when commiting log");
    exit(EXIT_FAILURE);
  }
  }

  if (file == STDOUT) {
    message += "\e[0m";
  }

  if (message[message.size() - 1] != '\n') {
    message += "\n";
  }
  char const *msg = message.c_str();
  write(file, msg, message.size());
}
