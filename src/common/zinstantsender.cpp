#include <zinstantsender.h>
#include <hirschberg.h>
#include <algorithm>

ZInstantSender::ZInstantSender(Ztbot& bot, ZMessageHistory& history, float accuracy, float spread, 
                               bool dontApproximateMultibyte, int historyCheckCount, int historyMaxAgeMinutes)
  : bot_(bot), history_(history), accuracy_(accuracy), spread_(spread), 
    dontApproximateMultibyte_(dontApproximateMultibyte), 
    historyCheckCount_(historyCheckCount), historyMaxAgeMinutes_(historyMaxAgeMinutes) {
}

std::vector<HistoryMessage> ZInstantSender::findSimilarMessages(int64_t chatId, const std::string& newMessage) {
  std::vector<HistoryMessage> recentMessages = history_.getRecentMessages(chatId, historyCheckCount_, historyMaxAgeMinutes_);
  std::vector<HistoryMessage> similarMessages;
  
  for (const auto& histMsg : recentMessages) {
    std::string msg1 = newMessage;
    std::string msg2 = histMsg.text;
    
    // Calculate Levenshtein distance
    float distance = 0;
    char* ops = stringmetric::hirschberg(msg1.c_str(), msg2.c_str());
    
    if (ops) {
      for (char* c = ops; *c != '\0'; c++) {
        if (*c != '=') distance++;
      }
      free(ops);
      
      float normalizedDistance = distance / msg1.size();
      
      if (normalizedDistance < accuracy_) {
        // Check multibyte handling if enabled
        if (dontApproximateMultibyte_) {
          char* editOps = stringmetric::hirschberg(msg1.c_str(), msg2.c_str());
          std::string editingOperations = editOps ? editOps : "";
          if (editOps) free(editOps);
          
          int si = 0;
          bool unchangedMultibyte = true;
          
          for (int i = 0; i < editingOperations.size(); i++) {
            switch (editingOperations[i]) {
            case '-':
            case '!':
              if ((msg1[si] & 0x80) != 0) unchangedMultibyte = false;
              si++;
              break;
            case '=':
              si++;
              break;
            case '+':
              break;
            }
            if (!unchangedMultibyte || si == msg1.size()) break;
          }
          
          if (unchangedMultibyte) {
            similarMessages.push_back(histMsg);
          }
        } else {
          similarMessages.push_back(histMsg);
        }
      }
    }
  }
  
  return similarMessages;
}

std::string ZInstantSender::createGroupedMessage(const std::vector<std::string>& messages, int totalCount) {
  if (messages.empty()) {
    return "";
  }
  
  if (messages.size() == 1) {
    return std::to_string(totalCount) + " similar messages were received:\n" + messages[0];
  }
  
  // Use the existing approximation logic from ZMsgBox
  ZMsgBox tempBox;
  for (const auto& msg : messages) {
    tempBox.pushMessage(msg);
  }
  
  std::vector<std::string> approximated = tempBox.approximation(accuracy_, spread_, dontApproximateMultibyte_);
  
  if (!approximated.empty()) {
    // Update the count in the approximated message
    std::string result = approximated[0];
    size_t pos = result.find(" similar messages were received:");
    if (pos != std::string::npos) {
      result = std::to_string(totalCount) + " similar messages were received:" + result.substr(pos + 32);
    }
    return result;
  }
  
  return std::to_string(totalCount) + " similar messages were received:\n" + messages[0];
}

void ZInstantSender::sendMessage(int64_t chatId, const std::string& message) {
  // Find similar messages in history
  std::vector<HistoryMessage> similarMessages = findSimilarMessages(chatId, message);
  
  // Debug logging
  std::cout << "[ZInstantSender] Processing message for chat " << chatId << std::endl;
  std::cout << "[ZInstantSender] Found " << similarMessages.size() << " similar messages" << std::endl;
  
  if (similarMessages.empty()) {
    // No similar messages found, send immediately
    std::cout << "[ZInstantSender] No similar messages, sending directly" << std::endl;
    TgBot::Message::Ptr sentMsg = bot_.sendMessage(chatId, message);
    if (sentMsg) {
      history_.addMessage(chatId, sentMsg->messageId, message);
      std::cout << "[ZInstantSender] Added message to history with ID " << sentMsg->messageId << std::endl;
    }
  } else {
    // Found similar messages, group them
    std::cout << "[ZInstantSender] Grouping messages!" << std::endl;
    std::vector<std::string> messagesToGroup;
    std::vector<int32_t> messageIdsToDelete;
    
    messagesToGroup.push_back(message); // Add the new message
    
    for (const auto& simMsg : similarMessages) {
      messagesToGroup.push_back(simMsg.text);
      messageIdsToDelete.push_back(simMsg.messageId);
      
      // If this message was already a group, add its grouped messages to deletion list
      if (!simMsg.groupedMessageIds.empty()) {
        messageIdsToDelete.insert(messageIdsToDelete.end(), 
                                 simMsg.groupedMessageIds.begin(), 
                                 simMsg.groupedMessageIds.end());
      }
    }
    
    // Create grouped message
    std::string groupedMessage = createGroupedMessage(messagesToGroup, messagesToGroup.size());
    
    // Delete old similar messages
    if (!messageIdsToDelete.empty()) {
      bot_.deleteMessages(chatId, messageIdsToDelete);
      history_.removeMessages(chatId, messageIdsToDelete);
    }
    
    // Send the new grouped message
    TgBot::Message::Ptr sentMsg = bot_.sendMessage(chatId, groupedMessage);
    if (sentMsg) {
      history_.addMessage(chatId, sentMsg->messageId, groupedMessage);
      // Mark the new message as a group containing the deleted messages
      history_.markAsGrouped(chatId, messageIdsToDelete, sentMsg->messageId);
    }
  }
}
