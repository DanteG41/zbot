#ifndef DEFAULTCONFIG_H
#define DEFAULTCONFIG_H
#include <map>
#include <string>

namespace defaultconfig {
std::map<std::string, std::string> params         = {{"storage", "/var/spool/zbot"},
                                             {"log_file", "/var/log/zbot.log"},
                                             {"pid_file", "/var/run/zbot.pid"},
                                             {"wait", "10"}};
std::map<std::string, std::string> zabbixParams   = {{"zabbix_server", "127.0.0.1"}};
std::map<std::string, std::string> telegramParams = {{"token", "*token*"}};
} // namespace defaultconfig
#endif // DEFAULTCONFIG_H