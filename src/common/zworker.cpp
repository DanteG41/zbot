#include <boost/algorithm/string.hpp>
#include <zworker.h>

int zworker::workerBot(sigset_t& sigset, siginfo_t& siginfo) {
  zbot::setProcName("zbotd: bot");
  zbot::log << "zbotd: start telegram bot worker";

  ZConfig telegramConfig, zabbixConfig;
  zbot::config configBot;
  int webhookPid = 0;

  telegramConfig.configFile = zbot::mainConfig.configFile;
  zabbixConfig.configFile   = zbot::mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  zabbixConfig.load("zabbix", defaultconfig::zabbixParams);
  zworker::botGetParams(telegramConfig, zabbixConfig, configBot);

  TgBot::InlineKeyboardMarkup::Ptr mainMenu = zworker::createMenu(zbot::Menu::MAIN);
  TgBot::InlineKeyboardMarkup::Ptr infoMenu = zworker::createMenu(zbot::Menu::INFO);

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
      zbot::setProcName("zbotd: webhook");
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
        zbot::mainConfig.load(defaultconfig::params);
        zworker::botGetParams(telegramConfig, zabbixConfig, configBot);
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

void zworker::botGetParams(ZConfig& tc, ZConfig& zc, zbot::config& c) {
  std::string adminUsers;

  try {
    tc.getParam("token", c.token);
    tc.getParam("admin_users", adminUsers);
    tc.getParam("webhook_enable", c.webhook);
    tc.getParam("webhook_path", c.webhookPath);
    tc.getParam("webhook_public_host", c.webhookPublicHost);
    tc.getParam("webhook_bind_port", c.webhookBindPort);
    zc.getParam("zabbix_api", c.zabbixApi);
    zc.getParam("user", c.zabbixUser);
    zc.getParam("password", c.zabbixPassword);
    zbot::mainConfig.getParam("wait", c.wait);
  } catch (ZConfigException& e) {
    zbot::log.write(ZLogger::LogLevel::WARNING, e.getError());
  }

  // parse and place the list of users in the set container
  boost::split(c.adminUsers, adminUsers, boost::is_any_of(";, "));
}

void zworker::addButton(std::vector<TgBot::InlineKeyboardButton::Ptr>& row, std::string text,
                        std::string callbackData) {
  auto button(std::make_shared<TgBot::InlineKeyboardButton>());
  button->text         = text;
  button->callbackData = callbackData;
  row.push_back(button);
}

std::vector<TgBot::InlineKeyboardButton::Ptr> zworker::createPaginator(int curr, int max,
                                                                       std::string callbackData) {
  std::vector<TgBot::InlineKeyboardButton::Ptr> result;
  std::string samedata;
  samedata += " " + std::to_string(curr);
  samedata += " " + std::to_string(std::time(nullptr));
  if (max > 5) {
    if (curr >= 3 && curr < max - 3) {
      addButton(result, "<<1", callbackData + " 0");
      addButton(result, std::to_string(curr), callbackData + " " + std::to_string(curr - 1));
      addButton(result, "-" + std::to_string(curr + 1) + "-", callbackData + samedata);
      addButton(result, std::to_string(curr + 2), callbackData + " " + std::to_string(curr + 1));
      addButton(result, std::to_string(max) + ">>", callbackData + " " + std::to_string(max - 1));
      return result;
    } else if (curr < 3) {
      for (int c = 0; c < 3; ++c) {
        std::string page = std::to_string(c + 1);
        if (curr == c) {
          addButton(result, "-" + page + "-", callbackData + samedata);
        } else {
          addButton(result, page, callbackData + " " + std::to_string(c));
        }
      }
      addButton(result, "4", callbackData + " 3");
      addButton(result, std::to_string(max) + ">>", callbackData + " " + std::to_string(max - 1));
      return result;
    } else {
      addButton(result, "<<1", callbackData + " 0");
      addButton(result, std::to_string(max - 3), callbackData + " " + std::to_string(max - 4));
      for (int c = max - 3; c < max; ++c) {
        std::string page = std::to_string(c + 1);
        if (curr == c) {
          addButton(result, "-" + page + "-", callbackData + samedata);
        } else {
          addButton(result, page, callbackData + " " + std::to_string(c));
        }
      }
      return result;
    }
  } else {
    for (int c = 0; c < max; ++c) {
      std::string page = std::to_string(c + 1);
      if (curr == c) {
        addButton(result, "-" + page + "-", callbackData + samedata);
      } else {
        addButton(result, page, callbackData + " " + std::to_string(c));
      }
    }
  }
  return result;
}

void zworker::addList(TgBot::InlineKeyboardMarkup::Ptr markup, std::string callbackName,
                      ZZabbix* zabbix,
                      std::vector<std::pair<std::string, std::string>> (ZZabbix::*fp)(int),
                      int page, int lineperpage) {
  std::vector<std::pair<std::string, std::string>> data;
  data = (zabbix->*fp)(100);

  if (data.size() > lineperpage) {
    for (int c = 0; c < lineperpage; ++c) {
      auto button(std::make_shared<TgBot::InlineKeyboardButton>());
      std::vector<TgBot::InlineKeyboardButton::Ptr> row;
      int ind = page * lineperpage + c;
      if (ind < data.size()) {
        button->text         = data[ind].second;
        button->callbackData = callbackName + " " + data[ind].first;
      } else {
        button->text         = "-";
        button->callbackData = "button.empty";
      }
      row.push_back(button);
      markup->inlineKeyboard.push_back(row);
    }
    markup->inlineKeyboard.push_back(
        createPaginator(page, std::ceil(data.size() / float(lineperpage)), callbackName + ".page"));
  } else {
    for (std::pair<std::string, std::string> d : data) {
      auto button(std::make_shared<TgBot::InlineKeyboardButton>());
      std::vector<TgBot::InlineKeyboardButton::Ptr> maintRow;

      button->text         = d.second;
      button->callbackData = callbackName + " " + d.first;

      maintRow.push_back(button);
      markup->inlineKeyboard.push_back(maintRow);
    }
  }
}

TgBot::InlineKeyboardMarkup::Ptr zworker::createMenu(zworker::Menu menu, ZZabbix& zabbix,
                                                     int page) {

  switch (menu) {
  case zworker::Menu::MAIN: {
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
  case zworker::Menu::INFO: {
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

void zworker::senderGetParams(ZConfig& tc, zbot::config& c) {
  try {
    tc.getParam("token", c.token);
    zbot::mainConfig.getParam("storage", c.path);
    zbot::mainConfig.getParam("max_messages", c.maxmessages);
    zbot::mainConfig.getParam("min_approx", c.minapprox);
    zbot::mainConfig.getParam("accuracy", c.accuracy);
    zbot::mainConfig.getParam("spread", c.spread);
    zbot::mainConfig.getParam("wait", c.wait);
  } catch (ZConfigException& e) {
    zbot::log.write(ZLogger::LogLevel::WARNING, e.getError());
  }
}

int zworker::workerSender(sigset_t& sigset, siginfo_t& siginfo) {
  zbot::setProcName("zbotd: sender");
  zbot::log << "zbotd: start sender worker";

  ZConfig telegramConfig;
  std::vector<std::string> messages;
  zbot::config configSender;

  telegramConfig.configFile = zbot::mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  zworker::senderGetParams(telegramConfig, configSender);
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
        zbot::mainConfig.load(defaultconfig::params);
        zworker::senderGetParams(telegramConfig, configSender);
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