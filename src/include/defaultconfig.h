#include <map>
#include <string>

namespace defaultconfig {
std::map<std::string, std::string> params = {
    {"storage", "/var/spool/zbot"}, {"log", "/var/log/zbot.log"}, {"wait", "10"}};
std::map<std::string, std::string> zabbixParams   = {{"zabbix_server", "127.0.0.1"}};
std::map<std::string, std::string> telegramParams = {{"token", "*token*"}};
}