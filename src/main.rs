extern crate signal_hook;

use signal_hook::consts::signal::*;
use signal_hook::iterator::Signals;
use std::convert::TryFrom;

mod conf;
mod ftr;
mod log;
mod server;

fn main() {
    let mut prefix = String::from("/home/jonathan/dev/ftrd/");
    let mut conf_file_opt = String::new();
    let mut prefix_path_opt = String::new();
    let mut run_daemon_opt = false;
    let mut log_stderr = false;

    parse_opts(
        &mut conf_file_opt,
        &mut prefix_path_opt,
        &mut run_daemon_opt,
    );

    // user provided a specific prefix on the command line
    if prefix_path_opt.len() > 0 {
        prefix = prefix_path_opt;
    }

    // make sure the prefix path ends with "/"
    if !prefix.ends_with("/") {
        prefix.push('/');
    }

    let mut conf_file_loc = prefix.clone() + ftr::DEFAULT_CONF;

    // user provided a specific conf file on the command line
    if conf_file_opt.len() > 0 {
        conf_file_loc = conf_file_opt;
    }

    if run_daemon_opt {
        println!("Running as daemon....");
        std::process::exit(0);
    }

    loop {
        // TODO: create server object (this might have to be global)
        let mut conf = ftr::Conf::new();

        // load and test the configuration file
        if let Err(err) = conf.load(&conf_file_loc) {
            eprintln!("server configuration error: {}", err);
            std::process::exit(1);
        }

        // create logging object
        let mut log = match ftr::Log::new(&prefix, &conf, true) {
            Ok(log) => log,
            Err(err) => {
                eprintln!("error starting the logger: {}", err);
                std::process::exit(1);
            }
        };

        let mut server = ftr::Server::new(conf, log);
        if let Err(err) = server.start() {
            eprintln!("error starting the server: {}", err);
            std::process::exit(1);
        }

        // log.log_acc(&format!("prefix: {}", prefix));
        // log.log_acc(&format!("conf file: {}", conf_file_loc));
        break;
    }
}

// TODO: update this function, parsing cmd line options like this is terrible
// take a look at getopts: https://crates.io/crates/getopts
fn parse_opts(conf_file_opt: &mut String, prefix_path_opt: &mut String, run_daemon_opt: &mut bool) {
    let mut inp = false;
    let mut inc = false;

    for arg in std::env::args().skip(1) {
        let opt = arg.trim_start_matches('-');

        if opt == "h" {
            print_usage();
        } else if opt == "v" {
            print_version();
        } else if opt == "t" {
            test_conf();
        } else if opt == "d" {
            *run_daemon_opt = true;
        } else if opt == "p" {
            inp = true;
        } else if opt == "c" {
            inc = true;
        } else if inp == true {
            *prefix_path_opt = arg;
            inp = false;
        } else if inc == true {
            *conf_file_opt = arg;
            inc = false;
        } else {
            println!("unknown option {}, please try again", arg);
            std::process::exit(1);
        }
    }
}

fn handle_signals(server: Arc<ftr::Server>) -> Result<(), std::io::Error> {
    let mut signals = Signals::new(&[SIGTERM, SIGINT, SIGQUIT, SIGHUP])?;

    // TODO: do something with the handle of the signal

    std::thread::spawn(move || {
        for signal in signals.forever() {
            match signal {
                SIGTERM | SIGINT | SIGQUIT => {
                    server.shutdown();
                }
                SIGHUP => {
                    server.reload();
                }
                _ => {
                    panic!("unknown signal catched: {}", signal);
                }
            }
        }
    });

    Ok(())
}

fn test_conf() {
    println!("Testing the configuration file...Done");
    std::process::exit(0);
}

fn print_usage() {
    println!("Usage: ftr -[htv] [-p prefix] [-c conf]");
    println!("");
    println!("Options:");
    println!("  -h\t\t: This help menu");
    println!("  -v\t\t: Show server version and exit");
    println!("  -t\t\t: Test the configuration file and exit");
    println!("  -d\t\t: Run the server in the background (as a daemon)");
    println!("  -p prefix\t: Set the path of the prefix");
    println!("  -c filename\t: Use the specified configuration file");
    std::process::exit(0);
}

fn print_version() {
    println!("ftr version v{}", ftr::VERSION);
    std::process::exit(0);
}
