#include <signal.h>
#include <zinit.h>
#include <zzabbix.h>

namespace zworker {
enum Menu {
  MAIN,
  INFO,
  MAINTENANCE,
  MAINTENANCESELECT,
  MAINTENANCESELECTHOSTGRP,
  ACTION,
  ACTIONENABLE,
  ACTIONDISABLE,
  SCREEN
};
int workerBot(sigset_t& sigset, siginfo_t& siginfo);
int workerSender(sigset_t& sigset, siginfo_t& siginfo);
void botGetParams(ZConfig& tc, ZConfig& zc, zbot::config& c);
TgBot::InlineKeyboardMarkup::Ptr createMenu(Menu menu, ZZabbix& zabbix, int page = 0,
                                            std::string callback = "", std::string chatid = "");
std::vector<TgBot::InlineKeyboardButton::Ptr> createPaginator(int curr, int max,
                                                              std::string callbackData);
void addList(TgBot::InlineKeyboardMarkup::Ptr markup, std::string callbackName, ZZabbix* zabbix,
             std::vector<std::pair<std::string, std::string>> data, int page, int lineperpage);
inline void addList(TgBot::InlineKeyboardMarkup::Ptr markup, std::string callbackName,
                    ZZabbix* zabbix,
                    std::vector<std::pair<std::string, std::string>> (ZZabbix::*fp)(int), int page,
                    int lineperpage = 5) {
  addList(markup, callbackName, zabbix, (zabbix->*fp)(100), page, lineperpage);
};
inline void addList(TgBot::InlineKeyboardMarkup::Ptr markup, std::string callbackName,
                    ZZabbix* zabbix,
                    std::vector<std::pair<std::string, std::string>> (ZZabbix::*fp)(int, int),
                    int arg, int page, int lineperpage = 5) {
  addList(markup, callbackName, zabbix, (zabbix->*fp)(arg, 100), page, lineperpage);
};

void addButton(std::vector<TgBot::InlineKeyboardButton::Ptr>& row, std::string text,
               std::string callbackData);
void senderGetParams(ZConfig& tc, zbot::config& c);
void removeFiles(std::vector<std::string> files);
} // namespace zworker