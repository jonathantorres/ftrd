#include "file_data.hpp"
#include "ftrd.hpp"
#include "log.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <grp.h>
#include <iomanip>
#include <memory>
#include <pwd.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace ftr;

FileData::FileData(const std::filesystem::directory_entry &entry,
                   std::shared_ptr<ftr::Log> log)
    : m_dir_entry{entry}, m_owner_user_id{0}, m_owner_group_id{0}, m_log{log} {
    struct stat file_stat = {};

    if (stat(m_dir_entry.path().c_str(), &file_stat) < 0) {
        return;
    }

    m_owner_user_id = file_stat.st_uid;
    m_owner_group_id = file_stat.st_gid;
}

std::string FileData::get_file_line() {
    std::stringstream file_data;
    std::filesystem::file_status entry_status = m_dir_entry.status();
    std::filesystem::perms entry_perms = entry_status.permissions();

    file_data << get_permissions_line(m_dir_entry.is_directory(), entry_perms);
    file_data << ' ';
    file_data << get_owner_name();
    file_data << ' ';
    file_data << get_group_name();
    file_data << ' ';

    if (m_dir_entry.is_directory()) {
        file_data << ftr::DIR_SIZE << ' ';
    } else {
        file_data << m_dir_entry.file_size() << ' ';
    }

    // TODO: do a bit of more research on how to do this better
    auto sys_time =
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            m_dir_entry.last_write_time() -
            std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now());
    std::time_t cur_time = std::chrono::system_clock::to_time_t(sys_time);
    file_data << std::put_time(std::localtime(&cur_time), "%h %d %H:%M");
    file_data << ' ';
    file_data << m_dir_entry.path().filename().string();

    return file_data.str();
}

std::string FileData::get_owner_name() {
    std::string own_name;

    if (m_owner_user_id == 0) {
        return own_name;
    }

    struct passwd *pass_data = getpwuid(m_owner_user_id);

    if (pass_data == nullptr) {
        m_log->log_err("getpwuid error: ", std::strerror(errno));
        return own_name;
    }

    own_name = pass_data->pw_name;

    return own_name;
}

std::string FileData::get_group_name() {
    std::string grp_name;

    if (m_owner_group_id == 0) {
        return grp_name;
    }

    struct group *grp_data = getgrgid(m_owner_group_id);

    if (grp_data == nullptr) {
        m_log->log_err("getgrgid error: ", std::strerror(errno));
        return grp_name;
    }

    grp_name = grp_data->gr_name;

    return grp_name;
}

std::string
FileData::get_permissions_line(bool is_directory,
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
