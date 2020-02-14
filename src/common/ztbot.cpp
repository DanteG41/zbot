#include <ztbot.h>

void Ztbot::send(int64_t c, std::string m) {
  bot.getApi().sendMessage(-c, m);
}