#include <iostream>
#include <sys/stat.h>
#include <zstorage.h>

void ZStorage::checkDir() {
  struct stat stat_buf;
  if (stat(path_.c_str(), &stat_buf) != 0) {
    if (errno == ENOENT) {
      createDir();
    } else {
      throw ZStorageException("could not read permissions of directory " + path_);
    }
  }
}

void ZStorage::createDir() {
  if (mkdir(path_.c_str(), 0750) != 0) {
    if (errno == EEXIST) {
      fprintf(stderr, "Dir %s exists. Error: %d\n", path_.c_str(), errno);
    } else {
      throw ZStorageException("cannot create directory " + path_);
    }
  }
}
