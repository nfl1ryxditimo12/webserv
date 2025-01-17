#include "CgiHandler.hpp"

#include <cstdlib>
#include <cstring>

#include "Socket.hpp"

extern char** environ;

ws::CgiHandler::CgiHandler() throw() : _eof(true) {
  _fpipe[0] = -1;
  _fpipe[1] = -1;
  _bpipe[0] = -1;
  _bpipe[1] = -1;
}

ws::CgiHandler::CgiHandler(const CgiHandler& other) {
  _fpipe[0] = other._fpipe[0];
  _fpipe[1] = other._fpipe[1];
  _bpipe[0] = other._bpipe[0];
  _bpipe[1] = other._bpipe[1];
  _eof = other._eof;
}

ws::CgiHandler::~CgiHandler() {
  close(_fpipe[0]);
  close(_fpipe[1]);
  close(_bpipe[0]);
  close(_bpipe[1]);
}

bool ws::CgiHandler::set_cgi_env(const char* method, const char* path) {
  return !(
    setenv("SERVER_PROTOCOL", "HTTP/1.1", 1)
    || setenv("REQUEST_METHOD", method, 1)
    || setenv("PATH_INFO", path, 1)
  );
}

bool ws::CgiHandler::init_pipe() throw() {
  return (!
    (
      pipe(_fpipe)
      || pipe(_bpipe)
      || fcntl(_fpipe[1], F_SETFL, O_NONBLOCK) == -1
      || fcntl(_bpipe[0], F_SETFL, O_NONBLOCK) == -1
    )
  );
}

pid_t ws::CgiHandler::init_child(const char* cgi_path, pid_t& pid) {
  pid = fork();

  if (!pid) {
    int fatal = 0;

    close(_fpipe[1]);
    fatal |= dup2(_fpipe[0], STDIN_FILENO) == -1;
    close(_fpipe[0]);

    close(_bpipe[0]);
    fatal |= dup2(_bpipe[1], STDOUT_FILENO) == -1;
    close(_bpipe[1]);

    char *argv[2];
    argv[0] = strdup(cgi_path);
    argv[1] = NULL;

    if (fatal)
      exit(EXIT_FAILURE);

    execve(cgi_path, argv, environ);

    exit(EXIT_FAILURE);
  }

  close(_fpipe[0]);
  close(_bpipe[1]);

  return pid;
}

const int* ws::CgiHandler::get_fpipe() const throw() {
  return _fpipe;
}

const int* ws::CgiHandler::get_bpipe() const throw() {
  return _bpipe;
}

int ws::CgiHandler::get_eof() const throw() {
  return _eof;
}

void ws::CgiHandler::set_eof(bool value) {
  _eof = value;
}

pid_t ws::CgiHandler::run_cgi(const char *method, const char *path_info, const char *cgi_path, ws::Kernel& kernel) {
  pid_t pid = -1;

  if (!init_pipe() || !set_cgi_env(method, path_info) || init_child(cgi_path, pid) == -1)
    return pid;

  _eof = false;

  kernel.add_write_event(_fpipe[1], reinterpret_cast<void*>(Socket::write_pipe));
  kernel.add_read_event(_bpipe[0], reinterpret_cast<void*>(ws::Socket::read_pipe));

  return pid;
}
