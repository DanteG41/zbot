#ifndef DEFAULTCONFIG_H
#define DEFAULTCONFIG_H
#include <map>
#include <string>

namespace defaultconfig {
extern std::map<std::string, std::string> params;
extern std::map<std::string, std::string> zabbixParams;
extern std::map<std::string, std::string> telegramParams;
} // namespace defaultconfig
#endif // DEFAULTCONFIG_H