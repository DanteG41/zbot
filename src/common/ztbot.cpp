#include <tgbot/TgTypeParser.h>
#include <zinit.h>
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
    return bot.getApi().sendMessage(c, html, nullptr, nullptr,
                                    std::make_shared<TgBot::GenericReply>(), "HTML");
  } catch (TgBot::TgException& e) {
    /* A message telegram cannot parse would fail on every retry and hold up the
    queue of the chat, so it goes out without the markup. */
    if (std::string(e.what()).find("parse entities") == std::string::npos) throw;
    return bot.getApi().sendMessage(c, fit(htmlToText(html)));
  }
}


int64_t zbot::dispatchUpdates(TgBot::Bot& bot, const std::string& answer, int64_t offset) {
  TgBot::TgTypeParser parser;
  boost::property_tree::ptree tree = parser.parseJson(answer);
  boost::property_tree::ptree empty;

  if (!tree.get<bool>("ok", false))
    throw TgBot::TgException(tree.get<std::string>("description", "getUpdates failed"),
                             TgBot::TgException::ErrorCode::Undefined);
  for (const boost::property_tree::ptree::value_type& item : tree.get_child("result", empty)) {
    int64_t id = item.second.get<int64_t>("update_id", 0);

    if (id >= offset) offset = id + 1;
    try {
      bot.getEventHandler().handleUpdate(parser.parseJsonAndGetUpdate(item.second));
    } catch (std::exception& e) {
      zbot::log.write(ZLogger::LogLevel::ERROR,
                      "zbotd: update " + std::to_string(id) + " skipped: " + e.what());
    }
  }
  return offset;
}

int64_t zbot::pollUpdates(TgBot::Bot& bot, const std::string& token, int64_t offset, int timeout) {
  TgBot::Url url("https://api.telegram.org/bot" + token + "/getUpdates");
  std::vector<TgBot::HttpReqArg> args;

  if (offset) args.emplace_back("offset", offset);
  args.emplace_back("limit", 100);
  args.emplace_back("timeout", timeout);
  return zbot::dispatchUpdates(bot, zbot::httpClient().makeRequest(url, args), offset);
}
