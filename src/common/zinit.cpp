#include <chrono>
#include <execinfo.h>
#include <fstream>
#include <string>
#include <wait.h>
#include <zinit.h>
#include <zworker.h>

namespace zbot {
ZLogger log;
ZConfig mainConfig;
char** progName;
int argc;
} // namespace zbot

void zbot::init() {
  setlocale(LC_ALL, "");
  if (geteuid() == 0) {
    fprintf(stderr,
            "%s: cannot be run as root\n"
            "Please log in (using, e.g., \"su\") as the "
            "(unprivileged) user.\n",
            program_invocation_name);
    exit(EXIT_FAILURE);
  }
  auto t0      = std::chrono::high_resolution_clock::now();
  auto nanosec = t0.time_since_epoch();
  srand(nanosec.count());
}

void zbot::setPidFile(std::string& f) {
  std::ofstream pidFile;
  pidFile.open(f);
  if (!pidFile.is_open()) {
    zbot::log.write(ZLogger::LogLevel::ERROR, "[MONITOR] unable to create a pidfile " + f);
    exit(EXIT_FAILURE);
  }
  pidFile << getpid();
  pidFile.close();
}

void zbot::setProcName(const char* procname) {
  int limit = 16;
  char buff[limit];
  for (int i = 0; i < zbot::argc; i++) {
    for (int c = 0; zbot::progName[i][c] != '\0'; c++) {
      zbot::progName[i][c] = ' ';
    }
  }
  strncpy(buff, procname, limit - 1);
  strncat(buff, "\0", 1);
  strncpy(zbot::progName[0], buff, limit);
}

int zbot::startWorker(int& pid, int& status, int& start,
                      int (*func)(sigset_t& sigset, siginfo_t& siginfo)) {
  if (start) {
    pid = fork();
  }

  start = 0;

  if (pid < 0) {
    zbot::log.write(ZLogger::LogLevel::ERROR, "[MONITOR] Fork failed " + errno);
    return pid;
  }
  if (pid > 0) {
    return pid;
  }

  struct sigaction sigact;
  siginfo_t siginfo;
  sigset_t sigset;

  sigact.sa_flags     = SA_SIGINFO;
  sigact.sa_sigaction = zbot::signal_error;

  sigemptyset(&sigact.sa_mask);

  sigaction(SIGFPE, &sigact, 0);
  sigaction(SIGILL, &sigact, 0);
  sigaction(SIGSEGV, &sigact, 0);
  sigaction(SIGBUS, &sigact, 0);
  sigaction(SIGABRT, &sigact, 0);

  sigemptyset(&sigset);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGUSR1);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  status = func(sigset, siginfo);
  exit(status);
}

int zbot::zwait(int& pid, int& start, siginfo_t& siginfo) {
  int status;

  if (siginfo.si_signo == SIGCHLD) {
    waitpid(pid, &status, 0);
    if (WIFSIGNALED(status)) {
      zbot::log << "[MONITOR] Child terminate by signal " + std::to_string(WTERMSIG(status));
    }
    status = WEXITSTATUS(status);
    if (status == zbot::ChildSignal::CHILD_TERMINATE) {
      zbot::log << "[MONITOR] Child with pid " + std::to_string(pid) + " stopped";
      return zbot::ChildSignal::CHILD_TERMINATE;
    } else if (status == zbot::ChildSignal::CHILD_RESTART) {
      zbot::log << "[MONITOR] Child restart";
      start = 1;
    }
  }
  return 1;
}

int zbot::zmonitor() {
  int botPid, senderPid;
  int botStatus, senderStatus, status;
  int childStatus1    = 0;
  int childStatus2    = 0;
  int senderNeedStart = 1;
  int botNeedStart    = 1;
  bool botEnable;
  std::string pidFile;
  zbot::mainConfig.getParam("pid_file", pidFile);
  zbot::mainConfig.getParam("bot_enable", botEnable);

  zbot::setPidFile(pidFile);

  sigset_t sigset;
  siginfo_t siginfo;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGCHLD);
  sigaddset(&sigset, SIGUSR1);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  if (!botEnable) childStatus1 = zbot::ChildSignal::CHILD_TERMINATE;

  while (true) {
    if (botEnable) zbot::startWorker(botPid, botStatus, botNeedStart, zworker::workerBot);
    zbot::startWorker(senderPid, senderStatus, senderNeedStart, zworker::workerSender);
    sigwaitinfo(&sigset, &siginfo);
    if (siginfo.si_pid == botPid)
      childStatus1 = zbot::zwait(botPid, botNeedStart, siginfo);
    else if (siginfo.si_pid == senderPid)
      childStatus2 = zbot::zwait(senderPid, senderNeedStart, siginfo);
    else {
      if (siginfo.si_signo == SIGUSR1) // reload config
      {
        zbot::log << "[MONITOR] Signal " + std::to_string(siginfo.si_signo);
        zbot::log << "[MONITOR] reload config. ";
        mainConfig.load(defaultconfig::params);
        kill(botPid, SIGUSR1);
        kill(senderPid, SIGUSR1);
        botNeedStart    = 0;
        senderNeedStart = 0;
      } else { // other signal
        zbot::log << "[MONITOR] Signal " + std::string(strsignal(siginfo.si_signo));
        kill(botPid, SIGTERM);
        kill(senderPid, SIGTERM);
        return zbot::ChildSignal::CHILD_TERMINATE;
      }
    }
    if (childStatus1 == zbot::ChildSignal::CHILD_TERMINATE &&
        childStatus2 == zbot::ChildSignal::CHILD_TERMINATE) {
      break;
    }
    sleep(2);
  }
  status = childStatus1 + childStatus2;
  zbot::log << "[MONITOR] Stop. exit:" + std::to_string(status);
  unlink(pidFile.c_str());
  return status;
}

int zbot::zfork() {
  int status, pid;
  pid = fork();

  if (pid < 0) {
    zbot::log.write(ZLogger::LogLevel::ERROR, "Error: start failed");
    return EXIT_FAILURE;
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }
  umask(0);
  setsid();
  if (chdir("/")) zbot::log << "enter dir / ";
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  status = zbot::zmonitor();
  return status;
};

void zbot::signal_error(int sig, siginfo_t* si, void* ptr) {
  void* ErrorAddr;
  void* Trace[16];
  int x;
  int TraceSize;
  char** Messages;

  std::string signaltext = strsignal(sig);
  zbot::log.write(ZLogger::LogLevel::ERROR, "zbotd: Signal: " + signaltext);

#if __WORDSIZE == 64
  ErrorAddr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_RIP];
#else
  ErrorAddr = (void*)((ucontext_t*)ptr)->uc_mcontext.gregs[REG_EIP];
#endif

  TraceSize = backtrace(Trace, 16);
  Trace[1]  = ErrorAddr;

  Messages = backtrace_symbols(Trace, TraceSize);
  if (Messages) {
    zbot::log.write(ZLogger::LogLevel::ERROR, "== Backtrace ==");

    for (x = 1; x < TraceSize; x++) {
      zbot::log.write(ZLogger::LogLevel::ERROR, Messages[x]);
    }
    zbot::log.write(ZLogger::LogLevel::ERROR, "== End Backtrace ==");
    free(Messages);
  }
  exit(zbot::ChildSignal::CHILD_TERMINATE);
}