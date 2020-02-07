#ifndef ZMSGBOX_H
#define ZMSGBOX_H
#include <list>
#include <vector>
#include <zstorage.h>

class ZMsgBox : public ZStorage {
private:
  const char* chatName_;
  std::vector<std::string> messages_;
  std::string hex_string(int l);
  struct similar {
    double distance;
    std::string* storage;
    similar(std::string* s, double d) : storage(s), distance(d){};
  };

public:
  ZMsgBox(ZStorage& s, const char* c);
  std::vector<std::string> approximation(double d);
  void pushMessage(const char* c);
  void pushMessage(std::string s);
  void printMessage();
  void save();
  void load();
};
#endif // ZMSGBOX_H