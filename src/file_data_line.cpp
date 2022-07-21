#include "file_data_line.hpp"
#include "constants.hpp"
#include <filesystem>
#include <grp.h>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace ftr;

FileDataLine::FileDataLine(const std::filesystem::directory_entry &entry)
    : dir_entry{entry}, owner_user_id{0}, owner_group_id{0} {
    struct stat file_stat = {};

    if (stat(dir_entry.path().c_str(), &file_stat) < 0) {
        return;
    }

    owner_user_id = file_stat.st_uid;
    owner_group_id = file_stat.st_gid;
}

std::string FileDataLine::get_file_line() {
    std::stringstream file_data;
    std::filesystem::file_status entry_status = dir_entry.status();
    std::filesystem::perms entry_perms = entry_status.permissions();

    file_data << get_permissions_line(dir_entry.is_directory(), entry_perms);
    file_data << ' ';
    file_data << get_owner_name();
    file_data << ' ';
    file_data << get_group_name();
    file_data << ' ';

    if (dir_entry.is_directory()) {
        file_data << ftr::DIR_SIZE << ' ';
    } else {
        file_data << dir_entry.file_size() << ' ';
    }

    file_data << "Jul 10 19:07" << ' '; // TODO
    file_data << dir_entry.path().filename().string();

    return file_data.str();
}

std::string FileDataLine::get_owner_name() {
    std::string own_name;

    if (owner_user_id == 0) {
        return own_name;
    }

    struct passwd *pass_data = getpwuid(owner_user_id);

    if (pass_data == nullptr) {
        // TODO: Log this error
        return own_name;
    }

    own_name = pass_data->pw_name;

    return own_name;
}

std::string FileDataLine::get_group_name() {
    std::string grp_name;

    if (owner_group_id == 0) {
        return grp_name;
    }

    struct group *grp_data = getgrgid(owner_group_id);

    if (grp_data == nullptr) {
        // TODO: log this error
        return grp_name;
    }

    grp_name = grp_data->gr_name;

    return grp_name;
}

std::string
FileDataLine::get_permissions_line(bool is_directory,
                                   const std::filesystem::perms &entry_perms) {
    std::stringstream perms_data;

    if (is_directory) {
        perms_data << 'd';
    } else {
        perms_data << '-';
    }

    perms_data << ((entry_perms & std::filesystem::perms::owner_read) !=
                           std::filesystem::perms::none
                       ? 'r'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::owner_write) !=
                           std::filesystem::perms::none
                       ? 'w'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::owner_exec) !=
                           std::filesystem::perms::none
                       ? 'x'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::group_read) !=
                           std::filesystem::perms::none
                       ? 'r'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::group_write) !=
                           std::filesystem::perms::none
                       ? 'w'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::group_exec) !=
                           std::filesystem::perms::none
                       ? 'x'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::others_read) !=
                           std::filesystem::perms::none
                       ? 'r'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::others_write) !=
                           std::filesystem::perms::none
                       ? 'w'
                       : '-');

    perms_data << ((entry_perms & std::filesystem::perms::others_exec) !=
                           std::filesystem::perms::none
                       ? 'x'
                       : '-');

    return perms_data.str();
}

/* drwxr-xr-x  5 jonathan jonathan 4096 Jul 21 11:30 build */
/* -rw-r--r--  1 jonathan jonathan  106 Jun 11 16:17 .clang-format */
/* -rw-r--r--  1 jonathan jonathan  347 Jul 10 17:40 CMakeLists.txt */
