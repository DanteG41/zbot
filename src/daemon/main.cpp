#include <cstring>
#include <zinit.h>

int main(int argc, char* argv[]) {
  zbot::progName = argv;
  zbot::argc = argc;
  std::string logPath;
  zbot::mainConfig.load(defaultconfig::params);
  zbot::mainConfig.getParam("log_file", logPath);
  try {
    zbot::log.open(logPath);
    zbot::log.close();
  } catch (ZLoggerException& e) {
    fprintf(stderr, "%s\n", e.getError());
    exit(EXIT_FAILURE);
  }
  zbot::zfork();
}
