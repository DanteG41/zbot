#ifndef ZMSGBOX_H
#define ZMSGBOX_H
#include <list>
#include <vector>
#include <zstorage.h>

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;
  std::vector<std::string> messages_;
  std::vector<std::string> files_;
  std::string hex_string(int l);
  struct similar {
    float distance;
    std::string* storage;
    similar(std::string* s, float d) : storage(s), distance(d){};
  };

public:
  ZMsgBox() : ZStorage(""), chatName_("") {} // Default constructor for temporary use
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