#include <iostream>
#include <unistd.h>
#include <zstorage.h>

int ZStorage::updateStat() { return stat(path_.c_str(), &stat_buf_); }

void ZStorage::checkDir() {
  if (updateStat() != 0) {
    if (errno == ENOENT) {
      createDir();
    } else {
      throw ZStorageException("could not read permissions of directory " + path_);
    }
  }
  if (!S_ISDIR(stat_buf_.st_mode)) {
    throw ZStorageException("specified data directory \"" + path_ + "\" is not a directory");
  }
  if (stat_buf_.st_uid != geteuid()) {
    throw ZStorageException("data directory \"" + path_ + "\" has wrong ownership");
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
  updateStat();
}
