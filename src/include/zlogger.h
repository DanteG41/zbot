#include <fstream>

class ZLogger {
private:
  std::ofstream logFile_;
  int logLevel_;
  std::string path_;

public:
  enum LogLevel { INFO, WARNING, ERROR };

  ZLogger() {};
  ZLogger(const char* c);
  ZLogger(std::string s);
  void operator<<(const char* c);
  void operator<<(std::string s);
  void write(LogLevel l, const char* c);
  void write(LogLevel l, std::string s);
  std::string formatting(LogLevel l, const char* c);
  void open();
  void open(const char* c);
  void open(std::string s);
  void close();
  void reopen();
};

class ZLoggerException {
private:
  std::string m_error;

public:
  ZLoggerException(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};