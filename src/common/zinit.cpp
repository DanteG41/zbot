#include <boost/algorithm/string.hpp>
#include <chrono>
#include <execinfo.h>
#include <fstream>
#include <string>
#include <wait.h>
#include <zinit.h>

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
  std::string pidFile;
  zbot::mainConfig.getParam("pid_file", pidFile);

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

  while (true) {
    zbot::startWorker(botPid, botStatus, botNeedStart, zbot::workerBot);
    zbot::startWorker(senderPid, senderStatus, senderNeedStart, zbot::workerSender);
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

int zbot::workerBot(sigset_t& sigset, siginfo_t& siginfo) {
  zbot::setProcName("zbotd: bot");
  zbot::log << "zbotd: start telegram bot worker";

  ZConfig telegramConfig, zabbixConfig;
  config configBot;
  int webhookPid = 0;

  telegramConfig.configFile = mainConfig.configFile;
  zabbixConfig.configFile   = mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  zabbixConfig.load("zabbix", defaultconfig::zabbixParams);
  zbot::botGetParams(telegramConfig, zabbixConfig, configBot);

  TgBot::InlineKeyboardMarkup::Ptr mainMenu = zbot::createMenu(zbot::Menu::MAIN);
  TgBot::InlineKeyboardMarkup::Ptr infoMenu = zbot::createMenu(zbot::Menu::INFO);

  TgBot::Bot bot(configBot.token);
  std::string webhookUrl = "https://";
  webhookUrl += configBot.webhookPublicHost;
  webhookUrl += configBot.webhookPath;

  bot.getEvents().onCallbackQuery(
      [&bot, &mainMenu, &infoMenu, &configBot](TgBot::CallbackQuery::Ptr callback) {
        if (configBot.adminUsers.count(callback->from->username)) {
          if (callback->data == "info") {
            bot.getApi().editMessageText("*Info:*", callback->message->chat->id,
                                         callback->message->messageId, callback->inlineMessageId,
                                         "Markdown", false, infoMenu);
          } else if (callback->data == "getchatid") {
            std::string response;
            response = "Chatid: " + std::to_string(callback->message->chat->id);
            bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
            bot.getApi().sendMessage(callback->message->chat->id, response);
          } else if (callback->data == "userlist") {
            std::string users, response;
            for (std::string s : configBot.adminUsers) {
              users += s + "\n";
            }
            response = "Users: \n" + users;
            bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
            bot.getApi().sendMessage(callback->message->chat->id, response);
          }
        }
      });
  bot.getEvents().onCommand("start", [&bot, &mainMenu, &configBot](TgBot::Message::Ptr message) {
    if (configBot.adminUsers.count(message->from->username)) {
      bot.getApi().sendMessage(message->chat->id, "*Selecting an action:*", false, 0, mainMenu,
                               "MarkDown");
    } else {
      bot.getApi().sendMessage(message->chat->id, "Access denied");
    }
  });

  TgBot::TgLongPoll longPoll(bot, 100, 10);

  if (configBot.webhook) {
    TgBot::TgWebhookTcpServer webhookServer(configBot.webhookBindPort, configBot.webhookPath,
                                            bot.getEventHandler());
    bot.getApi().setWebhook(webhookUrl);
    webhookPid = fork();
    if (webhookPid < 0) {
      zbot::log.write(ZLogger::LogLevel::ERROR, "zbotd: webhookServer fork failed " + errno);
    }
    if (webhookPid == 0) {
      zbot::log << "zbotd: start webhookServer";
      zbot::setProcName("zbotd: webhookServer");
      webhookServer.start();
    }
  }

  while (true) {
    struct timespec timeout;
    if (configBot.webhook) {
      timeout.tv_sec  = 10;
      timeout.tv_nsec = 0;
    } else {
      timeout.tv_sec  = 0;
      timeout.tv_nsec = 10000000;
    }

    if (sigtimedwait(&sigset, &siginfo, &timeout) > 0) {
      if (siginfo.si_signo == SIGUSR1) {
        telegramConfig.load("telegram", defaultconfig::telegramParams);
        zabbixConfig.load("zabbix", defaultconfig::zabbixParams);
        mainConfig.load(defaultconfig::params);
        zbot::botGetParams(telegramConfig, zabbixConfig, configBot);
        zbot::log << "zbotd: bot reload config";
      } else if (siginfo.si_signo == SIGTERM) {
        if (configBot.webhook) {
          kill(webhookPid, SIGKILL);
          zbot::log << "zbotd: stop webhookServer worker by signal SIGKILL";
        }
        zbot::log << "zbotd: stop bot worker by signal SIGTERM";
        exit(zbot::ChildSignal::CHILD_TERMINATE);
      }
    }
    try {
      if (!configBot.webhook) {
        bot.getApi().deleteWebhook();
        longPoll.start();
      }
    } catch (TgBot::TgException& e) {
      std::string err = "TgBot exception: ";
      err += e.what();
      zbot::log.write(ZLogger::LogLevel::ERROR, err);
      continue;
    }
  }
  zbot::log << "zbotd: stopped bot worker";
  if (configBot.webhook) {
    kill(webhookPid, SIGKILL);
    zbot::log << "zbotd: stop webhookServer worker by signal SIGKILL";
  }
  return zbot::ChildSignal::CHILD_TERMINATE;
}

void zbot::botGetParams(ZConfig& tc, ZConfig& zc, zbot::config& c) {
  std::string adminUsers;

  try {
    tc.getParam("token", c.token);
    tc.getParam("admin_users", adminUsers);
    tc.getParam("webhook_enable", c.webhook);
    tc.getParam("webhook_path", c.webhookPath);
    tc.getParam("webhook_public_host", c.webhookPublicHost);
    tc.getParam("webhook_bind_port", c.webhookBindPort);
    zc.getParam("zabbix_server", c.zabbixServer);
    mainConfig.getParam("wait", c.wait);
  } catch (ZConfigException& e) {
    zbot::log.write(ZLogger::LogLevel::WARNING, e.getError());
  }

  // parse and place the list of users in the set container
  boost::split(c.adminUsers, adminUsers, boost::is_any_of(";, "));
}

TgBot::InlineKeyboardMarkup::Ptr zbot::createMenu(Menu menu) {
  switch (menu) {
  case zbot::Menu::MAIN: {
    auto mainMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto info(std::make_shared<TgBot::InlineKeyboardButton>());
    auto maintenance(std::make_shared<TgBot::InlineKeyboardButton>());
    auto actions(std::make_shared<TgBot::InlineKeyboardButton>());
    auto screen(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> mainrow1;
    std::vector<TgBot::InlineKeyboardButton::Ptr> mainrow2;

    info->text                = "Info";
    info->callbackData        = "info";
    maintenance->text         = "Maintenance";
    maintenance->callbackData = "maintenance";
    actions->text             = "Actions";
    actions->callbackData     = "actions";
    screen->text              = "Screen";
    screen->callbackData      = "screen";

    mainrow1.push_back(info);
    mainrow1.push_back(maintenance);
    mainrow2.push_back(actions);
    mainrow2.push_back(screen);
    mainMenu->inlineKeyboard.push_back(mainrow1);
    mainMenu->inlineKeyboard.push_back(mainrow2);
    return mainMenu;
  }
  case zbot::Menu::INFO: {
    auto infoMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto getchatid(std::make_shared<TgBot::InlineKeyboardButton>());
    auto userlist(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> inforow;

    getchatid->text         = "Get chatid";
    getchatid->callbackData = "getchatid";
    userlist->text          = "User list";
    userlist->callbackData  = "userlist";

    inforow.push_back(getchatid);
    inforow.push_back(userlist);
    infoMenu->inlineKeyboard.push_back(inforow);
    return infoMenu;
  }
  case zbot::Menu::ACTION:
    break;
  case zbot::Menu::MAINTENANCE:
    break;
  default:
    break;
  }
}

void zbot::senderGetParams(ZConfig& tc, zbot::config& c) {
  try {
    tc.getParam("token", c.token);
    mainConfig.getParam("storage", c.path);
    mainConfig.getParam("max_messages", c.maxmessages);
    mainConfig.getParam("min_approx", c.minapprox);
    mainConfig.getParam("accuracy", c.accuracy);
    mainConfig.getParam("spread", c.spread);
    mainConfig.getParam("wait", c.wait);
  } catch (ZConfigException& e) {
    zbot::log.write(ZLogger::LogLevel::WARNING, e.getError());
  }
}

int zbot::workerSender(sigset_t& sigset, siginfo_t& siginfo) {
  zbot::setProcName("zbotd: sender");
  zbot::log << "zbotd: start sender worker";

  ZConfig telegramConfig;
  std::vector<std::string> messages;
  config configSender;

  telegramConfig.configFile = mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  zbot::senderGetParams(telegramConfig, configSender);
  ZStorage zbotStorage(configSender.path);
  ZStorage pendingStorage(configSender.path + "/pending");
  ZStorage processingStorage(configSender.path + "/processing");
  Ztbot tbot(configSender.token);

  while (true) {
    struct timespec timeout;
    timeout.tv_sec  = configSender.wait;
    timeout.tv_nsec = 0;

    if (sigtimedwait(&sigset, &siginfo, &timeout) > 0) {
      if (siginfo.si_signo == SIGUSR1) {
        telegramConfig.load("telegram", defaultconfig::telegramParams);
        mainConfig.load(defaultconfig::params);
        zbot::senderGetParams(telegramConfig, configSender);
        zbot::log << "zbotd: sender reload config";
      } else if (siginfo.si_signo == SIGTERM) {
        zbot::log << "zbotd: stop sender worker by signal SIGTERM";
        exit(zbot::ChildSignal::CHILD_TERMINATE);
      }
    }
    try {
      for (std::string chat : pendingStorage.listChats()) {
        ZMsgBox sendBox(pendingStorage, chat.c_str());
        sendBox.load(configSender.maxmessages);
        sendBox.move(processingStorage);
        if (sendBox.size() > configSender.minapprox) {
          messages = sendBox.approximation(configSender.accuracy, configSender.spread);
        } else {
          messages = sendBox.popMessages();
        }
        for (std::string msg : messages) {
          tbot.send(atoll(chat.c_str()), msg);
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
  zbot::log << "zbotd: stopped sender worker";
  return zbot::ChildSignal::CHILD_TERMINATE;
}

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