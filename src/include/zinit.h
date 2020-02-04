#ifndef ZINIT_H
#define ZINIT_H

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

void init();
int zfork();
int zmonitor();
int workerBot();
int workerSender();
int zwait(int& pid, int& start, siginfo_t& siginfo);
int startWorker(int& pid, int& status, int& start, int (*func)());
void setPidFile(std::string& f);
void setProcName(const char* procname);
enum ChildSignal { CHILD_TERMINATE = 80, CHILD_RESTART };
} // namespace zbot
#endif // ZINIT_H