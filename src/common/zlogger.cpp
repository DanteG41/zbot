#include <zlogger.h>

ZLogger::ZLogger(const char* c) : path_(c){};
ZLogger::ZLogger(std::string s) : path_(s){};
void ZLogger::operator<<(const char* c) {
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
  logFile_ << formatting(LogLevel::INFO, c);
};
void ZLogger::operator<<(std::string s) {
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
  logFile_ << formatting(LogLevel::INFO, s.c_str());
};
void ZLogger::write(LogLevel l, const char* c) {
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
  logFile_ << formatting(l, c);
};
void ZLogger::write(LogLevel l, std::string s) {
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
  logFile_ << formatting(l, s.c_str());
};

std::string ZLogger::formatting(LogLevel l, const char* c) {
  std::string r, level;
  char timedate[100];
  time_t now = time(0);
  // char* dt   = ctime(&now);
  struct tm* p = localtime(&now);
  strftime(timedate, 100, "%b %d %T", p);

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
  logFile_.open(c, std::fstream::app);
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
};
void ZLogger::open(std::string s) {
  logFile_.open(s, std::fstream::app);
  if (!logFile_.is_open()) throw ZLoggerException("unable to open the log " + path_);
};
void ZLogger::close() { logFile_.close(); };
void ZLogger::reopen() {
  close();
  open();
};