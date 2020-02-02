#include <fstream>
#include <zmsgbox.h>

ZMsgBox::ZMsgBox(ZStorage& s, const char* c) : chatName_(c) {
  path_ = s.getPath() + "/" + chatName_;
  checkDir();
};

void ZMsgBox::pushMessage(const char* c) { messages_.push_back(c); };
void ZMsgBox::pushMessage(std::string s) { messages_.push_back(s); };

std::string ZMsgBox::hex_string(int l) {
  char hex_characters[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  int i;
  std::string str;

  for (i = 0; i < l; i++) {
    str.push_back(hex_characters[rand() % 16]);
  }
  return str;
}

void ZMsgBox::save() {
  for (std::string s : messages_) {
    std::ofstream msgFile;
    msgFile.open(path_ + "/" + hex_string(16));
    msgFile << s;
    msgFile.close();
  }
};

void ZMsgBox::printMessage() {
  for (std::string s : messages_) {
    fprintf(stdout, "%s", s.c_str());
  }
}