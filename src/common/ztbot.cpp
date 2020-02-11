#include <ztbot.h>

void Ztbot::send(int c, std::string m) {
  bot.getApi().sendMessage(-c, m);
}