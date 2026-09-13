#ifndef ZMESSAGEHISTORY_H
#define ZMESSAGEHISTORY_H

#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <zmsgbox.h>

/* A message the bot has sent: the text as it went out and the group it was made
of, which is a group of one for a single message. */
struct HistoryMessage {
  int64_t chatId    = 0;
  int32_t messageId = 0;
  std::string text;
  std::chrono::system_clock::time_point ts;
  MessageGroup group;
};

class ZMessageHistory {
public:
  ZMessageHistory() = default;

  void addMessage(int64_t chatId, int32_t messageId, const std::string& text,
                  const MessageGroup& group);

  std::vector<HistoryMessage> getRecentMessages(int64_t chatId, int maxCount, int maxAgeMinutes);

  void removeMessages(int64_t chatId, const std::vector<int32_t>& ids);

  void cleanup(int maxAgeMinutes);

private:
  std::mutex mtx_;
  std::unordered_map<int64_t, std::deque<HistoryMessage>> perChat_;
};

#endif // ZMESSAGEHISTORY_H
