#ifndef ZMSGBOX_H
#define ZMSGBOX_H
#include <list>
#include <string>
#include <vector>
#include <zstorage.h>

/* One group of similar messages: the template they share, one of the messages it
was built from and how many of them it stands for. The sample is what a later
group is compared against, because a template full of wildcards is close to
everything and would swallow unrelated messages. */
struct MessageGroup {
  std::string pattern;
  std::string sample;
  int count;
};

/* The text a group is sent as, and the way back from that text to the template
and the count. parseMessageGroup returns false for a message that is not a group,
leaving the whole text as the pattern and the count at one. */
std::string formatMessageGroup(const std::string& pattern, int count);
bool parseMessageGroup(const std::string& text, std::string& pattern, int& count);

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
  std::vector<std::string> approximation(float accuracy, float spread,
                                         bool dont_approximate_multibyte);
  std::vector<MessageGroup> grouping(float accuracy, float spread,
                                     bool dont_approximate_multibyte);
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