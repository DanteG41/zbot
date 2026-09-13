#include <ztbot.h>

void Ztbot::send(int64_t c, std::string m) {
  bot.getApi().sendMessage(c, Ztbot::fit(m));
}

/* The visible text of the html the sender builds: tags dropped, entities decoded. */
static std::string htmlToText(const std::string& html) {
  std::string text;
  bool tag = false;

  for (size_t i = 0; i < html.size(); i++) {
    if (tag) {
      if (html[i] == '>') tag = false;
    } else if (html[i] == '<') {
      tag = true;
    } else if (html.compare(i, 4, "&lt;") == 0) {
      text.push_back('<');
      i += 3;
    } else if (html.compare(i, 4, "&gt;") == 0) {
      text.push_back('>');
      i += 3;
    } else if (html.compare(i, 5, "&amp;") == 0) {
      text.push_back('&');
      i += 4;
    } else {
      text.push_back(html[i]);
    }
  }
  return text;
}

TgBot::Message::Ptr Ztbot::sendHtml(int64_t c, const std::string& html) {
  try {
    return bot.getApi().sendMessage(c, html, false, 0, std::make_shared<TgBot::GenericReply>(),
                                    "HTML");
  } catch (TgBot::TgException& e) {
    /* A message telegram cannot parse would fail on every retry and hold up the
    queue of the chat, so it goes out without the markup. */
    if (std::string(e.what()).find("parse entities") == std::string::npos) throw;
    return bot.getApi().sendMessage(c, fit(htmlToText(html)));
  }
}
