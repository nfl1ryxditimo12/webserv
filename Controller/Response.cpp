#include "Response.hpp"

#include <unistd.h>

#include "Define.hpp"


ws::Response::Response() {}

ws::Response::~Response() {}

void ws::Response::set_data(ws::Socket* socket, client_value_type& client_data, uintptr_t client_fd) {
  _socket = socket;
  _repo = &client_data.repository;
  _client_fd = client_fd;
}

void ws::Response::set_kernel(Kernel *kernel) {
  _kernel = kernel;
}

// todo: can remove client_data arg
void ws::Response::process(ws::Socket* socket, client_value_type& client_data, uintptr_t client_fd) {
  set_data(socket, client_data, client_fd);

  ws::Repository::redirect_type redirect = client_data.repository.get_redirect();

  if (client_data.status >= BAD_REQUEST)
    _kernel->kevent_ctl(client_fd, EVFILT_USER, EV_ADD, NOTE_TRIGGER, 0, reinterpret_cast<void*>(ws::Socket::read_data), NULL);
  else if (redirect.first > 0) {
    /*
      redirect.second 값을 어디에 세팅하나
      300 이상: header 설정 후 send
      300 미만: body 설정 후 send
    */
    if (redirect.first < 300)
      client_data.response = redirect.second;
    _kernel->kevent_ctl(client_fd, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, reinterpret_cast<void*>(ws::Socket::generate_response), NULL);
  }
  else {
    if (client_data.repository.get_method() == "DELETE") {
      remove(client_data.repository.get_file_path().c_str());
      _kernel->kevent_ctl(client_fd, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, reinterpret_cast<void*>(ws::Socket::generate_response), NULL);
    }
    else {
      if (client_data.repository.get_cgi().first != "") {
        (void)redirect;
        /* 알아서 cgi 처리해야함 */
      }
      else if (!client_data.repository.get_autoindex().empty()) {
        ws::Repository::autoindex_type autoindex = client_data.repository.get_autoindex();
        std::string response;

        response += "<html>\n<head>\n</head>\n<body>\n<ul>\n";
        for (ws::Repository::autoindex_type::iterator it = autoindex.begin(); it != autoindex.end(); ++it) {
          response += ("<li><a href=\"" + *it +"\">" + *it +"</a></li>\n");
        }
        response += "</ul>\n</body>\n</html>";
        client_data.response = response;
        _kernel->kevent_ctl(client_fd, EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, reinterpret_cast<void*>(ws::Socket::generate_response), NULL);
      }
      else {
        ws::Socket::kevent_func func = client_data.repository.get_method() == "GET" ? ws::Socket::read_data : ws::Socket::write_data;
        _kernel->kevent_ctl(client_fd, EVFILT_USER, EV_ADD, NOTE_TRIGGER, 0, reinterpret_cast<void*>(func), NULL);
      }
    }
  }

//  if (!_repo.get_autoindex().empty()) {
//    client_data.response.first = this->generate_directory_list();
//    _kernel->kevent_ctl(client_fd, EVFILT_WRITE, EV_ADD, 0, 0, reinterpret_cast<void*>(&ws::Socket::send_response));
//  }
}

void ws::Response::generate(ws::Socket *socket, ws::Response::client_value_type &client_data, uintptr_t client_fd) {
  set_data(socket, client_data, client_fd);

  std::string& response_data = client_data.response;
  std::string response_header = ws::HeaderGenerator::generate(client_data.status, response_data.length());
  if (client_data.status < 400 && client_data.status >= 300 && client_data.repository.get_redirect().first > 0)
    response_header += ("Location: " + client_data.repository.get_redirect().second + "\r\n");
  response_data = response_header + "\r\n" + response_data;
}

std::string ws::Response::generate_directory_list() const {
  std::string body = this->generate_directory_list_body();
  return ws::HeaderGenerator::generate(301, body.length()) + "\r\n" + body; // todo status
}

std::string ws::Response::generate_directory_list_body() const {
  return "https://www.naver.com"; // todo html directory list page
}
