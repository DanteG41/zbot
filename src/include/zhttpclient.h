#ifndef ZHTTPCLIENT_H
#define ZHTTPCLIENT_H

#include <string>
#include <tgbot/net/HttpClient.h>
#include <tgbot/net/HttpParser.h>
#include <vector>

/* The client shipped with the telegram library waits on the socket without any
deadline. A connection that stops answering without being closed, which is what a
broken link usually leaves behind, blocks the worker for good: no error, no log,
nothing sent. This client gives every step of a request a deadline. */
class ZHttpClient : public TgBot::HttpClient {
public:
  explicit ZHttpClient(int timeoutSeconds = 60) : timeout_(timeoutSeconds){};
  std::string makeRequest(const TgBot::Url& url,
                          const std::vector<TgBot::HttpReqArg>& args) const override;

private:
  int timeout_;
  TgBot::HttpParser parser_;
};

namespace zbot {
/* The client every bot of this process talks through. */
ZHttpClient& httpClient();
} // namespace zbot

#endif // ZHTTPCLIENT_H
