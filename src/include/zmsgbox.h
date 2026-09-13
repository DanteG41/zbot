#ifndef ZMSGBOX_H
#define ZMSGBOX_H
#include <ctime>
#include <list>
#include <string>
#include <vector>
#include <zstorage.h>

/* One group of similar messages: the template they share, one of the messages it
was built from and how many of them it stands for. The sample is what a later
group is compared against, because a template full of masks is close to
everything and would swallow unrelated messages. The members, the most recent of
the messages, give the values hidden behind the masks, and first and last are the
times the first and the last of them arrived. */
struct MessageGroup {
  std::string pattern;
  std::string sample;
  int count = 1;
  std::vector<std::string> members;
  std::time_t first = 0;
  std::time_t last  = 0;
};

/* Takes an older group into this one: the counts add up, the time range widens and
the members of the older group go first, keeping only the most recent of them.
The template is merged by the caller. */
void absorbMessageGroup(MessageGroup& group, const MessageGroup& older);

/* The html text a group is sent as: a header with the count and the time range,
the template, and the values hidden behind its word masks in an expandable quote.
A group of one is the message itself. The visible text stays within the limit
of a telegram message. */
std::string renderMessageGroup(const MessageGroup& group);

/* Message template helpers. A template is a message where a differing number is
replaced with '?', the differing digits of a date or a time with the name of the
field and a differing word with '…'. */
float messageTokenDistance(const std::string& a, const std::string& b);
std::string mergeMessageTemplates(const std::string& a, const std::string& b,
                                  bool* multibyteChanged = nullptr);
bool templateMatchesMessage(const std::string& pattern, const std::string& text);

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;
  std::vector<std::string> messages_;
  std::vector<std::time_t> times_;
  std::vector<std::string> files_;
  std::string hex_string(int l);

public:
  ZMsgBox(ZStorage& s, const char* c);
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
