#include <zmsgbox.h>

ZMsgBox::ZMsgBox(ZStorage& s, const char* c) : chatName_(c) {
  path_ = s.getPath() + "/" + chatName_;
  checkDir();
};

void ZMsgBox::pushMessage(const char* c) {
    messages_.push_back(c);
};
void ZMsgBox::pushMessage(std::string s) {
    messages_.push_back(s);
};

void ZMsgBox::save() {
    // Save messages to disk
};

void ZMsgBox::printMessage() {
    for (std::string s : messages_) {
        fprintf(stdout,"%s", s.c_str());
    }
}