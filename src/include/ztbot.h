#ifndef ZTBOT_H
#define ZTBOT_H

#include <tgbot/tgbot.h>
#include <vector>
#include <chrono>

struct TelegramMessage {
  int32_t messageId;
  std::string text;
  std::chrono::system_clock::time_point timestamp;
  int64_t chatId;
};

class Ztbot {
private:
  const char* token;
  TgBot::Bot bot;

public:
  Ztbot(const char* t) : bot(t){};
  Ztbot(std::string t) : bot(t.c_str()){};
  void send(int64_t c, std::string m);
  TgBot::Message::Ptr sendMessage(int64_t chatId, const std::string& message);
  std::vector<TelegramMessage> getChatHistory(int64_t chatId, int count, int maxAgeMinutes);
  bool deleteMessage(int64_t chatId, int32_t messageId);
  bool deleteMessages(int64_t chatId, const std::vector<int32_t>& messageIds);
};
#endif // ZTBOT_H