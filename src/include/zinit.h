#ifndef ZINIT_H
#define ZINIT_H

#include <defaultconfig.h>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <zconfig.h>
#include <zlogger.h>
#include <zmsgbox.h>

namespace zbot {
extern ZLogger log;
extern ZConfig mainConfig;
extern char** progName;
extern int argc;
enum ChildSignal { CHILD_TERMINATE = 80, CHILD_RESTART };
struct config {
  std::string path, token;
  int maxmessages, minapprox, wait;
  float accuracy, spread;
};

void init();
int zfork();
int zmonitor();
int workerBot();
int workerSender();
int zwait(int& pid, int& start, siginfo_t& siginfo);
int startWorker(int& pid, int& status, int& start, int (*func)());
void setPidFile(std::string& f);
void setProcName(const char* procname);
void senderGetParams(ZConfig& zc, config& c);
void signal_error(int sig, siginfo_t* si, void* ptr);
} // namespace zbot
#endif // ZINIT_H