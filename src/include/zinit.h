#ifndef ZINIT_H
#define ZINIT_H

#include <defaultconfig.h>
#include <iostream>
#include <set>
#include <zconfig.h>
#include <zlogger.h>
#include <zmsgbox.h>
#include <ztbot.h>

namespace zbot {
extern ZLogger log;
extern ZConfig mainConfig;
extern char** progName;
extern int argc, fd[2];
enum ChildSignal { CHILD_TERMINATE = 80, CHILD_RESTART };
struct config {
  std::set<std::string> adminUsers, notifyChats;
  std::string path, token, webhookPublicHost, webhookPath;
  std::string zabbixUrl, zabbixUser, zabbixPassword;
  int maxmessages, minapprox, wait, webhookBindPort;
  float accuracy, spread;
  bool webhook, dont_approximate_multibyte;
};

void init();
int zfork();
int zmonitor();
int zwait(int& pid, int& start, siginfo_t& siginfo);
int startWorker(int& pid, int& status, int& start,
                int (*func)(sigset_t& sigset, siginfo_t& siginfo));
void setPidFile(std::string& f);
void setProcName(const char* procname);
void signal_error(int sig, siginfo_t* si, void* ptr);
} // namespace zbot
#endif // ZINIT_H