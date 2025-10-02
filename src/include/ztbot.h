#ifndef ZTBOT_H
#define ZTBOT_H

#include <tgbot/tgbot.h>

class Ztbot {
private:
  const char* token;
  TgBot::Bot bot;

public:
  Ztbot(const char* t) : bot(t){};
  Ztbot(std::string t) : bot(t.c_str()){};
  void send(int64_t c, std::string m);
};
#endif // ZTBOT_H