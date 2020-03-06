#ifndef ZSTORAGE_H
#define ZSTORAGE_H

#include <string>
#include <vector>
#include <sys/stat.h>

class ZStorage {
private:
  struct stat stat_buf_;

  void createDir();
  int updateStat();

protected:
  ZStorage() {};
  void checkDir();
  std::string path_;
  bool status = true;

public:
  bool checkTrigger();
  inline bool getStatus() { return status; };
  inline void disable() { status = false; };
  inline void enable() { status = true; };
  std::string getPath() { return path_; };
  std::vector<std::string> listChats();
  ZStorage(std::string c) : path_(c) { checkDir(); };
};

class ZStorageException {
private:
  std::string m_error;

public:
  ZStorageException(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};
#endif // ZSTORAGE_H