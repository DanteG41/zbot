#ifndef ZSTORAGE_H
#define ZSTORAGE_H

#include <string>
#include <sys/stat.h>

class ZStorage {
private:
  struct stat stat_buf_;

  void checkDir();
  void createDir();
  int updateStat();

protected:
  ZStorage() {};
  std::string path_;

public:
  std::string getPath() { return path_; };
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