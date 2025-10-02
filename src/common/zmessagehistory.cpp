#include <zmessagehistory.h>
#include <algorithm>

void ZMessageHistory::addMessage(int64_t chatId, int32_t messageId, const std::string& text) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  
  // Check if we have too many chats and clean up if necessary
  if (chatHistories_.size() >= MAX_CHATS) {
    // Remove the oldest chat (by last message timestamp)
    auto oldestChat = chatHistories_.begin();
    auto oldestTime = std::chrono::system_clock::now();
    
    for (auto it = chatHistories_.begin(); it != chatHistories_.end(); ++it) {
      if (!it->second.empty()) {
        auto lastMsgTime = it->second.back().timestamp;
        if (lastMsgTime < oldestTime) {
          oldestTime = lastMsgTime;
          oldestChat = it;
        }
      }
    }
    
    if (oldestChat != chatHistories_.end()) {
      chatHistories_.erase(oldestChat);
    }
  }
  
  HistoryMessage msg;
  msg.messageId = messageId;
  msg.text = text;
  msg.timestamp = std::chrono::system_clock::now();
  msg.isGrouped = false;
  
  chatHistories_[chatId].push_back(msg);
  
  // Keep only recent messages to avoid memory bloat per chat
  auto& history = chatHistories_[chatId];
  if (history.size() > MAX_MESSAGES_PER_CHAT) {
    history.erase(history.begin(), history.begin() + (history.size() - MAX_MESSAGES_PER_CHAT));
  }
}

std::vector<HistoryMessage> ZMessageHistory::getRecentMessages(int64_t chatId, int count, int maxAgeMinutes) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  
  cleanOldMessages(chatId, maxAgeMinutes);
  
  std::vector<HistoryMessage> result;
  auto it = chatHistories_.find(chatId);
  if (it == chatHistories_.end()) {
    return result;
  }
  
  auto& history = it->second;
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(maxAgeMinutes);
  
  // Get recent messages that are not already grouped
  for (auto rit = history.rbegin(); rit != history.rend() && result.size() < count; ++rit) {
    if (!rit->isGrouped && (now - rit->timestamp) <= maxAge) {
      result.push_back(*rit);
    }
  }
  
  return result;
}

void ZMessageHistory::markAsGrouped(int64_t chatId, const std::vector<int32_t>& messageIds, int32_t groupMessageId) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  
  auto it = chatHistories_.find(chatId);
  if (it == chatHistories_.end()) {
    return;
  }
  
  auto& history = it->second;
  for (auto& msg : history) {
    if (std::find(messageIds.begin(), messageIds.end(), msg.messageId) != messageIds.end()) {
      msg.isGrouped = true;
    }
    // Update the group message with the list of grouped messages
    if (msg.messageId == groupMessageId) {
      msg.groupedMessageIds = messageIds;
    }
  }
}

void ZMessageHistory::removeMessages(int64_t chatId, const std::vector<int32_t>& messageIds) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  
  auto it = chatHistories_.find(chatId);
  if (it == chatHistories_.end()) {
    return;
  }
  
  auto& history = it->second;
  history.erase(
    std::remove_if(history.begin(), history.end(),
      [&messageIds](const HistoryMessage& msg) {
        return std::find(messageIds.begin(), messageIds.end(), msg.messageId) != messageIds.end();
      }),
    history.end()
  );
}

void ZMessageHistory::cleanOldMessages(int64_t chatId, int maxAgeMinutes) {
  auto it = chatHistories_.find(chatId);
  if (it == chatHistories_.end()) {
    return;
  }
  
  auto& history = it->second;
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(maxAgeMinutes);
  
  history.erase(
    std::remove_if(history.begin(), history.end(),
      [now, maxAge](const HistoryMessage& msg) {
        return (now - msg.timestamp) > maxAge;
      }),
    history.end()
  );
}

void ZMessageHistory::cleanInactiveChats(int maxAgeMinutes) {
  auto now = std::chrono::system_clock::now();
  auto maxAge = std::chrono::minutes(maxAgeMinutes);
  
  // Remove chats that have no messages or all messages are too old
  for (auto it = chatHistories_.begin(); it != chatHistories_.end();) {
    bool hasRecentMessages = false;
    
    for (const auto& msg : it->second) {
      if ((now - msg.timestamp) <= maxAge) {
        hasRecentMessages = true;
        break;
      }
    }
    
    if (!hasRecentMessages) {
      it = chatHistories_.erase(it);
    } else {
      ++it;
    }
  }
}

void ZMessageHistory::cleanup(int maxAgeMinutes) {
  std::lock_guard<std::mutex> lock(historyMutex_);
  
  // Clean old messages from all chats
  for (auto& pair : chatHistories_) {
    cleanOldMessages(pair.first, maxAgeMinutes);
  }
  
  // Remove completely inactive chats
  cleanInactiveChats(maxAgeMinutes);
}

ZMessageHistory::MemoryStats ZMessageHistory::getMemoryStats() const {
  std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(historyMutex_));
  
  MemoryStats stats;
  stats.totalChats = chatHistories_.size();
  stats.totalMessages = 0;
  stats.maxMessagesPerChat = 0;
  
  for (const auto& pair : chatHistories_) {
    size_t chatMessages = pair.second.size();
    stats.totalMessages += chatMessages;
    if (chatMessages > stats.maxMessagesPerChat) {
      stats.maxMessagesPerChat = chatMessages;
    }
  }
  
  stats.avgMessagesPerChat = stats.totalChats > 0 ? stats.totalMessages / stats.totalChats : 0;
  
  return stats;
}
