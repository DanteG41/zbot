#ifndef ZMSGBOX_H
#define ZMSGBOX_H
#include <list>
#include <string>
#include <vector>
#include <zstorage.h>

/* Message template helpers. A template is a message where the varying parts are
replaced with '?': either single digits inside a token or a whole token. */
float messageTokenDistance(const std::string& a, const std::string& b);
std::string mergeMessageTemplates(const std::string& a, const std::string& b,
                                  bool* multibyteChanged = nullptr);
bool templateMatchesMessage(const std::string& pattern, const std::string& text);

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;
  std::vector<std::string> messages_;
  std::vector<std::string> files_;
  std::string hex_string(int l);

public:
  ZMsgBox(ZStorage& s, const char* c);
  std::vector<std::string> approximation(float accuracy, float spread, bool dont_approximate_multibyte);
  std::vector<std::string> popMessages();
  void pushMessage(const char* c);
  void pushMessage(std::string s);
  void printMessage();
  void save();
  void load(int maxMessage);
  void move(ZStorage& s);
  void erase();
  int size();
};
#endif // ZMSGBOX_H