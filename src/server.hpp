#ifndef ftr_server_hpp
#define ftr_server_hpp

#include "conf.hpp"
#include "constants.hpp"
#include "session.hpp"
#include <arpa/inet.h>
#include <map>
#include <memory>
#include <netdb.h>
#include <string>
#include <sys/types.h>

namespace ftr {

class Server {
  public:
    Server()
        : conf{nullptr}, port{0}, ctrl_listener_fd{0}, is_reloading{false},
          is_shutting_down{false} {};
    ~Server() = default;

    Server(const Server &server) = delete;
    Server(Server &&server) = delete;
    Server &operator=(const Server &server) = delete;
    Server &operator=(Server &&server) = delete;

    // TODO: move these 2 constants to the constants.hpp header file
    static const int BACKLOG = 4096;
    static constexpr char const *DEFAULT_CONF = "ftr.conf";

    void start(std::unique_ptr<ftr::Conf> &conf);
    void shutdown();
    void reload_conf();
    void send_response(int conn_fd, int status_code, std::string extra_msg);
    std::string get_status_code_msg(int status_code);
    std::string get_command_help_msg(std::string cmd);
    std::string get_all_commands_help_msg();

  private:
    std::string host;
    std::unique_ptr<ftr::Conf> conf;
    std::map<int, std::shared_ptr<ftr::Session>> sessions;
    int port;
    int ctrl_listener_fd;
    bool is_reloading;
    bool is_shutting_down;

    enum class HostType {
        DomainName,
        IPv4,
        IPv6,
        Invalid,
    };

    int get_server_ctrl_listener();
    HostType validate_server_host();
    std::string get_addr_string(struct sockaddr *addr);
    int bind_address(const struct addrinfo *addr_info);
    void handle_conn(int conn_fd);
};

} // namespace ftr
#endif
