#include <zlogger.h>

ZLogger::ZLogger(const char* c) : path_(c){};
ZLogger::ZLogger(std::string s) : path_(s){};
void ZLogger::operator<<(const char* c) {
  if (!logFile_.is_open()) open();
  logFile_ << formatting(LogLevel::INFO, c);
  close();
};
void ZLogger::operator<<(std::string s) {
  if (!logFile_.is_open()) open();
  logFile_ << formatting(LogLevel::INFO, s.c_str());
  close();
};
void ZLogger::write(LogLevel l, const char* c) {
  if (!logFile_.is_open()) open();
  logFile_ << formatting(l, c);
  close();
};
void ZLogger::write(LogLevel l, std::string s) {
  if (!logFile_.is_open()) open();
  logFile_ << formatting(l, s.c_str());
  close();
};

std::string ZLogger::formatting(LogLevel l, const char* c) {
  std::string r, level;
  char timedate[100];
  time_t now = time(0);
  struct tm p;
  localtime_r(&now, &p);
  strftime(timedate, 100, "%b %d %T", &p);

  switch (l) {
  case LogLevel::WARNING: {
    level = " WARNING: ";
    break;
  }
  case LogLevel::ERROR: {
    level = " ERROR: ";
    break;
  }
  default:
    level = " INFO: ";
  }
  return r = timedate + level + c + "\n";
};

void ZLogger::open() { logFile_.open(path_, std::fstream::app); };
void ZLogger::open(const char* c) {
  path_ = c;
  logFile_.open(c, std::fstream::app);
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
};
void ZLogger::open(std::string s) {
  path_ = s;
  logFile_.open(s, std::fstream::app);
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
};
void ZLogger::close() { logFile_.close(); };
void ZLogger::reopen() {
  close();
  open();
};