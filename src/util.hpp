#ifndef ftr_util_hpp
#define ftr_util_hpp

#include <string>

namespace ftr {

std::string trim_whitespace(const std::string &s);
std::string trim_right(const std::string &s);
std::string trim_left(const std::string &s);
bool is_ipv4(const std::string &s);
bool is_ipv6(const std::string &s);
bool is_domain_name(const std::string &s);

} // namespace ftr
#endif
