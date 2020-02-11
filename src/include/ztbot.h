#include <tgbot/tgbot.h>

class Ztbot {
private:
  const char* token;
  TgBot::Bot bot;

public:
  Ztbot(const char* t) : bot(t){};
  Ztbot(std::string t) : bot(t.c_str()){};
  void send(int c, std::string m);
};