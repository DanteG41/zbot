#include <ztbot.h>
#include <algorithm>

void Ztbot::send(int64_t c, std::string m) {
  bot.getApi().sendMessage(c, m);
}

TgBot::Message::Ptr Ztbot::sendMessage(int64_t chatId, const std::string& message) {
  return bot.getApi().sendMessage(chatId, message);
}

std::vector<TelegramMessage> Ztbot::getChatHistory(int64_t chatId, int count, int maxAgeMinutes) {
  // NOTE: Telegram Bot API doesn't provide access to chat history
  // This method is a placeholder for potential future functionality
  // or integration with external message storage systems
  
  std::vector<TelegramMessage> result;
  // Always return empty - we rely on ZMessageHistory for tracking our sent messages
  return result;
}

bool Ztbot::deleteMessage(int64_t chatId, int32_t messageId) {
  try {
    bot.getApi().deleteMessage(chatId, messageId);
    return true;
  } catch (TgBot::TgException& e) {
    return false;
  }
}

bool Ztbot::deleteMessages(int64_t chatId, const std::vector<int32_t>& messageIds) {
  bool allDeleted = true;
  for (int32_t messageId : messageIds) {
    if (!deleteMessage(chatId, messageId)) {
      allDeleted = false;
    }
  }
  return allDeleted;
}