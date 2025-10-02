#ifndef ZTBOT_H
#define ZTBOT_H

#include <tgbot/tgbot.h>

class Ztbot {
private:
  const char* token;
  TgBot::Bot bot;

public:
  Ztbot(const char* t) : bot(t){};
  Ztbot(std::string t) : bot(t.c_str()){};
  void send(int64_t c, std::string m);
  TgBot::Message::Ptr sendMessage(int64_t c, const std::string& m) { return bot.getApi().sendMessage(c, m); }
  bool deleteMessage(int64_t chatId, int32_t messageId) {
    try { bot.getApi().deleteMessage(chatId, messageId); return true; } catch (...) { return false; }
  }
  void deleteMessages(int64_t chatId, const std::vector<int32_t>& ids) {
    for (auto id : ids) { try { bot.getApi().deleteMessage(chatId, id); } catch (...) {} }
  }
};
#endif // ZTBOT_H