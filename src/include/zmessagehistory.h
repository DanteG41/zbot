#ifndef ZMESSAGEHISTORY_H
#define ZMESSAGEHISTORY_H

#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <mutex>
#include <ztbot.h>

struct HistoryMessage {
  int32_t messageId;
  std::string text;
  std::chrono::system_clock::time_point timestamp;
  bool isGrouped;
  std::vector<int32_t> groupedMessageIds; // For grouped messages
};

class ZMessageHistory {
private:
  std::map<int64_t, std::vector<HistoryMessage>> chatHistories_;
  std::mutex historyMutex_;
  
  // Configuration constants
  static const size_t MAX_MESSAGES_PER_CHAT = 100;
  static const size_t MAX_CHATS = 1000;  // Maximum number of chats to track
  
  void cleanOldMessages(int64_t chatId, int maxAgeMinutes);
  void cleanInactiveChats(int maxAgeMinutes);

public:
  ZMessageHistory() = default;
  
  // Add a sent message to history
  void addMessage(int64_t chatId, int32_t messageId, const std::string& text);
  
  // Get recent messages for grouping analysis
  std::vector<HistoryMessage> getRecentMessages(int64_t chatId, int count, int maxAgeMinutes);
  
  // Mark messages as grouped and store the group info
  void markAsGrouped(int64_t chatId, const std::vector<int32_t>& messageIds, int32_t groupMessageId);
  
  // Remove messages from history (when deleted from Telegram)
  void removeMessages(int64_t chatId, const std::vector<int32_t>& messageIds);
  
  // Clear old messages beyond the retention period
  void cleanup(int maxAgeMinutes);
  
  // Get memory usage statistics
  struct MemoryStats {
    size_t totalChats;
    size_t totalMessages;
    size_t maxMessagesPerChat;
    size_t avgMessagesPerChat;
  };
  MemoryStats getMemoryStats() const;
};

#endif // ZMESSAGEHISTORY_H
