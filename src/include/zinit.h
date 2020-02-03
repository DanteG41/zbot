#ifndef ZINIT_H
#define ZINIT_H

#include <iostream>
#include <unistd.h>
#include <zconfig.h>
#include <zlogger.h>
#include <zmsgbox.h>
#include <signal.h>

namespace zbot {
extern ZLogger log;
extern ZConfig mainConfig;
void init();
int zfork();
int zmonitor();
int workerBot();
int workerSender();
int zwait(int& pid, int& start, siginfo_t& siginfo);
int startWorker(int& pid, int& status, int& start, int (*func)());
void setPidFile(std::string& f);
enum ChildSignal { CHILD_TERMINATE = 80, CHILD_RESTART };
} // namespace zbot
#endif // ZINIT_H