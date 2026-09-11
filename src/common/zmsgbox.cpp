#include <dirent.h>
#include <fstream>
#include <algorithm>
#include <cctype>
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

/* Message templates are built on whole tokens (runs of non-whitespace characters)
instead of single bytes. A template keeps every token that is common to the whole
group, replaces varying digits inside a token with '?' and collapses a token that
varies completely into a single '?' wildcard. Working on tokens keeps multi-byte
characters intact and never lets a template grow with every merged message. */

static const char* const wildcard = "?";

static bool isAsciiDigit(char c) { return c >= '0' and c <= '9'; }

static bool hasMultibyte(const std::string& token) {
  for (unsigned char c : token) {
    if (c & 0x80) return true;
  }
  return false;
}

/* Split a message into tokens and remember the whitespace preceding each of them,
so that a template can be rendered back with the original layout. */
static void splitTokens(const std::string& message, std::vector<std::string>& tokens,
                        std::vector<std::string>& separators) {
  size_t i = 0;
  while (i < message.size()) {
    std::string separator;
    while (i < message.size() and isspace(static_cast<unsigned char>(message[i])))
      separator.push_back(message[i++]);
    if (i == message.size()) break;
    std::string token;
    while (i < message.size() and !isspace(static_cast<unsigned char>(message[i])))
      token.push_back(message[i++]);
    separators.push_back(separator);
    tokens.push_back(token);
  }
}

static std::string joinTokens(const std::vector<std::string>& tokens,
                              const std::vector<std::string>& separators) {
  std::string result;
  for (size_t i = 0; i < tokens.size(); i++) {
    result += separators[i];
    result += tokens[i];
  }
  return result;
}

/* Merge two tokens. Identical tokens are kept as they are, tokens of the same
length that differ in digits only keep their constant part, everything else
becomes a wildcard. The wildcard flag tells the caller that the tokens have
nothing in common. */
static std::string mergeTokens(const std::string& a, const std::string& b, bool& wildcarded) {
  wildcarded = false;
  if (a == b) return a;
  if (a == wildcard or b == wildcard) return wildcard;
  if (a.size() != b.size()) {
    wildcarded = true;
    return wildcard;
  }
  std::string merged;
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i] == b[i]) {
      merged.push_back(a[i]);
      continue;
    }
    if (!(isAsciiDigit(a[i]) or a[i] == '?') or !(isAsciiDigit(b[i]) or b[i] == '?')) {
      wildcarded = true;
      return wildcard;
    }
    merged.push_back('?');
  }
  return merged;
}

static bool tokensCompatible(const std::string& a, const std::string& b) {
  bool wildcarded;
  mergeTokens(a, b, wildcarded);
  return !wildcarded;
}

/* Needleman-Wunsch alignment of two token sequences. Every insertion, deletion
and substitution costs one token. The edit operations are returned as '=' match,
'!' substitution, '-' deletion and '+' insertion. */
static int alignTokens(const std::vector<std::string>& a, const std::vector<std::string>& b,
                       std::string& operations) {
  size_t n = a.size(), m = b.size();
  std::vector<std::vector<int>> cost(n + 1, std::vector<int>(m + 1, 0));

  for (size_t i = 1; i <= n; i++) cost[i][0] = i;
  for (size_t j = 1; j <= m; j++) cost[0][j] = j;
  for (size_t i = 1; i <= n; i++) {
    for (size_t j = 1; j <= m; j++) {
      int substitution = cost[i - 1][j - 1] + (tokensCompatible(a[i - 1], b[j - 1]) ? 0 : 1);
      int deletion     = cost[i - 1][j] + 1;
      int insertion    = cost[i][j - 1] + 1;
      cost[i][j]       = std::min(substitution, std::min(deletion, insertion));
    }
  }

  operations.clear();
  size_t i = n, j = m;
  while (i > 0 or j > 0) {
    if (i > 0 and j > 0) {
      int substitution = cost[i - 1][j - 1] + (tokensCompatible(a[i - 1], b[j - 1]) ? 0 : 1);
      if (cost[i][j] == substitution) {
        operations.push_back(a[i - 1] == b[j - 1] ? '=' : '!');
        i--;
        j--;
        continue;
      }
    }
    if (i > 0 and cost[i][j] == cost[i - 1][j] + 1) {
      operations.push_back('-');
      i--;
      continue;
    }
    operations.push_back('+');
    j--;
  }
  std::reverse(operations.begin(), operations.end());
  return cost[n][m];
}

float messageTokenDistance(const std::string& a, const std::string& b) {
  std::vector<std::string> tokensA, separatorsA, tokensB, separatorsB;
  std::string operations;

  splitTokens(a, tokensA, separatorsA);
  splitTokens(b, tokensB, separatorsB);
  size_t length = std::max(tokensA.size(), tokensB.size());
  if (!length) return 0;
  return static_cast<float>(alignTokens(tokensA, tokensB, operations)) / length;
}

std::string mergeMessageTemplates(const std::string& a, const std::string& b,
                                  bool* multibyteChanged) {
  std::vector<std::string> tokensA, separatorsA, tokensB, separatorsB;
  std::vector<std::string> tokens, separators;
  std::string operations;

  if (multibyteChanged) *multibyteChanged = false;
  splitTokens(a, tokensA, separatorsA);
  splitTokens(b, tokensB, separatorsB);
  alignTokens(tokensA, tokensB, operations);

  size_t i = 0, j = 0;
  for (char operation : operations) {
    std::string token, separator;

    switch (operation) {
    case '=':
    case '!': {
      bool wildcarded = false;
      token           = mergeTokens(tokensA[i], tokensB[j], wildcarded);
      separator       = separatorsA[i];
      if (multibyteChanged and token != tokensA[i] and
          (hasMultibyte(tokensA[i]) or hasMultibyte(tokensB[j])))
        *multibyteChanged = true;
      i++;
      j++;
      break;
    }
    case '-':
      token     = wildcard;
      separator = separatorsA[i];
      if (multibyteChanged and hasMultibyte(tokensA[i])) *multibyteChanged = true;
      i++;
      break;
    default:
      token     = wildcard;
      separator = separatorsB[j];
      if (multibyteChanged and hasMultibyte(tokensB[j])) *multibyteChanged = true;
      j++;
      break;
    }

    /* Collapse neighbouring wildcards, otherwise a template would gain a token
    on every merged message. */
    if (token == wildcard and !tokens.empty() and tokens.back() == wildcard) continue;
    /* A token taken from the message keeps the whitespace it had there, but the
    leading whitespace belongs to the first token only. */
    if (!tokens.empty() and separator.empty()) separator = " ";
    tokens.push_back(token);
    separators.push_back(separator);
  }
  if (!tokens.empty())
    separators[0] = !separatorsA.empty()   ? separatorsA[0]
                    : !separatorsB.empty() ? separatorsB[0]
                                           : std::string();
  return joinTokens(tokens, separators);
}

/* A template token matches a message token when the constant part is the same
and every masked position holds a digit. A bare '?' stands for any number of
whole tokens, including none, because a wildcard also takes the place of a token
that one of the grouped messages does not have. */
static bool tokenMatches(const std::string& pattern, const std::string& token) {
  if (pattern.size() != token.size()) return false;
  for (size_t i = 0; i < pattern.size(); i++) {
    if (pattern[i] == token[i]) continue;
    if (pattern[i] != '?' or !isAsciiDigit(token[i])) return false;
  }
  return true;
}

bool templateMatchesMessage(const std::string& pattern, const std::string& text) {
  std::vector<std::string> patternTokens, patternSeparators, tokens, separators;

  splitTokens(pattern, patternTokens, patternSeparators);
  splitTokens(text, tokens, separators);

  size_t n = patternTokens.size(), m = tokens.size();
  std::vector<std::vector<bool>> matched(n + 1, std::vector<bool>(m + 1, false));
  matched[0][0] = true;
  for (size_t i = 1; i <= n; i++)
    matched[i][0] = matched[i - 1][0] and patternTokens[i - 1] == wildcard;
  for (size_t i = 1; i <= n; i++) {
    for (size_t j = 1; j <= m; j++) {
      if (patternTokens[i - 1] == wildcard)
        matched[i][j] = matched[i - 1][j - 1] or matched[i][j - 1] or matched[i - 1][j];
      else
        matched[i][j] = matched[i - 1][j - 1] and tokenMatches(patternTokens[i - 1], tokens[j - 1]);
    }
  }
  return matched[n][m];
}

static const char* const groupHeader = " similar messages received:\n";

std::string formatMessageGroup(const std::string& pattern, int count) {
  if (count < 2) return pattern;
  return std::to_string(count) + groupHeader + pattern;
}

bool parseMessageGroup(const std::string& text, std::string& pattern, int& count) {
  const std::string header = groupHeader;
  size_t digits            = 0;

  while (digits < text.size() and isdigit(static_cast<unsigned char>(text[digits]))) digits++;
  if (digits == 0 or digits > 9 or text.compare(digits, header.size(), header) != 0) {
    pattern = text;
    count   = 1;
    return false;
  }
  count   = std::stoi(text.substr(0, digits));
  pattern = text.substr(digits + header.size());
  return true;
}

std::vector<MessageGroup> ZMsgBox::grouping(float accuracy, float spread,
                                            bool dont_approximate_multibyte) {
  struct group {
    size_t representative; /* distances are measured against the first message of the
                           group, never against its template, so that a group cannot
                           drift away from the message it started with */
    std::string pattern;
    int count;
  };
  std::vector<group> groups;
  std::vector<MessageGroup> result;

  for (size_t m = 0; m < messages_.size(); m++) {
    const std::string& message = messages_[m];
    std::vector<size_t> candidates;
    std::vector<float> distances(groups.size(), 0);
    float bestDistance = accuracy;

    for (size_t g = 0; g < groups.size(); g++) {
      distances[g] = messageTokenDistance(message, messages_[groups[g].representative]);
      if (distances[g] >= accuracy) continue;
      candidates.push_back(g);
      if (distances[g] < bestDistance) bestDistance = distances[g];
    }

    /* Groups no further away than spread from the closest one are equally good,
    the biggest of them wins. */
    int chosen = -1;
    std::string chosenPattern;
    while (!candidates.empty()) {
      size_t best = candidates.size();
      for (size_t c = 0; c < candidates.size(); c++) {
        if (distances[candidates[c]] > bestDistance + spread) continue;
        if (best == candidates.size() or
            groups[candidates[c]].count > groups[candidates[best]].count)
          best = c;
      }
      if (best == candidates.size()) break;

      bool multibyteChanged = false;
      std::string pattern =
          mergeMessageTemplates(groups[candidates[best]].pattern, message, &multibyteChanged);
      if (dont_approximate_multibyte and multibyteChanged) {
        candidates.erase(candidates.begin() + best);
        continue;
      }
      chosen        = candidates[best];
      chosenPattern = pattern;
      break;
    }

    if (chosen < 0) {
      group newGroup;
      newGroup.representative = m;
      newGroup.pattern        = message;
      newGroup.count          = 1;
      groups.push_back(newGroup);
    } else {
      groups[chosen].pattern = chosenPattern;
      groups[chosen].count++;
    }
  }

  for (const group& g : groups) {
    MessageGroup out;
    /* A group of one is the message itself, an approximated template would only
    make it harder to read. */
    out.pattern = g.count > 1 ? g.pattern : messages_[g.representative];
    out.sample  = messages_[g.representative];
    out.count   = g.count;
    result.push_back(out);
  }
  return result;
}

std::vector<std::string> ZMsgBox::approximation(float accuracy, float spread,
                                                bool dont_approximate_multibyte) {
  std::vector<std::string> result;

  for (const MessageGroup& g : grouping(accuracy, spread, dont_approximate_multibyte))
    result.push_back(formatMessageGroup(g.pattern, g.count));
  return result;
}
