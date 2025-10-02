#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <zworker.h>
#include <zmessagehistory.h>
#include <string>
#include <algorithm>
#include <cctype>

// Forward declaration from zmsgbox.cpp
float levensteinDistance(std::string& s, std::string& t);
std::string levensteinOps(std::string& s, std::string& t);

using boost::property_tree::ptree;
using boost::property_tree::write_json;

int zworker::workerBot(sigset_t& sigset, siginfo_t& siginfo) {
  zbot::setProcName("zbotd: bot");
  zbot::log << "zbotd: start telegram bot worker";

  ZConfig telegramConfig, zabbixConfig;
  zbot::config configBot;
  int webhookPid = 0;

  struct zEvent {
    int64_t chat;
    std::string from;
    std::vector<std::string> callback;
  };
  std::vector<zEvent> waitEvent;

  telegramConfig.configFile = zbot::mainConfig.configFile;
  zabbixConfig.configFile   = zbot::mainConfig.configFile;
  telegramConfig.load("telegram", defaultconfig::telegramParams);
  zabbixConfig.load("zabbix", defaultconfig::zabbixParams);
  zworker::botGetParams(telegramConfig, zabbixConfig, configBot);

  ZZabbix zabbix(configBot.zabbixUrl.c_str(), configBot.zabbixUser.c_str(),
                 configBot.zabbixPassword.c_str());
  zabbix.auth();

  TgBot::InlineKeyboardMarkup::Ptr mainMenu = zworker::createMenu(zworker::Menu::MAIN, zabbix);
  TgBot::InlineKeyboardMarkup::Ptr infoMenu = zworker::createMenu(zworker::Menu::INFO, zabbix);

  TgBot::Bot bot(configBot.token);
  std::string webhookUrl = "https://";
  webhookUrl += configBot.webhookPublicHost;
  webhookUrl += configBot.webhookPath;

  bot.getEvents().onCallbackQuery([&bot, &mainMenu, &infoMenu, &configBot, &zabbix,
                                   &waitEvent](TgBot::CallbackQuery::Ptr callback) {
    if (configBot.adminUsers.count(callback->from->username)) {
      std::string messageText;
      bool notify = false;
      if (callback->data == "main") {
        bot.getApi().editMessageText("*Selecting an action:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, mainMenu);
      } else if (callback->data == "info") {
        bot.getApi().editMessageText("*Info:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, infoMenu);
      } else if (callback->data == "actions") {
        TgBot::InlineKeyboardMarkup::Ptr actionMenu = zworker::createMenu(
            zworker::Menu::ACTION, zabbix, 0, "", std::to_string(callback->message->chat->id));
        bot.getApi().editMessageText("*Actions:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, actionMenu);
      } else if (callback->data == "screen") {
        TgBot::InlineKeyboardMarkup::Ptr screenMenu =
            zworker::createMenu(zworker::Menu::SCREEN, zabbix);
        bot.getApi().editMessageText("*Screen:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, screenMenu);
      } else if (callback->data == "maintenance") {
        TgBot::InlineKeyboardMarkup::Ptr maintenanceMenu =
            zworker::createMenu(zworker::Menu::MAINTENANCE, zabbix);
        bot.getApi().editMessageText("*Maintenance:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, maintenanceMenu);
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
      } else if (callback->data == "createmaintenance") {
        TgBot::InlineKeyboardMarkup::Ptr maintenanceMenuSelectHostGrp =
            zworker::createMenu(zworker::Menu::MAINTENANCESELECTHOSTGRP, zabbix);
        bot.getApi().editMessageText("*Maintenance/Select Host group:*",
                                     callback->message->chat->id, callback->message->messageId,
                                     callback->inlineMessageId, "Markdown", false,
                                     maintenanceMenuSelectHostGrp);
      } else if (callback->data == "action.enable") {
        TgBot::InlineKeyboardMarkup::Ptr actionEnableSelect =
            zworker::createMenu(zworker::Menu::ACTIONENABLE, zabbix);
        bot.getApi().editMessageText("*Actions/Select disabled:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, actionEnableSelect);
      } else if (callback->data == "action.disable") {
        TgBot::InlineKeyboardMarkup::Ptr actionDisableSelect =
            zworker::createMenu(zworker::Menu::ACTIONDISABLE, zabbix);
        bot.getApi().editMessageText("*Actions/Select enabled:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, actionDisableSelect);
      } else if (callback->data.compare(0, 18, "action.enable.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr actionEnableSelect =
            zworker::createMenu(zworker::Menu::ACTIONENABLE, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Actions/Select disabled:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, actionEnableSelect);
      } else if (callback->data.compare(0, 19, "action.disable.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr actionDisableSelect =
            zworker::createMenu(zworker::Menu::ACTIONDISABLE, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Actions/Select enabled:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, actionDisableSelect);
      } else if (callback->data.compare(0, 15, "action.disable ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        try {
          bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
          zabbix.updateStatusAction(callbackData[1], 1);
          notify      = true;
          messageText = "User @" + callback->from->username + " disabled action: \"" +
                        zabbix.getActionName(callbackData[1]) + "\"";
          bot.getApi().sendMessage(callback->message->chat->id, messageText);
        } catch (ZZabbixException& e) {
          bot.getApi().sendMessage(callback->message->chat->id, std::string(e.getError()));
        }
      } else if (callback->data.compare(0, 14, "action.enable ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        try {
          bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
          zabbix.updateStatusAction(callbackData[1], 0);
          notify      = true;
          messageText = "User @" + callback->from->username + " enabled action: \"" +
                        zabbix.getActionName(callbackData[1]) + "\"";
          bot.getApi().sendMessage(callback->message->chat->id, messageText);
        } catch (ZZabbixException& e) {
          bot.getApi().sendMessage(callback->message->chat->id, std::string(e.getError()));
        }
      } else if (callback->data == "action.stopall") {
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        std::string storagePath, triggerPath;
        zbot::mainConfig.getParam("storage", storagePath);
        triggerPath = storagePath + "/sending_off";
        std::ofstream trigger(triggerPath);
        notify = true;
        messageText =
            "User @" + callback->from->username + " disabled sending messages for all chats.";
        bot.getApi().sendMessage(callback->message->chat->id, messageText);
      } else if (callback->data == "action.stopchat") {
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        std::string storagePath, triggerPath;
        zbot::mainConfig.getParam("storage", storagePath);
        triggerPath = storagePath + "/pending/" + std::to_string(callback->message->chat->id) +
                      "/sending_off";
        std::ofstream trigger(triggerPath);
        notify      = true;
        messageText = "User @" + callback->from->username +
                      " disabled sending messages for chat: " + callback->message->chat->title +
                      callback->message->chat->firstName;
        bot.getApi().sendMessage(callback->message->chat->id, messageText);
      } else if (callback->data == "action.startall") {
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        std::string storagePath, triggerPath;
        zbot::mainConfig.getParam("storage", storagePath);
        triggerPath = storagePath + "/sending_off";
        unlink(triggerPath.c_str());
        notify = true;
        messageText =
            "User @" + callback->from->username + " enabled sending messages for all chats.";
        bot.getApi().sendMessage(callback->message->chat->id, messageText);
      } else if (callback->data == "action.startchat") {
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        std::string storagePath, triggerPath;
        zbot::mainConfig.getParam("storage", storagePath);
        triggerPath = storagePath + "/pending/" + std::to_string(callback->message->chat->id) +
                      "/sending_off";
        std::ofstream trigger(triggerPath.c_str());
        unlink(triggerPath.c_str());
        notify      = true;
        messageText = "User @" + callback->from->username +
                      " enabled sending messages for chat: " + callback->message->chat->title +
                      callback->message->chat->firstName;
        bot.getApi().sendMessage(callback->message->chat->id, messageText);
      } else if (callback->data.compare(0, 34, "maintenance.create.select.grp.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr maintenanceMenuSelectHostGrp = zworker::createMenu(
            zworker::Menu::MAINTENANCESELECTHOSTGRP, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Maintenance/Select Host group:*",
                                     callback->message->chat->id, callback->message->messageId,
                                     callback->inlineMessageId, "Markdown", false,
                                     maintenanceMenuSelectHostGrp);
      } else if (callback->data.compare(0, 30, "maintenance.create.select.grp ") == 0) {
        zEvent event;
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        event.chat     = callback->message->chat->id;
        event.from     = callback->from->username;
        event.callback = callbackData;
        waitEvent.push_back(event);
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        bot.getApi().sendMessage(callback->message->chat->id, "Input maintenance name:");
      } else if (callback->data.compare(0, 14, "screen.select ") == 0 ||
                 callback->data.compare(0, 22, "screen.select.refresh ") == 0) {
        std::vector<std::string> callbackData, images;
        std::vector<TgBot::InputMedia::Ptr> media;
        std::vector<TgBot::InputFile::Ptr> files;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        if (callbackData[0] == "screen.select.refresh") {
          for (int c = 2; c != callbackData.size(); ++c) {
            bot.getApi().deleteMessage(callback->message->chat->id, std::stoi(callbackData[c]));
          }
        }
        try {
          int c        = 0;
          bool refresh = true;
          images       = zabbix.downloadGraphs(zabbix.getScreenGraphs(callbackData[1]));
          std::string refreshButtonCallbackData;
          auto refreshMarkup(std::make_shared<TgBot::InlineKeyboardMarkup>());
          auto refreshButton(std::make_shared<TgBot::InlineKeyboardButton>());
          std::vector<TgBot::InlineKeyboardButton::Ptr> refreshrow;
          refreshButton->text         = "Refresh";
          refreshButton->callbackData = "screen.select.refresh " + callbackData[1];

          if (images.size() == 0)
            bot.getApi().sendMessage(callback->message->chat->id,
                                     "No graphs on the selected complex screen.");

          for (std::vector<std::string>::iterator it = images.begin(); it != images.end(); it++) {
            auto graph(std::make_shared<TgBot::InputMediaPhoto>());
            TgBot::InputFile::Ptr file = TgBot::InputFile::fromFile(*it, "image/png");
            graph->media               = "attach://" + file->fileName;
            media.push_back(graph);
            files.push_back(file);
            c++;
            // Send it in batches , so as not to exceed the telegram limit.
            if (c == 10 || std::distance(images.begin(), it) + 1 == images.size()) {
              std::vector<TgBot::Message::Ptr> msg;
              msg = bot.getApi().sendMediaGroup(callback->message->chat->id, media, files);
              for (TgBot::Message::Ptr pt : msg) {
                std::string id = std::to_string(pt->messageId);
                // do not exceed telegram limit
                if (refreshButtonCallbackData.size() + id.size() >
                    64 - refreshButton->callbackData.size()) {
                  refresh = false;
                  break;
                }
                refreshButtonCallbackData += " " + id;
              }
              media.clear();
              files.clear();
              c = 0;
            }
          }
          zworker::removeFiles(images);
          if (refresh) {
            refreshButton->callbackData += refreshButtonCallbackData;
            refreshrow.push_back(refreshButton);
            refreshMarkup->inlineKeyboard.push_back(refreshrow);
            bot.getApi().sendMessage(callback->message->chat->id,
                                     "*Screen \"" + zabbix.getScreenName(callbackData[1]) + "\":*",
                                     false, 0, refreshMarkup, "MarkDown");
          }
        } catch (ZZabbixException& e) {
          zworker::removeFiles(images);
          bot.getApi().sendMessage(callback->message->chat->id, std::string(e.getError()));
        }
      } else if (callback->data.compare(0, 18, "screen.select.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr screenMenu =
            zworker::createMenu(zworker::Menu::SCREEN, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Screen:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, screenMenu);
      } else if (callback->data.compare(0, 23, "maintenance.select.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr maintenanceMenu =
            zworker::createMenu(zworker::Menu::MAINTENANCE, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Maintenance:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, maintenanceMenu);
      } else if (callback->data.compare(0, 19, "maintenance.select ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr maintenanceMenuSelect =
            zworker::createMenu(zworker::Menu::MAINTENANCESELECT, zabbix, 0, callbackData[1]);
        bot.getApi().editMessageText(
            "*Maintenance: " + zabbix.getMaintenanceName(callbackData[1]) + "*",
            callback->message->chat->id, callback->message->messageId, callback->inlineMessageId,
            "Markdown", false, maintenanceMenuSelect);
      } else if (callback->data.compare(0, 18, "maintenance.renew ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        std::string response = "✅ Maintenance period renew";
        try {
          zabbix.renewMaintenance(callbackData[1]);
        } catch (ZZabbixException& e) {
          response = "⚠️ ERROR: " + std::string(e.getError());
        }
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        bot.getApi().sendMessage(callback->message->chat->id, response);
      } else if (callback->data.compare(0, 19, "maintenance.delete ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        std::string response = "✅ Maintenance period delete";
        try {
          zabbix.deleteMaintenance(callbackData[1]);
        } catch (ZZabbixException& e) {
          response = "⚠️ ERROR: " + std::string(e.getError());
        }
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        bot.getApi().sendMessage(callback->message->chat->id, response);
      } else if (callback->data == "problems") {
        TgBot::InlineKeyboardMarkup::Ptr problemMenu =
            zworker::createMenu(zworker::Menu::PROBLEMS, zabbix);
        bot.getApi().editMessageText("*Problems:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, problemMenu);
      } else if (callback->data.compare(0, 24, "problems.select.grp.page") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr problemMenu =
            zworker::createMenu(zworker::Menu::PROBLEMS, zabbix, std::stoi(callbackData[1]));
        bot.getApi().editMessageText("*Problems:*", callback->message->chat->id,
                                     callback->message->messageId, callback->inlineMessageId,
                                     "Markdown", false, problemMenu);
      } else if (callback->data.compare(0, 20, "problems.select.grp ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr problemMenu = zworker::createMenu(
            zworker::Menu::PROBLEMSSELECTHOSTGRP, zabbix, 0, "", callbackData[1]);
        bot.getApi().editMessageText("*Problems/" + zabbix.getHostGrpName(callbackData[1]) + ":*",
                                     callback->message->chat->id, callback->message->messageId,
                                     callback->inlineMessageId, "Markdown", false, problemMenu);
      } else if (callback->data.compare(0, 21, "problems.select.page ") == 0) {
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        TgBot::InlineKeyboardMarkup::Ptr problemMenu =
            zworker::createMenu(zworker::Menu::PROBLEMSSELECTHOSTGRP, zabbix,
                                std::stoi(callbackData[2]), "", callbackData[1]);
        bot.getApi().editMessageText("*Problems/" + zabbix.getHostGrpName(callbackData[1]) + ":*",
                                     callback->message->chat->id, callback->message->messageId,
                                     callback->inlineMessageId, "Markdown", false, problemMenu);
      } else if (callback->data.compare(0, 16, "problems.select ") == 0) {
        zEvent event;
        std::vector<std::string> callbackData;
        boost::split(callbackData, callback->data, boost::is_any_of(" "));
        event.chat     = callback->message->chat->id;
        event.from     = callback->from->username;
        event.callback = callbackData;
        waitEvent.push_back(event);
        bot.getApi().deleteMessage(callback->message->chat->id, callback->message->messageId);
        bot.getApi().sendMessage(callback->message->chat->id, "Input acknowledge text:");
      }
      if (notify) {
        for (std::string s : configBot.notifyChats) {
          bot.getApi().sendMessage(std::stoi(s), messageText);
        }
      }
    }
  });
  bot.getEvents().onCommand("start", [&bot, &mainMenu, &configBot](TgBot::Message::Ptr message) {
    std::string botname = "@" + bot.getApi().getMe()->username;
    bool access         = false;
    if (message->text.size() == 6 + botname.size()) {
      if (message->text.compare(message->text.size() - botname.size(), botname.size(), botname) ==
          0)
        access = true;
    } else if (message->text.size() == 6)
      access = true;
    if (access) {
      if (configBot.adminUsers.count(message->from->username)) {
        bot.getApi().sendMessage(message->chat->id, "*Selecting an action:*", false, 0, mainMenu,
                                 "MarkDown");
      } else {
        bot.getApi().sendMessage(message->chat->id, "Access denied");
      }
    }
  });
  bot.getEvents().onNonCommandMessage(
      [&bot, &configBot, &zabbix, &waitEvent](TgBot::Message::Ptr message) {
        for (std::vector<zEvent>::iterator it = waitEvent.begin(); it != waitEvent.end(); it++) {
          if (message->from->username == it->from && message->chat->id == it->chat) {
            if (it->callback[0] == "maintenance.create.select.grp") {
              std::string response = "✅ Maintenance period successful created.";
              try {
                zabbix.createMaintenance(it->callback[1], message->text);
              } catch (ZZabbixException& e) {
                response = "⚠️ ERROR: " + std::string(e.getError());
              }
              bot.getApi().sendMessage(message->chat->id, response);
              it = waitEvent.erase(it);
              if (it == waitEvent.end()) break;
            } else if (it->callback[0] == "problems.select") {
              std::string response = "✅ Problem was acknowledge.";
              try {
                zabbix.ackProblem(zabbix.getEvent(it->callback[1]), message->text);
              } catch (ZZabbixException& e) {
                response = "⚠️ ERROR: " + std::string(e.getError());
              }
              bot.getApi().sendMessage(message->chat->id, response);
              it = waitEvent.erase(it);
              if (it == waitEvent.end()) break;
            }
          }
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
      try {
        webhookServer.start();
      } catch (TgBot::TgException& e) {
        std::string err = "TgBot exception: ";
        err += e.what();
        zbot::log.write(ZLogger::LogLevel::ERROR, err);
      } catch (std::exception& e) {
        std::string err = "WebhookServer exception: ";
        err += e.what();
        zbot::log.write(ZLogger::LogLevel::ERROR, err);
      }
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
    } catch (std::exception& e) {
      std::string err = "LongPoll exception: ";
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
  std::string adminUsers, notifyChats;

  try {
    tc.getParam("token", c.token);
    tc.getParam("admin_users", adminUsers);
    tc.getParam("notify_chats", notifyChats);
    tc.getParam("webhook_enable", c.webhook);
    tc.getParam("webhook_path", c.webhookPath);
    tc.getParam("webhook_public_host", c.webhookPublicHost);
    tc.getParam("webhook_bind_port", c.webhookBindPort);
    zc.getParam("zabbix_url", c.zabbixUrl);
    zc.getParam("user", c.zabbixUser);
    zc.getParam("password", c.zabbixPassword);
    zbot::mainConfig.getParam("wait", c.wait);
  } catch (ZConfigException& e) {
    zbot::log.write(ZLogger::LogLevel::WARNING, e.getError());
  }

  // parse and place the list of users in the set container
  boost::split(c.adminUsers, adminUsers, boost::is_any_of(";, "));
  boost::split(c.notifyChats, notifyChats, boost::is_any_of(";, "));
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
                      ZZabbix* zabbix, std::vector<std::pair<std::string, std::string>> data,
                      int page, std::string pageCallback, int lineperpage) {
  if (data.size() > lineperpage) {
    std::string pageCallbackData = callbackName + ".page";
    if (pageCallback.size() != 0) {
      pageCallbackData += " " + pageCallback;
    }
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
        createPaginator(page, std::ceil(data.size() / float(lineperpage)), pageCallbackData));
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

TgBot::InlineKeyboardMarkup::Ptr zworker::createMenu(zworker::Menu menu, ZZabbix& zabbix, int page,
                                                     std::string callback, std::string arg) {

  switch (menu) {
  case zworker::Menu::MAIN: {
    auto mainMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto info(std::make_shared<TgBot::InlineKeyboardButton>());
    auto maintenance(std::make_shared<TgBot::InlineKeyboardButton>());
    auto actions(std::make_shared<TgBot::InlineKeyboardButton>());
    auto screen(std::make_shared<TgBot::InlineKeyboardButton>());
    auto problems(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> mainrow1;
    std::vector<TgBot::InlineKeyboardButton::Ptr> mainrow2;
    std::vector<TgBot::InlineKeyboardButton::Ptr> mainrow3;

    info->text                = "Info";
    info->callbackData        = "info";
    maintenance->text         = "Maintenance";
    maintenance->callbackData = "maintenance";
    actions->text             = "Actions";
    actions->callbackData     = "actions";
    screen->text              = "Screen";
    screen->callbackData      = "screen";
    problems->text            = "Problems";
    problems->callbackData    = "problems";

    mainrow1.push_back(info);
    mainrow1.push_back(maintenance);
    mainrow2.push_back(actions);
    mainrow2.push_back(screen);
    mainrow3.push_back(problems);
    mainMenu->inlineKeyboard.push_back(mainrow1);
    mainMenu->inlineKeyboard.push_back(mainrow2);
    mainMenu->inlineKeyboard.push_back(mainrow3);
    return mainMenu;
  }
  case zworker::Menu::INFO: {
    auto infoMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto getchatid(std::make_shared<TgBot::InlineKeyboardButton>());
    auto userlist(std::make_shared<TgBot::InlineKeyboardButton>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> inforow;
    std::vector<TgBot::InlineKeyboardButton::Ptr> inforow2;

    getchatid->text         = "Get chatid";
    getchatid->callbackData = "getchatid";
    userlist->text          = "User list";
    userlist->callbackData  = "userlist";
    back->text              = "Back";
    back->callbackData      = "main";

    inforow.push_back(getchatid);
    inforow.push_back(userlist);
    inforow2.push_back(back);
    infoMenu->inlineKeyboard.push_back(inforow);
    infoMenu->inlineKeyboard.push_back(inforow2);
    return infoMenu;
  }
  case zworker::Menu::ACTION: {
    std::string storagePath;
    auto actionMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto enable(std::make_shared<TgBot::InlineKeyboardButton>());
    auto disable(std::make_shared<TgBot::InlineKeyboardButton>());
    auto stopall(std::make_shared<TgBot::InlineKeyboardButton>());
    auto stopchat(std::make_shared<TgBot::InlineKeyboardButton>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow1;
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow2;
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow3;
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow4;

    enable->text          = "Enable action";
    enable->callbackData  = "action.enable";
    disable->text         = "Disable action";
    disable->callbackData = "action.disable";

    zbot::mainConfig.getParam("storage", storagePath);
    struct stat stall;
    struct stat stchat;
    std::string triggerFileForAll  = storagePath + "/sending_off";
    std::string triggerFileForChat = storagePath + "/pending/" + arg + "/sending_off";
    stat(triggerFileForAll.c_str(), &stall);
    stat(triggerFileForChat.c_str(), &stchat);

    if (S_ISREG(stall.st_mode)) {
      stopall->text         = "Start all messages";
      stopall->callbackData = "action.startall";
    } else {
      stopall->text         = "Stop all messages";
      stopall->callbackData = "action.stopall";
    }
    if (S_ISREG(stchat.st_mode)) {
      stopchat->text         = "Start messages in this chat";
      stopchat->callbackData = "action.startchat";
    } else {
      stopchat->text         = "Stop messages in this chat";
      stopchat->callbackData = "action.stopchat";
    }
    back->text         = "Back";
    back->callbackData = "main";

    actionrow1.push_back(enable);
    actionrow1.push_back(disable);
    actionrow2.push_back(stopall);
    actionrow3.push_back(stopchat);
    actionrow4.push_back(back);
    actionMenu->inlineKeyboard.push_back(actionrow1);
    actionMenu->inlineKeyboard.push_back(actionrow2);
    actionMenu->inlineKeyboard.push_back(actionrow3);
    actionMenu->inlineKeyboard.push_back(actionrow4);
    return actionMenu;
  }
  case zworker::Menu::ACTIONENABLE: {
    auto actionMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow;

    back->text         = "Back";
    back->callbackData = "actions";

    zworker::addList(actionMenu, "action.enable", &zabbix, &ZZabbix::getActions, 1, page);
    actionrow.push_back(back);
    actionMenu->inlineKeyboard.push_back(actionrow);
    return actionMenu;
  }
  case zworker::Menu::ACTIONDISABLE: {
    auto actionMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> actionrow;

    back->text         = "Back";
    back->callbackData = "actions";

    zworker::addList(actionMenu, "action.disable", &zabbix, &ZZabbix::getActions, 0, page);
    actionrow.push_back(back);
    actionMenu->inlineKeyboard.push_back(actionrow);
    return actionMenu;
  }
  case zworker::Menu::SCREEN: {
    auto screenMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> screenrow;

    back->text         = "Back";
    back->callbackData = "main";

    zworker::addList(screenMenu, "screen.select", &zabbix, &ZZabbix::getScreens, page);
    screenrow.push_back(back);
    screenMenu->inlineKeyboard.push_back(screenrow);
    return screenMenu;
  }
  case zworker::Menu::MAINTENANCE: {
    auto maintenanceMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto create(std::make_shared<TgBot::InlineKeyboardButton>());
    auto back(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> maintenancerow;
    std::vector<TgBot::InlineKeyboardButton::Ptr> maintenancerow2;

    create->text         = "Create new maintenance period";
    create->callbackData = "createmaintenance";
    back->text           = "Back";
    back->callbackData   = "main";

    zworker::addList(maintenanceMenu, "maintenance.select", &zabbix, &ZZabbix::getMaintenances,
                     page);

    maintenancerow.push_back(create);
    maintenancerow2.push_back(back);
    maintenanceMenu->inlineKeyboard.push_back(maintenancerow);
    maintenanceMenu->inlineKeyboard.push_back(maintenancerow2);
    return maintenanceMenu;
  }
  case zworker::Menu::MAINTENANCESELECT: {
    auto maintenanceMenuSelect(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto renewButton(std::make_shared<TgBot::InlineKeyboardButton>());
    auto deleteButton(std::make_shared<TgBot::InlineKeyboardButton>());
    auto backButton(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> maintenancerow1;
    std::vector<TgBot::InlineKeyboardButton::Ptr> maintenancerow2;

    renewButton->text          = "Renew for 1 hour";
    renewButton->callbackData  = "maintenance.renew " + callback;
    deleteButton->text         = "Delete";
    deleteButton->callbackData = "maintenance.delete " + callback;
    backButton->text           = "Back";
    backButton->callbackData   = "maintenance";

    maintenancerow1.push_back(renewButton);
    maintenancerow1.push_back(deleteButton);
    maintenancerow2.push_back(backButton);
    maintenanceMenuSelect->inlineKeyboard.push_back(maintenancerow1);
    maintenanceMenuSelect->inlineKeyboard.push_back(maintenancerow2);
    return maintenanceMenuSelect;
  }
  case zworker::Menu::MAINTENANCESELECTHOSTGRP: {
    auto maintenanceMenuSelectHostGrp(std::make_shared<TgBot::InlineKeyboardMarkup>());
    zworker::addList(maintenanceMenuSelectHostGrp, "maintenance.create.select.grp", &zabbix,
                     &ZZabbix::getHostGrp, 0, page);
    return maintenanceMenuSelectHostGrp;
  }
  case zworker::Menu::PROBLEMS: {
    auto problemsMenu(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto backButton(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> problemsrow1;

    zworker::addList(problemsMenu, "problems.select.grp", &zabbix, &ZZabbix::getHostGrp, 1, page);

    backButton->text         = "Back";
    backButton->callbackData = "main";

    problemsrow1.push_back(backButton);
    problemsMenu->inlineKeyboard.push_back(problemsrow1);
    return problemsMenu;
  }
  case zworker::Menu::PROBLEMSSELECTHOSTGRP: {
    auto problemsMenuSelectHostGrp(std::make_shared<TgBot::InlineKeyboardMarkup>());
    auto backButton(std::make_shared<TgBot::InlineKeyboardButton>());
    std::vector<TgBot::InlineKeyboardButton::Ptr> problemsrow1;

    zworker::addList(problemsMenuSelectHostGrp, "problems.select", &zabbix, &ZZabbix::getProblems,
                     std::stoi(arg), page, arg);

    backButton->text         = "Back";
    backButton->callbackData = "problems";

    problemsrow1.push_back(backButton);
    problemsMenuSelectHostGrp->inlineKeyboard.push_back(problemsrow1);
    return problemsMenuSelectHostGrp;
  }
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
    zbot::mainConfig.getParam("dont_approximate_multibyte", c.dont_approximate_multibyte);
    zbot::mainConfig.getParam("immediate_send", c.immediateSend);
    zbot::mainConfig.getParam("history_check_count", c.historyCheckCount);
    zbot::mainConfig.getParam("history_max_age_minutes", c.historyMaxAgeMinutes);
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
  ZMessageHistory history;

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
        if (!zbotStorage.checkTrigger() || !sendBox.checkTrigger()) sendBox.disable();
        sendBox.load(configSender.maxmessages);
        sendBox.move(processingStorage);
        messages = sendBox.popMessages();
        if (sendBox.getStatus()) {
          try {
            int64_t chatId = atoll(chat.c_str());
            // Immediate mode: check recent history (in-memory) and group/send accordingly
            if (configSender.immediateSend) {
              history.cleanup(configSender.historyMaxAgeMinutes);
              for (const std::string& msg : messages) {
                auto recent = history.getRecentMessages(chatId, configSender.historyCheckCount, configSender.historyMaxAgeMinutes);

                auto stripHeader = [](const std::string& s) -> std::pair<int,std::string> {
                  // returns {count, pattern} or {0, s}
                  size_t i = 0;
                  while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
                  const std::string hdr = " similar messages were received:\n";
                  if (i > 0 && i + hdr.size() <= s.size() && s.compare(i, hdr.size(), hdr) == 0) {
                    int cnt = std::stoi(s.substr(0, i));
                    return {cnt, s.substr(i + hdr.size())};
                  }
                  return {0, s};
                };
                auto matchesTemplate = [](const std::string& pattern, const std::string& text) {
                  size_t i = 0, j = 0;
                  while (i < pattern.size() && j < text.size()) {
                    if (pattern[i] == '?') {
                      // legacy multi-byte wildcards: consume following spaces (0..3)
                      i++;
                      int spaceCount = 0;
                      while (i < pattern.size() && spaceCount < 3 && pattern[i] == ' ') { i++; spaceCount++; }
                      // If number run, greedily consume digits
                      if (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) {
                        while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) j++;
                        continue;
                      }
                      unsigned char c = static_cast<unsigned char>(text[j]);
                      int adv = (c & 0x80) == 0 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4;
                      j += adv;
                    } else {
                      if (pattern[i] != text[j]) return false;
                      i++; j++;
                    }
                  }
                  if (i == pattern.size() && j == text.size()) return true;
                  if (j == text.size()) {
                    while (i < pattern.size()) {
                      if (pattern[i] != '?') return false;
                      i++;
                      int spaceCount = 0; while (i < pattern.size() && spaceCount < 3 && pattern[i] == ' ') { i++; spaceCount++; }
                    }
                    return true;
                  }
                  return false;
                };
                auto patternsEquivalent = [&](const std::string& a, const std::string& b){
                  return matchesTemplate(a,b) && matchesTemplate(b,a);
                };

                // 1) Попробовать обновить уже отправленную группу
                int mergedCount = 1; std::string basePattern; std::vector<int32_t> groupIds;
                for (const auto& hm : recent) {
                  if (!hm.isGroup || hm.groupPattern.empty()) continue;
                  if (matchesTemplate(hm.groupPattern, msg)) {
                    if (basePattern.empty()) basePattern = hm.groupPattern;
                    if (patternsEquivalent(basePattern, hm.groupPattern)) {
                      mergedCount += hm.groupCount;
                      groupIds.push_back(hm.messageId);
                    }
                  }
                }
                // Если прямого совпадения шаблона нет, попробуем адаптивно слить по Левенштейну (<= accuracy)
                if (groupIds.empty()) {
                  auto mergeWithPattern = [&](const std::string& pat, const std::string& text){
                    std::string a = pat; std::string b = text;
                    std::string ops = levensteinOps(a, b);
                    int diffs = 0; for (char c: ops) if (c != '=') diffs++;
                    size_t maxLen = std::max(a.size(), b.size());
                    if (maxLen == 0) return std::pair<bool,std::string>(false, std::string());
                    float ndist = (float)diffs / (float)maxLen;
                    if (ndist > configSender.accuracy) return std::pair<bool,std::string>(false, std::string());
                    // Сформировать объединённый шаблон
                    std::string out; out.reserve(maxLen);
                    size_t ia = 0, ib = 0;
                    for (char op : ops) {
                      if (op == '=') {
                        char ca = a[ia], cb = b[ib];
                        if (ca == '?' || cb == '?') out.push_back('?'); else out.push_back(ca);
                        ia++; ib++;
                      } else if (op == '!') {
                        out.push_back('?'); ia++; ib++;
                      } else if (op == '+') { // insertion in a
                        out.push_back('?'); ib++;
                      } else if (op == '-') { // deletion from a
                        out.push_back('?'); ia++;
                      }
                    }
                    return std::pair<bool,std::string>(true, out);
                  };
                  for (const auto& hm : recent) {
                    if (!hm.isGroup || hm.groupPattern.empty()) continue;
                    auto res = mergeWithPattern(hm.groupPattern, msg);
                    if (res.first) {
                      basePattern = res.second;
                      mergedCount = hm.groupCount + 1;
                      groupIds.push_back(hm.messageId);
                      break;
                    }
                  }
                }
                if (!groupIds.empty()) {
                  tbot.deleteMessages(chatId, groupIds);
                  history.removeMessages(chatId, groupIds);
                  std::string groupedText = basePattern + "\n\nSimilar messages received: " + std::to_string(mergedCount);
                  auto sent = tbot.sendMessage(chatId, groupedText);
                  if (sent) history.addMessage(chatId, sent->messageId, groupedText, true, basePattern, mergedCount);
                  continue;
                }

                // 2) Иначе попытаться собрать новую группу с одиночными
                // Build a temporary box with current msg plus recent singles
                ZMsgBox box(processingStorage, chat.c_str());
                box.pushMessage(msg);
                for (const auto& hm : recent) if (!hm.isGroup) box.pushMessage(hm.text);
                auto pats = box.approximation(configSender.accuracy, configSender.spread, configSender.dont_approximate_multibyte);

                int bestCount = 0; std::string bestPattern;
                for (const auto& pr : pats) {
                  auto [cnt, pat] = stripHeader(pr);
                  if (cnt >= 2 && matchesTemplate(pat, msg)) {
                    if (cnt > bestCount) { bestCount = cnt; bestPattern = pat; }
                  }
                }

                if (bestCount >= 2) {
                  // Collect IDs of matching recent singles
                  std::vector<int32_t> toDelete;
                  for (const auto& hm : recent) {
                    if (!hm.isGroup && matchesTemplate(bestPattern, hm.text)) toDelete.push_back(hm.messageId);
                  }
                  if (!toDelete.empty()) { tbot.deleteMessages(chatId, toDelete); history.removeMessages(chatId, toDelete); }
                  std::string groupedText = bestPattern + "\n\nSimilar messages received: " + std::to_string(bestCount);
                  auto sent = tbot.sendMessage(chatId, groupedText);
                  if (sent) history.addMessage(chatId, sent->messageId, groupedText, true, bestPattern, bestCount);
                } else {
                  auto sent = tbot.sendMessage(chatId, msg);
                  if (sent) history.addMessage(chatId, sent->messageId, msg, false);
                }
              }
            } else {
              // Legacy batch mode
              if (sendBox.size() > configSender.minapprox) {
                auto grouped = sendBox.approximation(configSender.accuracy, configSender.spread, configSender.dont_approximate_multibyte);
                for (const std::string& g : grouped) tbot.send(atoll(chat.c_str()), g);
              } else {
                for (const std::string& m : messages) tbot.send(atoll(chat.c_str()), m);
              }
            }
          } catch (std::exception& e) {
            std::string err = "Sender exception: ";
            err += e.what();
            zbot::log.write(ZLogger::LogLevel::ERROR, err);
            sendBox.move(pendingStorage);
            continue;
          }
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

void zworker::removeFiles(std::vector<std::string> files) {
  for (std::string s : files) {
    if (s.compare(0, 5, "/tmp/") == 0) {
      unlink(s.c_str());
    }
  }
}