#include <defaultconfig.h>
#include <getopt.h>
#include <zinit.h>
#include <zmsgbox.h>

void printHelp(const char* appname) {
  fprintf(stderr, "Usage: %s [-f configfile] -c chat -m message\n\
            -f\t\tconfiguration file path\n\
            -c\t\tchat recipient name or ID\n\
            -m\t\tmessage (max length 256 characters)\n",
          appname);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  int opt;
  const char* chat;
  ZConfig config;
  std::string path, msg;

  while ((opt = getopt(argc, argv, "hf:c:m:")) != -1) {
    switch (opt) {
    case 'f': {
      config.configFile = optarg;
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
  config.load(defaultconfig::params);
  config.getParam("storage", path);
  try {
    ZStorage zbotStorage(path);
    ZStorage processingStorage(path + "/processing");
    ZMsgBox sendBox(processingStorage, chat);
    sendBox.pushMessage(msg);
    sendBox.save();
  } catch (ZStorageException& e) {
    fprintf(stderr, "%s\n", e.getError());
  }
  exit(EXIT_SUCCESS);
}