#include <dirent.h>
#include <fstream>
#include <hirschberg.h>
#include <map>
#include <sys/stat.h>
#include <zmsgbox.h>

#include <iostream>

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

void ZMsgBox::save() {
  for (std::string s : messages_) {
    std::ofstream msgFile;
    msgFile.open(path_ + "/" + hex_string(16));
    msgFile << s;
    msgFile.close();
  }
};

void ZMsgBox::load() {
  DIR* dirp = opendir(path_.c_str());
  struct dirent* dp;
  struct stat st;
  std::string fullpath;

  while ((dp = readdir(dirp)) != NULL) {
    std::string message;
    char c;
    fullpath = path_ + "/" + dp->d_name;
    stat(fullpath.c_str(), &st);
    if (S_ISREG(st.st_mode)) {
      // std::cout << fullpath << "\n";
      std::ifstream msgFile;
      msgFile.open(fullpath.c_str());

      while (msgFile.get(c)) {
        message.push_back(c);
      }
      messages_.push_back(message);
    }
  }
  closedir(dirp);
};

void ZMsgBox::printMessage() {
  for (std::string s : messages_) {
    fprintf(stdout, "%s", s.c_str());
  }
}

std::pair<double, std::string> levensteinDistance(std::string& s, std::string& t) {
  double distance;
  std::string editOperations;

  editOperations = stringmetric::hirschberg(s.c_str(), t.c_str(), stringmetric::levenshtein);
  for (char c : editOperations) {
    if (c != '=') distance++;
  }
  return std::pair<double, std::string>(distance, editOperations);
}

std::vector<std::string> ZMsgBox::approximation(double d) {
  std::multimap<std::string*, ZMsgBox::similar> temp;
  std::pair<std::multimap<std::string*, ZMsgBox::similar>::iterator,
            std::multimap<std::string*, ZMsgBox::similar>::iterator>
      tempIter;
  std::vector<std::string> result;
  std::list<std::string*> keys;

  /* Group similar messages where the Levenstein distance
  less than the var d */

  for (std::string& k : messages_) {
    for (std::string& v : messages_) {
      std::pair<double, std::string> l;
      double dist;
      bool find = false;
      if (&k != &v) {
        l    = levensteinDistance(k, v);
        dist = l.first / std::min(k.size(), v.size());
        similar sim(&v, l.second, dist);
        if (dist < d) {
          if (temp.count(&k) == 0) {
            for (std::pair<std::string*, ZMsgBox::similar> p : temp) {
              if (p.second.storage == &k) find = 1;
            }
          }
          if (find != true) {
            temp.insert(std::pair<std::string*, ZMsgBox::similar>(&k, sim));
          }
        }
      }
    }
  }

  /* Fill array with messages that cannot be approximated.
  And collect unique keys. */
  for (std::string& s : messages_) {
    bool find = false;
    for (std::pair<std::string*, ZMsgBox::similar> m : temp) {
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
    tempIter                  = temp.equal_range(s);
    int maxLevensteinDistance = 0;
    int groupSize             = 1;
    std::string mostCommonPattern, resultMessage;

    for (std::multimap<std::string*, ZMsgBox::similar>::iterator it = tempIter.first;
         it != tempIter.second; ++it) {
      if (maxLevensteinDistance < it->second.distance) {
        maxLevensteinDistance = it->second.distance;
        mostCommonPattern     = it->second.pattern;
        groupSize++;
      }
    }

    int si         = 0;
    int unknownSeq = 0;

    for (int i = 0; i < mostCommonPattern.size(); i++) {
      switch (mostCommonPattern[i]) {
      case '=':
        resultMessage.push_back(s->at(si));
        si++;
        unknownSeq = 0;
        break;
      case '+':
        if (unknownSeq < 3) resultMessage.push_back('?');
        unknownSeq++;
        break;
      default:
        if (unknownSeq < 3) resultMessage.push_back('?');
        unknownSeq++;
        si++;
        break;
      }
    }
    result.push_back("Received " + std::to_string(groupSize) +
                     " similar messages: " + resultMessage);
  }
  return result;
}