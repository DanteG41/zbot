#ifndef ZINIT_H
#define ZINIT_H

#include <defaultconfig.h>
#include <iostream>
#include <set>
#include <signal.h>
#include <unistd.h>
#include <zconfig.h>
#include <zlogger.h>
#include <zmsgbox.h>
#include <ztbot.h>

namespace zbot {
extern ZLogger log;
extern ZConfig mainConfig;
extern char** progName;
extern int argc;
enum ChildSignal { CHILD_TERMINATE = 80, CHILD_RESTART };
enum Menu { MAIN, INFO, MAINTENANCE, ACTION };
struct config {
  std::set<std::string> adminUsers;
  std::string path, token, zabbixServer, webhookPublicHost, webhookPath;
  int maxmessages, minapprox, wait, webhookBindPort;
  float accuracy, spread;
  bool webhook;
};

void init();
int zfork();
int zmonitor();
int workerBot(sigset_t& sigset, siginfo_t& siginfo);
int workerSender(sigset_t& sigset, siginfo_t& siginfo);
int zwait(int& pid, int& start, siginfo_t& siginfo);
int startWorker(int& pid, int& status, int& start,
                int (*func)(sigset_t& sigset, siginfo_t& siginfo));
void setPidFile(std::string& f);
void setProcName(const char* procname);
void botGetParams(ZConfig& tc, ZConfig& zc, config& c);
TgBot::InlineKeyboardMarkup::Ptr createMenu(Menu menu);
void senderGetParams(ZConfig& tc, config& c);
void signal_error(int sig, siginfo_t* si, void* ptr);
} // namespace zbot
#endif // ZINIT_H