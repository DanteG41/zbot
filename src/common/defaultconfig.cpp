#include <defaultconfig.h>

std::map<std::string, std::string> defaultconfig::params         = {{"storage", "/var/spool/zbot"},
                                             {"log_file", "/var/log/zbot.log"},
                                             {"pid_file", "/var/run/zbot.pid"},
                                             {"wait", "10"},
                                             {"accuracy", "0.4"},
                                             {"spread", "0.1"},
                                             {"max_messages", "50"},
                                             {"min_approx", "5"}};
std::map<std::string, std::string> defaultconfig::zabbixParams   = {{"zabbix_server", "127.0.0.1"}};
std::map<std::string, std::string> defaultconfig::telegramParams = {{"token", "*token*"}};