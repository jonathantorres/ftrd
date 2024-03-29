#include "server.hpp"
#include "conf.hpp"
#include "exception.hpp"
#include "log.hpp"
#include "string.hpp"
#include "util.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <random>
#include <regex>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace ftr;

void Server::start(const std::shared_ptr<ftr::Conf> conf,
                   const std::shared_ptr<ftr::Log> log) {
    m_log = log;
    m_conf = conf;
    m_host = m_conf->get_server_name();
    m_port = m_conf->get_port();

    m_log->log_acc("Server starting...");

    m_ctrl_listener_fd = get_server_ctrl_listener();

    if (m_ctrl_listener_fd < 0) {
        throw ServerError("The hostname could not be resolved");
    }

    int res = listen(m_ctrl_listener_fd, ftr::BACKLOG);

    if (res < 0) {
        throw ServerError(std::strerror(errno));
    }

    // save the resolved address
    struct sockaddr_storage addr_stor = {};
    struct sockaddr *addr = reinterpret_cast<struct sockaddr *>(&addr_stor);
    socklen_t addr_size = sizeof(addr_stor);
    int name_res = getsockname(m_ctrl_listener_fd, addr, &addr_size);

    if (name_res == 0) {
        m_resolved_host = get_addr_string(addr);
    }

    m_log->log_acc("Server started OK.");

    while (true) {
        int conn_fd = accept(m_ctrl_listener_fd, nullptr, nullptr);

        if (conn_fd < 0) {
            if (m_is_shutting_down) {
                // the server is shutting down
                break;
            }

            m_log->log_err("accept error: ", std::strerror(errno));

            if (errno == EINTR) {
                // interrupted system call, let's try again
                continue;
            }

            break;
        }

        // create session id
        std::random_device rd;
        std::uniform_int_distribution<int> dist(1, 1000000);
        int id = dist(rd);

        // handle the client
        std::thread client_thrd(&Server::handle_conn, this, conn_fd, id);
        m_session_threads[id] = std::move(client_thrd);
    }
}

void Server::handle_conn(const int conn_fd, const int session_id) {
    // send welcome message
    send_response(conn_fd, ftr::STATUS_CODE_SERVICE_READY, "");

    // TODO: could this be made better
    // not sure if using *this over here is the right thing to do
    std::shared_ptr s = std::make_shared<Session>(conn_fd, *this, m_log);

    m_sessions[session_id] = s;
    s->start();

    // client finished normally (not from a shutdown)
    // remove this session from the map of sessions
    if (!m_is_shutting_down) {
        m_sessions.erase(session_id);

        // extract and cleanup thread object
        auto node_handle = m_session_threads.extract(session_id);

        if (!node_handle.empty()) {
            if (node_handle.mapped().joinable()) {
                node_handle.mapped().detach();
            }
        }
    }
}

void Server::shutdown() {
    m_log->log_acc("Shutting down server...");
    m_is_shutting_down = true;

    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        std::shared_ptr<ftr::Session> sess = it->second;
        sess->quit();
    }

    for (auto it = m_session_threads.begin(); it != m_session_threads.end();
         ++it) {
        if (it->second.joinable()) {
            it->second.join();
        }
    }

    if (m_ctrl_listener_fd > 0) {
        int res = close(m_ctrl_listener_fd);

        if (res < 0) {
            m_log->log_err("error closing main server listener ",
                           std::strerror(errno));
        }
    }

    m_log->log_acc("Server shutdown complete");
}

void Server::reload(const std::string &prefix) {
    m_log->log_acc("Reloading the configuration file...");
    auto conf = std::make_shared<ftr::Conf>();

    try {
        conf->load(prefix + ftr::DEFAULT_CONF);
    } catch (std::exception &e) {
        m_log->log_err("server configuration error: ", e.what());
        return;
    }

    m_log->log_acc("Configuration file OK");
    m_is_reloading = true;
    shutdown();
}

void Server::send_response(const int conn_fd, const int status_code,
                           const std::string &extra_msg) {
    std::stringstream resp_msg;
    std::string code_msg = get_status_code_msg(status_code);
    resp_msg << status_code << ' ';
    resp_msg << code_msg;

    if (extra_msg != "") {
        resp_msg << ' ' << extra_msg;
    }

    resp_msg << '\n';

    std::string msg_str = resp_msg.str();

    m_log->log_acc(ftr::trim_whitespace(msg_str));

    if (write(conn_fd, msg_str.c_str(), msg_str.size()) < 0) {
        m_log->log_err(std::strerror(errno));

        throw ServerError(std::strerror(errno));
    }
}

int Server::find_open_addr(bool use_ipv6) {
    sa_family_t fam = AF_INET;
    int fd = -1;

    if (use_ipv6) {
        fam = AF_INET6;
    }

    struct addrinfo *res, *res_begin = nullptr;
    struct addrinfo hints {
        AI_PASSIVE, fam, SOCK_STREAM, 0, 0, nullptr, nullptr, nullptr
    };

    int info_res = getaddrinfo(m_host.c_str(), "0", &hints, &res);

    if (info_res != 0) {
        freeaddrinfo(res);
        throw ServerError(gai_strerror(info_res));
    }

    res_begin = res;
    while (res != NULL) {
        fd = bind_address(res);

        if (fd > 0) {
            break;
        }
        res = res->ai_next;
    }

    freeaddrinfo(res_begin);

    if (fd < 0) {
        throw ServerError("An address to bind could not be found");
    }

    return fd;
}

std::string Server::get_status_code_msg(const int status_code) {
    auto sc = ftr::status_codes.find(status_code);

    if (sc != ftr::status_codes.end()) {
        return sc->second;
    }

    return "";
}

std::string Server::get_command_help_msg(const std::string &cmd) {
    auto msg = ftr::help_messages.find(cmd);

    if (msg != ftr::help_messages.end()) {
        return std::string(msg->second);
    }

    return "";
}

std::string Server::get_all_commands_help_msg() {
    std::stringstream buf;

    for (auto it = ftr::help_messages.begin(); it != ftr::help_messages.end();
         ++it) {
        buf << it->first;
        buf << ": ";
        buf << it->second;
        buf << " ";
    }

    return buf.str();
}

int Server::get_server_ctrl_listener() {
    Server::HostType server_host_type = validate_server_host();
    int fd = -1;

    if (server_host_type == Server::HostType::invalid) {
        throw ServerError("invalid host name");
    }

    struct addrinfo *res, *res_begin = nullptr;
    struct addrinfo hints {
        0, AF_UNSPEC, SOCK_STREAM, 0, 0, nullptr, nullptr, nullptr
    };

    int info_res = getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(),
                               &hints, &res);

    if (info_res != 0) {
        freeaddrinfo(res);
        throw ServerError(gai_strerror(info_res));
    }

    res_begin = res;
    while (res != NULL) {
        fd = bind_address(res);

        if (fd > 0) {
            break;
        }
        res = res->ai_next;
    }

    freeaddrinfo(res_begin);

    return fd;
}

int Server::bind_address(const struct addrinfo *addr_info) {
    if (!addr_info) {
        m_log->log_err("The addr_info pointer is NULL");

        return -1;
    }

    int fd = socket(addr_info->ai_family, addr_info->ai_socktype,
                    addr_info->ai_protocol);

    if (fd < 0) {
        m_log->log_err(std::strerror(errno));

        return -1;
    }

    int opt = 1;
    int res = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

    if (res < 0) {
        m_log->log_err(std::strerror(errno));
        close(fd);

        return -1;
    }

    res = bind(fd, addr_info->ai_addr, addr_info->ai_addrlen);

    if (res < 0) {
        m_log->log_err(std::strerror(errno));
        close(fd);

        return -1;
    }

    return fd;
}

std::string Server::get_addr_string(struct sockaddr *addr) {
    char buf[INET6_ADDRSTRLEN] = {0};

    if (!addr) {
        return std::string("nullptr");
    }

    if (addr->sa_family == AF_INET) {
        sockaddr_in *addr_in = reinterpret_cast<sockaddr_in *>(addr);
        if (inet_ntop(AF_INET, &addr_in->sin_addr, buf, INET6_ADDRSTRLEN) ==
            NULL) {
            return std::string(std::strerror(errno));
        }

        return std::string(buf);
    } else if (addr->sa_family == AF_INET6) {
        sockaddr_in6 *addr_in6 = reinterpret_cast<sockaddr_in6 *>(addr);
        if (inet_ntop(AF_INET6, &addr_in6->sin6_addr, buf, INET6_ADDRSTRLEN) ==
            NULL) {
            return std::string(std::strerror(errno));
        }

        return std::string(buf);
    }

    return std::string("unknown address family");
}

Server::HostType Server::validate_server_host() {
    if (ftr::is_ipv4(m_host)) {
        // host matches valid ipv4 address
        return Server::HostType::ipv4;
    } else if (ftr::is_ipv6(m_host)) {
        // host matches a valid ipv6 address
        return Server::HostType::ipv6;
    } else if (ftr::is_domain_name(m_host)) {
        // host matches valid domain name
        return Server::HostType::domain_name;
    } else if (m_host == "localhost") {
        // TODO: special case for localhost????
        return Server::HostType::domain_name;
    }

    return Server::HostType::invalid;
}
