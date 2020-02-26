#include <cstring>
#include <zinit.h>

void printHelp(const char* appname) {
  fprintf(stderr, "Usage: %s [configfile]\n", appname);
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  bool configErr;
  zbot::progName = argv;
  zbot::argc     = argc;
  std::string logPath;

  if (argc > 2) printHelp(argv[0]);
  if (argc == 2) {
    struct stat st;
    stat(argv[1], &st);

    if (S_ISREG(st.st_mode)) {
      zbot::mainConfig.configFile = argv[1];
    } else {
      configErr = true;
    }
  }

  zbot::mainConfig.load(defaultconfig::params);
  zbot::mainConfig.getParam("log_file", logPath);
  try {
    zbot::log.open(logPath);
    if (configErr)
      zbot::log.write(ZLogger::ERROR,
                      "Configuration file " + std::string(argv[1]) + " is not available.");
    zbot::log.close();
  } catch (ZLoggerException& e) {
    fprintf(stderr, "%s\n", e.getError());
    exit(EXIT_FAILURE);
  }
  zbot::zfork();
}
