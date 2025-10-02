#ifndef ZINSTANTSENDER_H
#define ZINSTANTSENDER_H

#include <ztbot.h>
#include <zmessagehistory.h>
#include <zmsgbox.h>
#include <string>
#include <vector>

class ZInstantSender {
private:
  Ztbot& bot_;
  ZMessageHistory& history_;
  float accuracy_;
  float spread_;
  bool dontApproximateMultibyte_;
  int historyCheckCount_;
  int historyMaxAgeMinutes_;
  
  // Check if a message can be grouped with existing messages
  std::vector<HistoryMessage> findSimilarMessages(int64_t chatId, const std::string& newMessage);
  
  // Create a grouped message template
  std::string createGroupedMessage(const std::vector<std::string>& messages, int totalCount);

public:
  ZInstantSender(Ztbot& bot, ZMessageHistory& history, float accuracy, float spread, 
                 bool dontApproximateMultibyte, int historyCheckCount, int historyMaxAgeMinutes);
  
  // Send a message immediately, checking for grouping opportunities
  void sendMessage(int64_t chatId, const std::string& message);
};

#endif // ZINSTANTSENDER_H
