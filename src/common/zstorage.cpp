#include <dirent.h>
#include <iostream>
#include <zstorage.h>

int ZStorage::updateStat() { return stat(path_.c_str(), &stat_buf_); }

bool ZStorage::checkTrigger() {
  std::string triggerFile = path_ + "/sending_off";
  struct stat st;

  /* Without the file sending is allowed. Reading st_mode when stat has failed
  would decide that on whatever the stack happens to hold. */
  if (stat(triggerFile.c_str(), &st) != 0) return true;
  return !S_ISREG(st.st_mode);
}

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
  if (stat_buf_.st_mode & (05007)) {
    throw ZStorageException("data directory \"" + path_ + "\" permissions should be u=rwx (2770)");
  }
}

void ZStorage::createDir() {
  if (mkdir(path_.c_str(), (S_ISGID | S_IRWXU | S_IRWXG)) != 0) {
    if (errno == EEXIST) {
      fprintf(stderr, "Dir %s exists. Error: %d\n", path_.c_str(), errno);
    } else {
      throw ZStorageException("cannot create directory " + path_);
    }
  }
  chmod(path_.c_str(), (S_ISGID | S_IRWXU | S_IRWXG));
  updateStat();
}

std::vector<std::string> ZStorage::listChats() {
  DIR* dirp = opendir(path_.c_str());
  struct dirent* dp;
  struct stat st;
  std::string fullpath;
  std::vector<std::string> result;

  if (dirp == NULL) throw ZStorageException("unable to read the directory " + path_);
  while ((dp = readdir(dirp)) != NULL) {
    std::string dir = dp->d_name;
    fullpath = path_ + "/" + dp->d_name;
    if (stat(fullpath.c_str(), &st) != 0) continue;
    if (S_ISDIR(st.st_mode) and dir != "." and dir != "..") {
      result.push_back(dir);
    }
  }
  closedir(dirp);
  return result;
};