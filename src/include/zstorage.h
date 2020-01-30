#include <string>
#include <sys/stat.h>

class ZStorage {
private:
  std::string path_;
  struct stat stat_buf_;

  void checkDir();
  void createDir();
  int updateStat();

public:
  ZStorage(std::string c) : path_(c) { checkDir(); };
};

class ZStorageException {
private:
  std::string m_error;

public:
  ZStorageException(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};