#include <signal.h>
#include <zinit.h>
#include <zzabbix.h>

namespace zworker {
int workerBot(sigset_t& sigset, siginfo_t& siginfo);
int workerSender(sigset_t& sigset, siginfo_t& siginfo);
void botGetParams(ZConfig& tc, ZConfig& zc, zbot::config& c);
TgBot::InlineKeyboardMarkup::Ptr createMenu(zbot::Menu menu);
void senderGetParams(ZConfig& tc, zbot::config& c);
} // namespace zworker