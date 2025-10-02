#include <dirent.h>
#include <fstream>
#include <hirschberg.h>
#include <map>
#include <sys/stat.h>
#include <unistd.h>
#include <zmsgbox.h>

ZMsgBox::ZMsgBox(ZStorage& s, const char* c) : chatName_(c) {
  path_ = s.getPath() + "/" + chatName_;
  checkDir();
};

void ZMsgBox::pushMessage(const char* c) { messages_.push_back(c); };
void ZMsgBox::pushMessage(std::string s) { messages_.push_back(s); };

std::string ZMsgBox::hex_string(int l) {
  char hex_characters[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  int i;
  std::string str;

  for (i = 0; i < l; i++) {
    str.push_back(hex_characters[rand() % 16]);
  }
  return str;
}
#include <iostream>
void ZMsgBox::save() {
  for (std::string s : messages_) {
    std::ofstream msgFile;
    std::string file = path_ + "/" + hex_string(16);
    msgFile.open(file);
    if (!msgFile.is_open()) throw ZStorageException("unable to create a file " + file);
    msgFile << s;
    msgFile.close();
  }
};

void ZMsgBox::load(int maxMessage) {
  DIR* dirp = opendir(path_.c_str());
  struct dirent* dp;
  struct stat st;
  std::string fullpath;

  for (int i = 0; (dp = readdir(dirp)) != NULL and i < maxMessage; i++) {
    std::string message;
    char c;
    fullpath = path_ + "/" + dp->d_name;
    stat(fullpath.c_str(), &st);
    if (S_ISREG(st.st_mode)) {
      if (std::string(dp->d_name) == "sending_off") continue;
      std::ifstream msgFile;
      msgFile.open(fullpath.c_str());
      files_.push_back(dp->d_name);
      while (msgFile.get(c)) {
        message.push_back(c);
      }
      messages_.push_back(message);
    }
  }
  closedir(dirp);
};

void ZMsgBox::move(ZStorage& s) {
  ZMsgBox tmpBox(s, chatName_);
  for (std::string f : files_) {
    std::string oldfile, newfile;
    oldfile = path_ + "/" + f;
    newfile = tmpBox.getPath() + "/" + f;
    rename(oldfile.c_str(), newfile.c_str());
  }
  path_ = tmpBox.getPath();
};

void ZMsgBox::erase() {
  for (std::string f : files_) {
    struct stat st;
    std::string fullpath = path_ + "/" + f;
    stat(fullpath.c_str(), &st);
    if (S_ISREG(st.st_mode)) {
      unlink(fullpath.c_str());
    } else {
      throw ZStorageException("unable to delete the message file " + fullpath);
    }
  }
};

int ZMsgBox::size() { return messages_.size(); }

void ZMsgBox::printMessage() {
  for (std::string s : messages_) {
    fprintf(stdout, "%s", s.c_str());
  }
}

std::vector<std::string> ZMsgBox::popMessages() {
  if (status) {
    return messages_;
  } else {
    return std::vector<std::string>();
  }
};

float levensteinDistance(std::string& s, std::string& t) {
  float distance = 0;
  char *ops, *c;

  ops = stringmetric::hirschberg(s.c_str(), t.c_str());
  for (c = ops; *c != '\0'; c++) {
    if (*c != '=') distance++;
  }
  free(ops);
  return distance;
}

std::string levensteinOps(std::string& s, std::string& t) {
  char* ops;
  std::string editOperations;
  ops            = stringmetric::hirschberg(s.c_str(), t.c_str());
  editOperations = ops;

  free(ops);
  return editOperations;
}

std::vector<std::string> ZMsgBox::approximation(float accuracy, float spread,
                                                bool dont_approximate_multibyte) {
  std::multimap<std::string*, ZMsgBox::similar> similarmessages;
  std::pair<std::multimap<std::string*, ZMsgBox::similar>::iterator,
            std::multimap<std::string*, ZMsgBox::similar>::iterator>
      tempIter;
  std::vector<std::string> result;
  std::list<std::string*> keys;

  /* Group similar messages where the Levenstein distance
  less than the var accuracy */

  for (std::string& k : messages_) {
    for (std::string& v : messages_) {
      float dist;
      bool insert = false;
      bool find   = false;

      if (&k != &v) {
        dist = levensteinDistance(k, v) / k.size();
        similar sim(&v, dist);
        if (dist < accuracy) {
          if (!similarmessages.size()) insert = true;
          for (std::multimap<std::string*, ZMsgBox::similar>::iterator it = similarmessages.begin();
               it != similarmessages.end(); it++) {
            if ((it->first == &k and it->second.storage == &v) or
                (it->first == &v and it->second.storage == &k))
              continue;
            if (it->first == &v) {
              insert = false;
              break;
            }
            if (it->second.storage == &v) {
              if (it->second.distance - dist > spread) {
                it = similarmessages.erase(it);
                if (it == similarmessages.end()) break;
              } else
                find = true;
            }
            insert = true;
          }
          if (insert and !find)
            if (dont_approximate_multibyte) {
              std::string editingOperations;
              editingOperations        = levensteinOps(k, v);
              int si                   = 0;
              bool unchanged_multibyte = true;
              for (int i = 0; i < editingOperations.size(); i++) {
                switch (editingOperations[i]) {
                case '-':
                case '!':
                  if ((k[si] & 0x80) != 0) unchanged_multibyte = false;
                  si++;
                  break;
                case '=':
                  si++;
                  break;
                case '+':
                  break;
                }
                if (!unchanged_multibyte || si == k.size()) break;
              }
              if (unchanged_multibyte) {
                similarmessages.insert(std::pair<std::string*, ZMsgBox::similar>(&k, sim));
              }
            } else
              similarmessages.insert(std::pair<std::string*, ZMsgBox::similar>(&k, sim));
        }
      }
    }
  }

  // Remove the keys got into values.
  for (std::multimap<std::string*, ZMsgBox::similar>::iterator itk = similarmessages.begin();
       itk != similarmessages.end(); itk++) {
    bool remove = false;
    for (std::multimap<std::string*, ZMsgBox::similar>::iterator itv = similarmessages.begin();
         itv != similarmessages.end(); itv++) {
      if (itv->second.storage == itk->first) {
        if (itv->second.distance - itk->second.distance < spread &&
            itv->first != itk->second.storage) {
          std::string* skey;
          ZMsgBox::similar* kval;
          skey   = itv->first;
          kval   = &itk->second;
          itv    = similarmessages.erase(itv);
          remove = true;
          itv    = similarmessages.insert(std::pair<std::string*, ZMsgBox::similar>(skey, *kval));
        } else {
          itv = similarmessages.erase(itv);
          if (itv == similarmessages.end()) break;
        }
      }
    }
    if (remove) {
      itk = similarmessages.erase(itk);
      if (itk == similarmessages.end()) break;
    }
  }

  /* Fill array with messages that cannot be approximated.
  And collect unique keys. */
  for (std::string& s : messages_) {
    bool find = false;
    for (std::pair<std::string*, ZMsgBox::similar> m : similarmessages) {
      if (m.first == &s or m.second.storage == &s) {
        find = true;
        keys.push_back(m.first);
      }
    }
    if (!find) result.push_back(s);
  }

  // Fill with approximated messages.
  keys.sort();
  keys.unique();
  for (std::string* s : keys) {
    tempIter                  = similarmessages.equal_range(s);
    int maxLevensteinDistance = 0;
    int groupSize             = 1;
    std::string mostCommonPattern, prevPattern, editingOperations;
    mostCommonPattern = *s;

    for (std::multimap<std::string*, ZMsgBox::similar>::iterator it = tempIter.first;
         it != tempIter.second; ++it) {
      int unknownSeq = 0;
      int si         = 0;

      groupSize++;
      editingOperations = levensteinOps(mostCommonPattern, *it->second.storage);
      prevPattern       = mostCommonPattern;
      mostCommonPattern.erase();
      si = 0; // Reset string index for each iteration
      for (int i = 0; i < editingOperations.size(); i++) {
        if (si >= prevPattern.size()) break; // Prevent out of bounds access
        
        if ((prevPattern[si] & 0x80) == 0) { // lead bit is zero, must be a single ascii
          switch (editingOperations[i]) {
          case '-':
          case '!':
            mostCommonPattern.push_back('?');
            si++;
            break;
          case '=':
            mostCommonPattern.push_back(prevPattern[si]);
            si++;
            break;
          case '+':
            mostCommonPattern.push_back('?');
            // Don't increment si for insertions
            break;
          }
        } else if ((prevPattern[si] & 0xE0) == 0xC0) { // 110x xxxx 2 octets
          if (editingOperations[i] == '=' && editingOperations[i + 1] == '=') {
            mostCommonPattern.push_back(prevPattern[si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            si++;
            i++;
          } else if (editingOperations[i] == '!' || editingOperations[i + 1] == '!') {
            mostCommonPattern.append("? ");
            si += 2;
            i++;
          } else if (editingOperations[i] == '+' || editingOperations[i + 1] == '+') {
            mostCommonPattern.append("? ");
            // Don't increment si for insertions, only increment i
            i++;
          } else if (editingOperations[i] == '-' || editingOperations[i + 1] == '-') {
            mostCommonPattern.append("? ");
            si += 2;
            i++;
          }
        } else if ((prevPattern[si] & 0xF0) == 0xE0) { // 1110 xxxx 3 octets
          if (editingOperations[i] == '=' && editingOperations[i + 1] == '=' &&
              editingOperations[i + 2] == '=') {
            mostCommonPattern.push_back(prevPattern[si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            si++;
            i += 2;
          } else if (editingOperations[i] == '!' || editingOperations[i + 1] == '!' ||
                     editingOperations[i + 2] == '!') {
            mostCommonPattern.append("?  ");
            si += 3;
            i += 2;
          } else if (editingOperations[i] == '+' || editingOperations[i + 1] == '+' ||
                     editingOperations[i + 2] == '+') {
            mostCommonPattern.append("?  ");
            // Don't increment si for insertions
            i += 2;
          } else if (editingOperations[i] == '-' || editingOperations[i + 1] == '-' ||
                     editingOperations[i + 2] == '-') {
            mostCommonPattern.append("?  ");
            si += 3;
            i += 2;
          }
        } else if ((prevPattern[si] & 0xF8) == 0xF0) { // 1111 0xxx 4 octets
          if (editingOperations[i] == '=' && editingOperations[i + 1] == '=' &&
              editingOperations[i + 2] == '=' && editingOperations[i + 3] == '=') {
            mostCommonPattern.push_back(prevPattern[si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            mostCommonPattern.push_back(prevPattern[++si]);
            si++;
            i += 3;
          } else if (editingOperations[i] == '!' || editingOperations[i + 1] == '!' ||
                     editingOperations[i + 2] == '!' || editingOperations[i + 3] == '!') {
            mostCommonPattern.append("?   ");
            si += 4;
            i += 3;
          } else if (editingOperations[i] == '+' || editingOperations[i + 1] == '+' ||
                     editingOperations[i + 2] == '+' || editingOperations[i + 3] == '+') {
            mostCommonPattern.append("?   ");
            // Don't increment si for insertions
            i += 3;
          } else if (editingOperations[i] == '-' || editingOperations[i + 1] == '-' ||
                     editingOperations[i + 2] == '-' || editingOperations[i + 3] == '-') {
            mostCommonPattern.append("?   ");
            si += 4;
            i += 3;
          }
        }
      }
    }

    result.push_back(std::to_string(groupSize) + " similar messages were received:\n" +
                     mostCommonPattern);
  }
  return result;
}