#ifndef ZMESSAGEHISTORY_H
#define ZMESSAGEHISTORY_H

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct HistoryMessage {
  int64_t chatId = 0;
  int32_t messageId = 0;
  std::string text;
  std::chrono::system_clock::time_point ts;
  bool isGroup = false;
  std::string groupPattern;
  int groupCount = 1;
};

class ZMessageHistory {
public:
  ZMessageHistory() = default;

  void addMessage(int64_t chatId, int32_t messageId, const std::string& text,
                  bool isGroup = false, const std::string& pattern = std::string(), int count = 1);

  std::vector<HistoryMessage> getRecentMessages(int64_t chatId, int maxCount, int maxAgeMinutes);

  void removeMessages(int64_t chatId, const std::vector<int32_t>& ids);

  void cleanup(int maxAgeMinutes);

private:
  std::mutex mtx_;
  std::unordered_map<int64_t, std::deque<HistoryMessage>> perChat_;
};

#endif // ZMESSAGEHISTORY_H


