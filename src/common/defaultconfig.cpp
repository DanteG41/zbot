#include <defaultconfig.h>

std::map<std::string, std::string> defaultconfig::params       = {{"storage", "/var/spool/zbot"},
                                                            {"log_file", "/var/log/zbot.log"},
                                                            {"pid_file", "/var/run/zbot/zbotd.pid"},
                                                            {"wait", "10"},
                                                            {"accuracy", "0.4"},
                                                            {"spread", "0.1"},
                                                            {"max_messages", "50"},
                                                            {"min_approx", "5"},
                                                            {"bot_enable", "1"},
                                                            {"dont_approximate_multibyte", "0"},
                                                            {"immediate_send", "0"},
                                                            {"history_check_count", "20"},
                                                            {"history_max_age_minutes", "60"}};
std::map<std::string, std::string> defaultconfig::zabbixParams = {
    {"zabbix_url", "http://company.com/zabbix/"},
    {"user", "zbot"},
    {"password", "zbotpassword"}};
std::map<std::string, std::string> defaultconfig::telegramParams = {
    {"token", "*token*"},
    {"admin_users", ""},
    {"notify_chats", ""},
    {"webhook_enable", "0"},
    {"webhook_path", "/zbot/webhook?token=randomhash"},
    {"webhook_public_host", "zbot.org"},
    {"webhook_bind_port", "8080"}};