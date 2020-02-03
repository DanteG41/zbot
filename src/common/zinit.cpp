#include <cstring>
#include <fstream>
#include <wait.h>
#include <zinit.h>

namespace zbot {
ZLogger log;
ZConfig mainConfig;
} // namespace zbot

void zbot::init() {
  if (geteuid() == 0) {
    fprintf(stderr,
            "%s: cannot be run as root\n"
            "Please log in (using, e.g., \"su\") as the "
            "(unprivileged) user.\n",
            program_invocation_name);
  }
  srand(time(NULL));
}

void zbot::setPidFile(std::string& f) {
  std::ofstream pidFile;
  pidFile.open(f);
  pidFile << getpid();
  pidFile.close();
}

int zbot::startWorker(int& pid, int& status, int& start, int (*func)()) {
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
  status = func();
  exit(status);
}

int zbot::zwait(int& pid, int& start, siginfo_t& siginfo) {
  int status;

  if (siginfo.si_signo == SIGCHLD) {
    waitpid(pid, &status, 0);
    status = WEXITSTATUS(status);
    if (status == zbot::ChildSignal::CHILD_TERMINATE) {
      zbot::log << "[MONITOR] Child with pid " + std::to_string(pid) + " stopped";
      return zbot::ChildSignal::CHILD_TERMINATE;
    } else if (status == zbot::ChildSignal::CHILD_RESTART) {
      zbot::log << "[MONITOR] Child restart";
      start = 1;
    }
  } else if (siginfo.si_signo == SIGUSR1) // reload config
  {
    kill(pid, SIGUSR1); // send to child
    start = 0;
  } else // other signal
  {
    zbot::log << "[MONITOR] Signal " + siginfo.si_signo;
    kill(pid, SIGTERM);
    return 1;
  }

  return 1;
}

int zbot::zmonitor() {
  int botPid, senderPid;
  int botStatus, senderStatus, status;
  int childStatus1, childStatus2;
  int senderNeedStart = 1;
  int botNeedStart    = 1;
  std::string pidFile;
  ZConfig config;
  zbot::mainConfig.getParam("pid_file", pidFile);

  setPidFile(pidFile);

  sigset_t sigset;
  siginfo_t siginfo;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGQUIT);
  sigaddset(&sigset, SIGINT);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGCHLD);
  sigaddset(&sigset, SIGUSR1);
  sigprocmask(SIG_BLOCK, &sigset, NULL);

  while (true) {
    zbot::startWorker(botPid, botStatus, botNeedStart, zbot::workerBot);
    zbot::startWorker(senderPid, senderStatus, senderNeedStart, zbot::workerSender);
    sigwaitinfo(&sigset, &siginfo);
    zbot::log << std::to_string(siginfo.si_pid);
    if (siginfo.si_pid == botPid) childStatus1 = zbot::zwait(botPid, botNeedStart, siginfo);
    if (siginfo.si_pid == senderPid)
      childStatus2 = zbot::zwait(senderPid, senderNeedStart, siginfo);
    if (childStatus1 == zbot::ChildSignal::CHILD_TERMINATE &&
        childStatus2 == zbot::ChildSignal::CHILD_TERMINATE) {
      break;
    }
    sleep(2);
  }
  status = childStatus1 + childStatus2;
  // zbot::log.open();
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
  chdir("/");
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
  status = zbot::zmonitor();
  return status;
};

int zbot::workerBot() {
  zbot::log << "start bot worker";
  sleep(2);
  return zbot::ChildSignal::CHILD_RESTART;
  //return zbot::ChildSignal::CHILD_TERMINATE;
}
int zbot::workerSender() {
  sleep(2);
  zbot::log << "start sender worker";
  return zbot::ChildSignal::CHILD_RESTART;
  //return zbot::ChildSignal::CHILD_TERMINATE;
}