#include <getopt.h>
#include <zinit.h>
#include <zmsgbox.h>
#include <ztbot.h>

void printHelp(const char* appname) {
  fprintf(stderr, "Usage: %s [-f configfile] -c chat -m message\n\
            -f\t\tconfiguration file path\n\
            -c\t\tchat recipient ID\n\
            -m\t\tmessage (max length 256 characters)\n\
            -i\t\timmediate sending\n",
          appname);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  int opt;
  const char* chat;
  bool immediately = false;
  ZConfig config, telegramConfig;
  std::string path, msg;

  while ((opt = getopt(argc, argv, "ihf:c:m:")) != -1) {
    switch (opt) {
    case 'f': {
      config.configFile         = optarg;
      telegramConfig.configFile = optarg;
      break;
    }
    case 'c': {
      chat = optarg;
      break;
    }
    case 'm': {
      msg = optarg;
      break;
    }
    case 'i': {
      immediately = true;
      break;
    }
    default:
      printHelp(argv[0]);
    }
  }
  zbot::init();
  if (!chat) {
    fprintf(stderr, "%s: requires -c option\n", argv[0]);
    printHelp(argv[0]);
    exit(EXIT_FAILURE);
  }
  if (msg.empty()) {
    fprintf(stderr, "%s: requires -m option\n", argv[0]);
    printHelp(argv[0]);
    exit(EXIT_FAILURE);
  }
  try {
    config.load(defaultconfig::params);
    config.getParam("storage", path);
    if (immediately) {
      std::string telegramToken;
      telegramConfig.load("telegram", defaultconfig::telegramParams);
      telegramConfig.getParam("token", telegramToken);
      Ztbot bot(telegramToken);
      bot.send(atoll(chat), msg);
    } else {
      ZStorage zbotStorage(path);
      ZStorage pendingStorage(path + "/pending");
      ZMsgBox pendingBox(pendingStorage, chat);
      pendingBox.pushMessage(msg);
      pendingBox.save();
    }
  } catch (ZStorageException& e) {
    fprintf(stderr, "%s\n", e.getError());
  } catch (TgBot::TgException& e) {
    fprintf(stderr, "TgBot exception: %s\n", e.what());
  }
  exit(EXIT_SUCCESS);
}