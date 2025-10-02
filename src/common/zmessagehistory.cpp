#include <zmessagehistory.h>
#include <algorithm>

void ZMessageHistory::addMessage(int64_t chatId, int32_t messageId, const std::string& text,
                                 bool isGroup, const std::string& pattern, int count) {
  std::lock_guard<std::mutex> lk(mtx_);
  HistoryMessage hm;
  hm.chatId = chatId;
  hm.messageId = messageId;
  hm.text = text;
  hm.ts = std::chrono::system_clock::now();
  hm.isGroup = isGroup;
  hm.groupPattern = pattern;
  hm.groupCount = count;
  perChat_[chatId].push_back(std::move(hm));
}

std::vector<HistoryMessage> ZMessageHistory::getRecentMessages(int64_t chatId, int maxCount, int maxAgeMinutes) {
  std::lock_guard<std::mutex> lk(mtx_);
  std::vector<HistoryMessage> out;
  auto it = perChat_.find(chatId);
  if (it == perChat_.end()) return out;
  const auto& dq = it->second;
  auto now = std::chrono::system_clock::now();
  for (auto rit = dq.rbegin(); rit != dq.rend() && (int)out.size() < maxCount; ++rit) {
    auto ageMin = std::chrono::duration_cast<std::chrono::minutes>(now - rit->ts).count();
    if (ageMin > maxAgeMinutes) break;
    out.push_back(*rit);
  }
  return out;
}

void ZMessageHistory::removeMessages(int64_t chatId, const std::vector<int32_t>& ids) {
  std::lock_guard<std::mutex> lk(mtx_);
  auto it = perChat_.find(chatId);
  if (it == perChat_.end()) return;
  auto& dq = it->second;
  dq.erase(std::remove_if(dq.begin(), dq.end(), [&](const HistoryMessage& hm){
    return std::find(ids.begin(), ids.end(), hm.messageId) != ids.end();
  }), dq.end());
}

void ZMessageHistory::cleanup(int maxAgeMinutes) {
  std::lock_guard<std::mutex> lk(mtx_);
  auto now = std::chrono::system_clock::now();
  for (auto& kv : perChat_) {
    auto& dq = kv.second;
    dq.erase(std::remove_if(dq.begin(), dq.end(), [&](const HistoryMessage& hm){
      return std::chrono::duration_cast<std::chrono::minutes>(now - hm.ts).count() > maxAgeMinutes;
    }), dq.end());
  }
}


