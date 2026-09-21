#ifndef ZTBOT_H
#define ZTBOT_H

#include <tgbot/tgbot.h>
#include <zhttpclient.h>

namespace zbot {
/* Hands every update of an answer of getUpdates to the bot and returns the offset
to ask for next time. An update the library cannot parse is logged and skipped:
telegram would hand out the same one on every poll and the bot would never move
past it. */
int64_t dispatchUpdates(TgBot::Bot& bot, const std::string& answer, int64_t offset);

/* One long poll of the telegram api. */
int64_t pollUpdates(TgBot::Bot& bot, const std::string& token, int64_t offset, int timeout);
} // namespace zbot

class Ztbot {
private:
  const char* token;
  TgBot::Bot bot;

public:
  Ztbot(const char* t) : bot(t, zbot::httpClient()){};
  Ztbot(std::string t) : bot(t.c_str(), zbot::httpClient()){};
  void send(int64_t c, std::string m);
  TgBot::Message::Ptr sendMessage(int64_t c, const std::string& m) {
    return bot.getApi().sendMessage(c, fit(m));
  }
  /* The text is html and has to be escaped and kept within the limit by the
  caller, cutting it here could break a tag or an entity. */
  TgBot::Message::Ptr sendHtml(int64_t c, const std::string& html);
  /* Telegram refuses a message longer than 4096 characters. Such a message would
  fail every time it is retried and would hold up everything queued behind it. */
  static std::string fit(const std::string& message) {
    const size_t limit = 4000;
    size_t cut         = limit;

    if (message.size() <= limit) return message;
    while (cut > 0 and (static_cast<unsigned char>(message[cut]) & 0xC0) == 0x80) cut--;
    return message.substr(0, cut) + "...";
  }
  bool deleteMessage(int64_t chatId, int32_t messageId) {
    try { bot.getApi().deleteMessage(chatId, messageId); return true; } catch (...) { return false; }
  }
  void deleteMessages(int64_t chatId, const std::vector<int32_t>& ids) {
    for (auto id : ids) { try { bot.getApi().deleteMessage(chatId, id); } catch (...) {} }
  }
};
#endif // ZTBOT_H