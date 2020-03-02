#include <signal.h>
#include <zinit.h>
#include <zzabbix.h>

namespace zworker {
enum Menu { MAIN, INFO, MAINTENANCE, MAINTENANCESELECTHOSTGRP, ACTION };
int workerBot(sigset_t& sigset, siginfo_t& siginfo);
int workerSender(sigset_t& sigset, siginfo_t& siginfo);
void botGetParams(ZConfig& tc, ZConfig& zc, zbot::config& c);
TgBot::InlineKeyboardMarkup::Ptr createMenu(Menu menu, ZZabbix& zabbix, int page = 0);
std::vector<TgBot::InlineKeyboardButton::Ptr> createPaginator(int curr, int max,
                                                              std::string callbackData);
void addList(TgBot::InlineKeyboardMarkup::Ptr markup, std::string callbackName, ZZabbix* zabbix,
             std::vector<std::pair<std::string, std::string>> (ZZabbix::*fp)(int), int page,
             int lineperpage = 5);
void addButton(std::vector<TgBot::InlineKeyboardButton::Ptr>& row, std::string text, std::string callbackData);
void senderGetParams(ZConfig& tc, zbot::config& c);
} // namespace zworker