#include <chrono>
#include <cstring>
#include <fstream>
#include <string>
#include <wait.h>
#include <zinit.h>
#include <ztbot.h>

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
  }
  auto t0      = std::chrono::high_resolution_clock::now();
  auto nanosec = t0.time_since_epoch();
  srand(nanosec.count());
}

void zbot::setPidFile(std::string& f) {
  std::ofstream pidFile;
  pidFile.open(f);
  if (!pidFile.is_open()) exit(EXIT_FAILURE);
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
    return zbot::ChildSignal::CHILD_TERMINATE;
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
  zbot::mainConfig.getParam("pid_file", pidFile);

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
  zbot::log << "[MONITOR] Stop. exit:" + std::to_string(status);
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

int zbot::workerBot() {
  zbot::setProcName("zbotd: bot");
  zbot::log << "start zbotd: telegram bot worker";
  sleep(120);
  // return zbot::ChildSignal::CHILD_RESTART;
  return zbot::ChildSignal::CHILD_TERMINATE;
}

int zbot::workerSender() {
  zbot::setProcName("zbotd: sender");
  zbot::log << "start zbotd: sender worker";

  std::string path, token;
  int maxmessages, minapprox, wait;
  float accuracy, spread;
  ZConfig telegramConfig;
  std::vector<std::string> messages;

  telegramConfig.configFile = mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  telegramConfig.getParam("token", token);
  mainConfig.getParam("storage", path);
  mainConfig.getParam("max_messages", maxmessages);
  mainConfig.getParam("min_approx", minapprox);
  mainConfig.getParam("accuracy", accuracy);
  mainConfig.getParam("spread", spread);
  mainConfig.getParam("wait", wait);
  ZStorage zbotStorage(path);
  ZStorage pendingStorage(path + "/pending");
  ZStorage processingStorage(path + "/processing");
  Ztbot tbot(token);
  while (true) {
    sleep(wait);
    try {
      for (std::string chat : pendingStorage.listChats()) {
        ZMsgBox sendBox(pendingStorage, chat.c_str());
        sendBox.load(maxmessages);
        sendBox.move(processingStorage);
        if (sendBox.size() > minapprox) {
          messages = sendBox.approximation(accuracy, spread);
        } else {
          messages = sendBox.popMessages();
        }
        for (std::string msg : messages) {
          tbot.send(atoi(chat.c_str()), msg);
        }
        sendBox.erase();
      }
    } catch (ZStorageException& e) {
      zbot::log.write(ZLogger::LogLevel::ERROR, e.getError());
      continue;
    } catch (TgBot::TgException& e) {
      std::string err = "TgBot exception: ";
      err += e.what();
      zbot::log.write(ZLogger::LogLevel::ERROR, err);
      continue;
    }
  }
  // return zbot::ChildSignal::CHILD_RESTART;
  return zbot::ChildSignal::CHILD_TERMINATE;
}