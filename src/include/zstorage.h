#include <list>
#include <string>

class ZStorage {
private:
  std::string path_;

  void checkDir();
  void createDir();

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

class ZStorageExceptionNoEnt {
private:
  std::string m_error;

public:
  ZStorageExceptionNoEnt(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};