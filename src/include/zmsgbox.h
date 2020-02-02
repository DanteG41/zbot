#include <list>
#include <zstorage.h>

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;
  std::list<std::string> messages_;
  std::string hex_string(int l);

public:
  ZMsgBox(ZStorage& s, const char* c);
  void pushMessage(const char* c);
  void pushMessage(std::string s);
  void printMessage();
  void save();
};