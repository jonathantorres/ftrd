#include "ftrd.hpp"
#include "command.hpp"
#include "conf.hpp"
#include "config.hpp"
#include "daemon.hpp"
#include "log.hpp"
#include "server.hpp"
#include "string.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <signal.h>
#include <string>
#include <sys/wait.h>

void parse_opts(int argc, const char **argv);
void print_usage();
void print_ftr_version();
void handle_signals();

const std::string VERSION(FTR_VERSION);
const std::string PROG_NAME(FTR_PROG_NAME);

std::string prefix(FTR_PREFIX);

// TODO: could there be a way that these are not used as globals?
std::shared_ptr<ftr::Log> serv_log = nullptr;
std::shared_ptr<ftr::Conf> conf = nullptr;
std::shared_ptr<ftr::Server> server = nullptr;
std::string conf_file_opt = "";
std::string prefix_path_opt = "";
bool run_daemon_opt = false;

int main(int argc, const char **argv) {
    parse_opts(argc, argv);

    bool log_stderr = true;

    // user provided a specific prefix on the command line
    if (prefix_path_opt.size() > 0) {
        prefix = prefix_path_opt;
    }

    // make sure the prefix path ends with "/"
    if (!ftr::ends_with(prefix, "/")) {
        prefix.append(1, '/');
    }

    std::string conf_file_loc = prefix + ftr::DEFAULT_CONF;

    // user provided a specific conf file on the command line
    if (conf_file_opt.size() > 0) {
        conf_file_loc = conf_file_opt;
    }

    // run as a daemon
    if (run_daemon_opt) {
        log_stderr = false;
        ftr::daemonize(PROG_NAME);

        if (ftr::daemon_is_running(PROG_NAME, prefix + PROG_NAME + ".pid")) {
            std::exit(EXIT_FAILURE);
        }
    }

    handle_signals();

    while (true) {
        conf = std::make_shared<ftr::Conf>();
        serv_log = std::make_shared<ftr::Log>();
        server = std::make_shared<ftr::Server>();

        try {
            // load and test the configuration file
            conf->load(conf_file_loc);
            serv_log->init(prefix, conf, log_stderr);
        } catch (std::exception &e) {
            std::cerr << "server configuration error: " << e.what() << '\n';
            std::exit(EXIT_FAILURE);
        }

        try {
            server->start(conf, serv_log);
        } catch (std::exception &e) {
            serv_log->log_err("server error: ", e.what());

            std::exit(EXIT_FAILURE);
        }

        conf.reset();
        serv_log.reset();

        if (server->is_reloading()) {
            server.reset();
            continue;
        }

        server.reset();

        break;
    }

    return 0;
}

void parse_opts(int argc, const char **argv) {
    ftr::Command c{argc, argv};
    bool print_version = false;
    bool print_help = false;
    bool test_conf = false;

    c.add_flag('v', print_version);
    c.add_flag('h', print_help);
    c.add_flag('t', test_conf);
    c.add_flag('d', run_daemon_opt);
    c.add_option('c', conf_file_opt);
    c.add_option('p', prefix_path_opt);
    c.parse();

    // user provided something we don't have an option for
    if (c.unknown_value_found()) {
        std::cerr << "Unknown option \"" << c.unknown_flag()
                  << "\" please try again\n";
        std::exit(EXIT_FAILURE);
    }

    // print version and exit
    if (print_version) {
        print_ftr_version();
    }

    // print help and exit
    if (print_help) {
        print_usage();
    }

    // test the configuration file and exit
    if (test_conf) {
        std::cerr << "Testing the configuration file...";
        conf = std::make_shared<ftr::Conf>();

        // make sure the prefix path ends with "/"
        if (!ftr::ends_with(prefix, "/")) {
            prefix.append(1, '/');
        }

        try {
            conf->load(prefix + ftr::DEFAULT_CONF);
            std::cerr << "OK.\n";
            conf.reset();
            std::exit(EXIT_SUCCESS);
        } catch (std::exception &e) {
            std::cerr << "Failed. ";
            std::cerr << e.what();
            std::cerr << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

void sig_handler(int signum) {
    switch (signum) {
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        server->shutdown();
        break;
    case SIGHUP:
        server->reload(prefix);
        break;
    }
}

void handle_signals() {
    struct sigaction act;
    act.sa_handler = sig_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    // server shutdown signals
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        // TODO: log the error
    }

    if (sigaction(SIGINT, &act, NULL) == -1) {
        // TODO: log the error
    }

    if (sigaction(SIGQUIT, &act, NULL) == -1) {
        // TODO: log the error
    }

    // server restart signal
    if (sigaction(SIGHUP, &act, NULL) == -1) {
        // TODO: log the error
    }
}

void print_usage() {
    std::cerr << "Usage: ftr -[htv] [-p prefix] [-c conf]\n";
    std::cerr << "\n";
    std::cerr << "Options:\n";
    std::cerr << "  -h\t\t: This help menu\n";
    std::cerr << "  -v\t\t: Show server version and exit\n";
    std::cerr << "  -t\t\t: Test the configuration file and exit\n";
    std::cerr << "  -d\t\t: Run the server in the background (as a daemon)\n";
    std::cerr << "  -p prefix\t: Set the path of the prefix\n";
    std::cerr << "  -c filename\t: Use the specified configuration file\n";
    std::exit(EXIT_SUCCESS);
}

void print_ftr_version() {
    std::cerr << "ftr version v" << VERSION << "\n";
    std::exit(EXIT_SUCCESS);
}
