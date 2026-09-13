#include <dirent.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>
#include <zmsgbox.h>

ZMsgBox::ZMsgBox(ZStorage& s, const char* c) : chatName_(c) {
  path_ = s.getPath() + "/" + chatName_;
  checkDir();
};

void ZMsgBox::pushMessage(const char* c) {
  messages_.push_back(c);
  times_.push_back(std::time(nullptr));
};
void ZMsgBox::pushMessage(std::string s) {
  messages_.push_back(s);
  times_.push_back(std::time(nullptr));
};

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
  struct queued {
    std::time_t time;
    std::string file, message;
  };
  std::vector<queued> found;

  if (dirp == NULL) throw ZStorageException("unable to read the directory " + path_);
  for (int i = 0; (dp = readdir(dirp)) != NULL and i < maxMessage; i++) {
    std::string message;
    char c;
    fullpath = path_ + "/" + dp->d_name;
    if (stat(fullpath.c_str(), &st) != 0) continue;
    if (S_ISREG(st.st_mode)) {
      if (std::string(dp->d_name) == "sending_off") continue;
      std::ifstream msgFile;
      msgFile.open(fullpath.c_str());
      while (msgFile.get(c)) {
        message.push_back(c);
      }
      found.push_back({st.st_mtime, dp->d_name, message}); /* the time it was queued */
    }
  }
  closedir(dirp);

  /* The files are named at random, so the directory order says nothing. Messages
  are taken in the order they came, which makes the first of a group its anchor
  and the last one the latest. */
  std::stable_sort(found.begin(), found.end(),
                   [](const queued& a, const queued& b) { return a.time < b.time; });
  for (const queued& q : found) {
    files_.push_back(q.file);
    messages_.push_back(q.message);
    times_.push_back(q.time);
  }
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
    /* A file that is already gone needs no deleting. */
    if (stat(fullpath.c_str(), &st) != 0) continue;
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

/* Message templates are built on whole tokens, the runs of characters between
whitespace. A token that is the same in every message of a group stays as it is.
A number that differs becomes '?'. A date or a time keeps its matching digits and
names each differing one after its field, as in 2026.09.1d/1h:mm:ss. A word that
differs, or that only some of the messages have, becomes '…', one mask for every
word, so that the template keeps the shape of the messages. */

static const char* const numberMask = "?";
static const char* const wordMask   = "\xE2\x80\xA6";

/* A group remembers at most this many of its messages for the list of values. */
static const size_t maxGroupMembers = 500;
/* A line of that list names at most this many values, each cut to this length. */
static const size_t maxListedValues = 10;
static const size_t maxValueLength  = 100;
/* Telegram refuses a message longer than 4096 characters. */
static const size_t messageLimit = 4000;
/* Aligning two messages takes time in proportion to the product of their lengths.
A longer message, a log dump for example, is never grouped: it would hold up the
sender and make an unreadable template anyway. */
static const size_t maxGroupedTokens = 200;

static bool isAsciiDigit(char c) { return c >= '0' and c <= '9'; }

/* Multibyte text in a token. The word mask is a multibyte character itself but no
text, otherwise merging with any template that has a mask would count as changing
multibyte text and be refused when dont_approximate_multibyte is set. */
static bool hasMultibyte(const std::string& token) {
  std::string text = token;
  for (size_t at; (at = text.find(wordMask)) != std::string::npos;) text.erase(at, strlen(wordMask));
  for (unsigned char c : text) {
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

/* Punctuation that sticks to a word from either side. It takes no part in the
comparison, so Resque and Resque: are the same word. A dot between digits is never
at the edge of a token, so a date is never mistaken for punctuation. */
static bool isEdgePunctuation(char c) {
  return c == '(' or c == ')' or c == '[' or c == ']' or c == '{' or c == '}' or c == ',' or
         c == ';' or c == ':' or c == '!' or c == '"' or c == '\'' or c == '.';
}

struct tokenParts {
  std::string lead, core, trail;
};

static tokenParts splitPunctuation(const std::string& token) {
  tokenParts parts;
  size_t begin = 0, end = token.size();

  while (begin < end and isEdgePunctuation(token[begin])) begin++;
  while (end > begin and isEdgePunctuation(token[end - 1])) end--;
  if (begin == end) {
    parts.core = token;
    return parts;
  }
  parts.lead  = token.substr(0, begin);
  parts.core  = token.substr(begin, end - begin);
  parts.trail = token.substr(end);
  return parts;
}

static bool isWordMask(const std::string& token) {
  return splitPunctuation(token).core == wordMask;
}

static bool isNumberChar(char c) { return isAsciiDigit(c) or c == '?'; }

/* A number is a run of digits with dots, colons or dashes between them, so 7.42,
17:36:11 and 2026.09.12 are single numbers. A slash splits, keeping the date and
the time of 2026.09.12/17:36:11 apart. The skeleton is the token with every number
replaced by a marker, two tokens with the same skeleton differ in numbers only. The
marker is a control character, since '#' and the like do occur in messages. */
static const char numberMarker = '\x01';

static std::string skeleton(const std::string& core, std::vector<std::string>* numbers) {
  std::string result;
  size_t i = 0;

  while (i < core.size()) {
    if (!isNumberChar(core[i])) {
      result.push_back(core[i++]);
      continue;
    }
    size_t begin = i;
    while (i < core.size() and isNumberChar(core[i])) i++;
    while (i + 1 < core.size() and (core[i] == '.' or core[i] == ':' or core[i] == '-') and
           isNumberChar(core[i + 1])) {
      i++;
      while (i < core.size() and isNumberChar(core[i])) i++;
    }
    if (numbers) numbers->push_back(core.substr(begin, i - begin));
    result.push_back(numberMarker);
  }
  return result;
}

/* For a date, a time or both, masked or not, the name of the field at every
position: yyyy.MM.dd/hh:mm:ss. An empty string for anything else. It runs for every
pair of tokens an alignment looks at, so it is written out rather than a regex. */
static bool fieldChar(char c, char field) { return isAsciiDigit(c) or c == field; }

static bool timeAt(const std::string& core, size_t at, std::string& mask) {
  size_t length = core.size() - at;

  if (length != 5 and length != 8) return false;
  if (!fieldChar(core[at], 'h') or !fieldChar(core[at + 1], 'h') or core[at + 2] != ':' or
      !fieldChar(core[at + 3], 'm') or !fieldChar(core[at + 4], 'm'))
    return false;
  mask += "hh:mm";
  if (length == 8) {
    if (core[at + 5] != ':' or !fieldChar(core[at + 6], 's') or !fieldChar(core[at + 7], 's'))
      return false;
    mask += ":ss";
  }
  return true;
}

static std::string fieldMask(const std::string& core) {
  std::string mask;

  if (core.size() >= 10 and fieldChar(core[0], 'y') and fieldChar(core[1], 'y') and
      fieldChar(core[2], 'y') and fieldChar(core[3], 'y') and (core[4] == '.' or core[4] == '-') and
      fieldChar(core[5], 'M') and fieldChar(core[6], 'M') and (core[7] == '.' or core[7] == '-') and
      fieldChar(core[8], 'd') and fieldChar(core[9], 'd')) {
    mask = std::string("yyyy") + core[4] + "MM" + core[7] + "dd";
    if (core.size() == 10) return mask;
    if (core[10] != '/' and core[10] != 'T') return std::string();
    mask.push_back(core[10]);
    return timeAt(core, 11, mask) ? mask : std::string();
  }
  return timeAt(core, 0, mask) ? mask : std::string();
}

/* Costs are counted in halves of a token. A word mask stands for any word, so
pairing it with one costs half of a real difference: that keeps the alignment from
pairing a mask with a word while a word that matches exactly is left substituted. */
enum tokenCost { costMatch = 0, costMask = 1, costChange = 2 };

/* Merge two tokens into the token of a template and tell what the difference cost.
The punctuation of the first token is kept. */
static std::string mergeTokens(const std::string& a, const std::string& b, int& cost) {
  tokenParts pa = splitPunctuation(a), pb = splitPunctuation(b);
  std::string lead  = pa.lead == pb.lead ? pa.lead : std::string();
  std::string trail = pa.trail == pb.trail ? pa.trail : std::string();

  if (pa.core == wordMask or pb.core == wordMask) {
    cost = pa.core == pb.core ? costMatch : costMask;
    return lead + wordMask + trail;
  }
  if (pa.core == pb.core) {
    cost = costMatch;
    return a;
  }

  std::string dateA = fieldMask(pa.core), dateB = fieldMask(pb.core);
  if (!dateA.empty() and dateA == dateB) {
    std::string merged = pa.core;
    for (size_t i = 0; i < merged.size(); i++)
      if (pa.core[i] != pb.core[i]) merged[i] = dateA[i];
    cost = costMatch;
    return pa.lead + merged + pa.trail;
  }

  std::vector<std::string> numbersA, numbersB;
  std::string shapeA = skeleton(pa.core, &numbersA), shapeB = skeleton(pb.core, &numbersB);
  if (!numbersA.empty() and shapeA == shapeB and numbersA.size() == numbersB.size()) {
    std::string merged;
    size_t n = 0;
    for (char c : shapeA) {
      if (c != numberMarker) {
        merged.push_back(c);
        continue;
      }
      merged += numbersA[n] == numbersB[n] ? numbersA[n] : numberMask;
      n++;
    }
    cost = costMatch;
    return pa.lead + merged + pa.trail;
  }

  cost = costChange;
  return lead + wordMask + trail;
}

static int alignmentCost(const std::string& a, const std::string& b) {
  int cost = costMatch;
  mergeTokens(a, b, cost);
  return cost;
}

/* Needleman-Wunsch alignment of two token sequences. The edit operations are
returned as '=' match, '!' substitution, '-' deletion and '+' insertion. */
static int alignTokens(const std::vector<std::string>& a, const std::vector<std::string>& b,
                       std::string& operations) {
  size_t n = a.size(), m = b.size();
  std::vector<std::vector<int>> cost(n + 1, std::vector<int>(m + 1, 0));
  std::vector<std::vector<int>> pair(n + 1, std::vector<int>(m + 1, 0));

  for (size_t i = 1; i <= n; i++) cost[i][0] = i * costChange;
  for (size_t j = 1; j <= m; j++) cost[0][j] = j * costChange;
  for (size_t i = 1; i <= n; i++) {
    for (size_t j = 1; j <= m; j++) {
      pair[i][j]       = alignmentCost(a[i - 1], b[j - 1]);
      int substitution = cost[i - 1][j - 1] + pair[i][j];
      int deletion     = cost[i - 1][j] + costChange;
      int insertion    = cost[i][j - 1] + costChange;
      cost[i][j]       = std::min(substitution, std::min(deletion, insertion));
    }
  }

  operations.clear();
  size_t i = n, j = m;
  while (i > 0 or j > 0) {
    if (i > 0 and j > 0) {
      int substitution = cost[i - 1][j - 1] + pair[i][j];
      if (cost[i][j] == substitution) {
        operations.push_back(a[i - 1] == b[j - 1] ? '=' : '!');
        i--;
        j--;
        continue;
      }
    }
    if (i > 0 and (j == 0 or cost[i][j] == cost[i - 1][j] + costChange)) {
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
  if (length > maxGroupedTokens) return 1;
  return static_cast<float>(alignTokens(tokensA, tokensB, operations)) / (costChange * length);
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
    bool touchesMultibyte = false;
    int cost              = costMatch;

    switch (operation) {
    case '=':
    case '!':
      token            = mergeTokens(tokensA[i], tokensB[j], cost);
      separator        = separatorsA[i];
      touchesMultibyte = token != tokensA[i] and
                         (hasMultibyte(tokensA[i]) or hasMultibyte(tokensB[j]));
      i++;
      j++;
      break;
    case '-':
      token            = wordMask;
      separator        = separatorsA[i];
      touchesMultibyte = hasMultibyte(tokensA[i]);
      i++;
      break;
    default:
      token            = wordMask;
      separator        = separatorsB[j];
      touchesMultibyte = hasMultibyte(tokensB[j]);
      j++;
      break;
    }
    if (multibyteChanged and touchesMultibyte) *multibyteChanged = true;

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

/* A template token matches a message token when the words are the same, when the
numbers that are not masked are the same, or when the digits of a date or a time
that are not named after their field are the same. */
static bool tokenMatches(const std::string& pattern, const std::string& token) {
  tokenParts pp = splitPunctuation(pattern), pt = splitPunctuation(token);

  if (pp.core == pt.core) return true;

  std::string date = fieldMask(pp.core);
  if (!date.empty() and date == fieldMask(pt.core)) {
    for (size_t i = 0; i < pp.core.size(); i++)
      if (pp.core[i] != pt.core[i] and pp.core[i] != date[i]) return false;
    return true;
  }

  std::vector<std::string> numbersP, numbersT;
  if (skeleton(pp.core, &numbersP) != skeleton(pt.core, &numbersT) or numbersP.empty() or
      numbersP.size() != numbersT.size())
    return false;
  for (size_t n = 0; n < numbersP.size(); n++)
    if (numbersP[n] != numberMask and numbersP[n] != numbersT[n]) return false;
  return true;
}

/* A word mask stands for one word or for none, because it also takes the place of
a word that some of the messages do not have. */
bool templateMatchesMessage(const std::string& pattern, const std::string& text) {
  std::vector<std::string> patternTokens, patternSeparators, tokens, separators;

  splitTokens(pattern, patternTokens, patternSeparators);
  splitTokens(text, tokens, separators);

  size_t n = patternTokens.size(), m = tokens.size();
  std::vector<std::vector<bool>> matched(n + 1, std::vector<bool>(m + 1, false));
  matched[0][0] = true;
  for (size_t i = 1; i <= n; i++)
    matched[i][0] = matched[i - 1][0] and isWordMask(patternTokens[i - 1]);
  for (size_t i = 1; i <= n; i++) {
    for (size_t j = 1; j <= m; j++) {
      if (isWordMask(patternTokens[i - 1]))
        matched[i][j] = matched[i - 1][j] or matched[i - 1][j - 1];
      else
        matched[i][j] = matched[i - 1][j - 1] and tokenMatches(patternTokens[i - 1], tokens[j - 1]);
    }
  }
  return matched[n][m];
}

void absorbMessageGroup(MessageGroup& group, const MessageGroup& older) {
  std::vector<std::string> members = older.members;

  members.insert(members.end(), group.members.begin(), group.members.end());
  if (members.size() > maxGroupMembers)
    members.erase(members.begin(), members.end() - maxGroupMembers);
  group.members = members;
  group.count += older.count;
  if (older.first and (!group.first or older.first < group.first)) group.first = older.first;
  if (older.last > group.last) group.last = older.last;
}

static std::string escapeHtml(const std::string& text) {
  std::string result;

  for (char c : text) {
    if (c == '&') result += "&amp;";
    else if (c == '<') result += "&lt;";
    else if (c == '>') result += "&gt;";
    else result.push_back(c);
  }
  return result;
}

/* Cut a text at a character boundary. */
static std::string cutText(const std::string& text, size_t limit) {
  if (text.size() <= limit) return text;
  size_t cut = limit;
  while (cut > 0 and (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) cut--;
  return text.substr(0, cut) + "...";
}

static std::string formatTime(std::time_t time, const char* format) {
  char buffer[32];
  struct tm local;

  localtime_r(&time, &local);
  strftime(buffer, sizeof buffer, format, &local);
  return buffer;
}

static std::string groupHeader(const MessageGroup& group) {
  std::string header = std::to_string(group.count) + " similar messages";

  if (group.first and group.last) {
    std::string firstDay = formatTime(group.first, "%Y.%m.%d");
    std::string lastDay  = formatTime(group.last, "%Y.%m.%d");
    std::string first    = firstDay + " " + formatTime(group.first, "%H:%M");
    std::string last     = formatTime(group.last, "%H:%M");

    if (first == lastDay + " " + last) header += " at " + first;
    else if (firstDay == lastDay) header += " from " + first + " to " + last;
    else header += " from " + first + " to " + lastDay + " " + last;
  }
  return header;
}

static bool isLink(const std::string& token) {
  std::string core = splitPunctuation(token).core;
  return core.compare(0, 7, "http://") == 0 or core.compare(0, 8, "https://") == 0;
}

/* A link with a mask in it leads nowhere, the link of the latest message is put in
its place. The template is aligned with that message, so every link is replaced by
the one at the same place even when another link of the template became a mask. */
static std::string linkLatest(const std::string& pattern, const std::string& latest) {
  std::vector<std::string> tokens, separators, latestTokens, latestSeparators;
  std::string operations;

  splitTokens(pattern, tokens, separators);
  splitTokens(latest, latestTokens, latestSeparators);
  alignTokens(tokens, latestTokens, operations);

  size_t i = 0, j = 0;
  for (char operation : operations) {
    if ((operation == '=' or operation == '!') and isLink(tokens[i]) and isLink(latestTokens[j]))
      tokens[i] = latestTokens[j];
    if (operation != '+') i++;
    if (operation != '-') j++;
  }
  return joinTokens(tokens, separators);
}

/* A literal token with a letter in it, the word a line of values is named after. */
static bool isNamingWord(const std::string& token) {
  tokenParts parts = splitPunctuation(token);

  if (parts.core.find(wordMask) != std::string::npos or parts.core.find('?') != std::string::npos)
    return false;
  if (!fieldMask(parts.core).empty()) return false;
  for (unsigned char c : parts.core)
    if (isalpha(c) or (c & 0x80)) return true;
  return false;
}

/* The values the members have behind every run of word masks. Every run gives a
line named after the word left of it, when there is a word, with the distinct
values in brackets. Runs named after the same word share a line. */
static std::vector<std::string> hiddenValues(const MessageGroup& group) {
  std::vector<std::string> tokens, separators;
  splitTokens(group.pattern, tokens, separators);

  std::vector<size_t> runOf(tokens.size(), SIZE_MAX), runs;
  for (size_t i = 0; i < tokens.size(); i++) {
    if (!isWordMask(tokens[i])) continue;
    runOf[i] = (i > 0 and runOf[i - 1] != SIZE_MAX) ? runOf[i - 1] : i;
    if (runOf[i] == i) runs.push_back(i);
  }
  if (runs.empty()) return std::vector<std::string>();

  std::vector<std::vector<std::string>> values(tokens.size());
  for (const std::string& member : group.members) {
    std::vector<std::string> words, wordSeparators;
    std::vector<std::string> phrase(tokens.size());
    std::string operations;
    size_t i = 0, j = 0, run = SIZE_MAX;

    splitTokens(member, words, wordSeparators);
    alignTokens(tokens, words, operations);
    for (char operation : operations) {
      if (operation != '+') run = runOf[i];
      if (operation != '-' and run != SIZE_MAX) {
        std::string word = words[j];
        word.erase(std::remove(word.begin(), word.end(), ','), word.end());
        if (!word.empty()) phrase[run] += (phrase[run].empty() ? "" : " ") + word;
      }
      if (operation != '+') i++;
      if (operation != '-') j++;
    }
    for (size_t r : runs)
      if (!phrase[r].empty() and
          std::find(values[r].begin(), values[r].end(), phrase[r]) == values[r].end())
        values[r].push_back(phrase[r]);
  }

  std::vector<std::string> names;
  std::vector<std::vector<std::string>> united;
  for (size_t r : runs) {
    if (values[r].empty()) continue;
    std::string name = (r > 0 and isNamingWord(tokens[r - 1])) ? splitPunctuation(tokens[r - 1]).core
                                                             : std::string();
    size_t line = names.size();
    for (size_t k = 0; k < names.size(); k++)
      if (!name.empty() and names[k] == name) line = k;
    if (line == names.size()) {
      names.push_back(name);
      united.push_back(std::vector<std::string>());
    }
    for (const std::string& value : values[r])
      if (std::find(united[line].begin(), united[line].end(), value) == united[line].end())
        united[line].push_back(value);
  }

  /* When the group has more messages than it remembers, the values of the older
  ones are not known and the count of the rest is only the least it can be. */
  bool complete = static_cast<size_t>(group.count) <= group.members.size();
  std::vector<std::string> lines;
  for (size_t k = 0; k < names.size(); k++) {
    std::string line = names[k].empty() ? "(" : names[k] + " (";
    for (size_t v = 0; v < united[k].size() and v < maxListedValues; v++)
      line += (v ? ", " : "") + cutText(united[k][v], maxValueLength);
    if (united[k].size() > maxListedValues)
      line += ", and " + std::to_string(united[k].size() - maxListedValues) + (complete ? "" : "+") +
              " more";
    line += ")";
    if (std::find(lines.begin(), lines.end(), line) == lines.end()) lines.push_back(line);
  }
  return lines;
}

std::string renderMessageGroup(const MessageGroup& group) {
  if (group.count < 2) return escapeHtml(cutText(group.sample, messageLimit));

  std::string latest  = group.members.empty() ? group.sample : group.members.back();
  std::string header  = groupHeader(group);
  std::string pattern = linkLatest(group.pattern, latest);
  std::vector<std::string> lines = hiddenValues(group);

  /* Keep the visible text within the limit: the list goes first, the template
  only when nothing else is left. */
  size_t size = header.size() + 1 + pattern.size();
  for (const std::string& line : lines) size += 1 + line.size();
  while (size > messageLimit and !lines.empty()) {
    size -= 1 + lines.back().size();
    lines.pop_back();
  }
  if (size > messageLimit) pattern = cutText(pattern, messageLimit - header.size() - 1);

  std::string html = escapeHtml(header) + "\n" + escapeHtml(pattern);
  if (!lines.empty()) {
    html += "\n<blockquote expandable>";
    for (size_t k = 0; k < lines.size(); k++) html += (k ? "\n" : "") + escapeHtml(lines[k]);
    html += "</blockquote>";
  }
  return html;
}

std::vector<MessageGroup> ZMsgBox::grouping(float accuracy, float spread,
                                            bool dont_approximate_multibyte) {
  struct group {
    size_t representative; /* distances are measured against the first message of the
                           group, never against its template, so that a group cannot
                           drift away from the message it started with */
    std::string pattern;
    std::vector<size_t> members;
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
            groups[candidates[c]].members.size() > groups[candidates[best]].members.size())
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
      newGroup.members.push_back(m);
      groups.push_back(newGroup);
    } else {
      groups[chosen].pattern = chosenPattern;
      groups[chosen].members.push_back(m);
    }
  }

  for (const group& g : groups) {
    MessageGroup out;
    out.count = g.members.size();
    /* A group of one is the message itself, a template would only make it harder
    to read. */
    out.pattern = out.count > 1 ? g.pattern : messages_[g.representative];
    out.sample  = messages_[g.representative];
    for (size_t m : g.members) {
      out.members.push_back(messages_[m]);
      std::time_t time = m < times_.size() ? times_[m] : 0;
      if (time and (!out.first or time < out.first)) out.first = time;
      if (time > out.last) out.last = time;
    }
    if (out.members.size() > maxGroupMembers)
      out.members.erase(out.members.begin(), out.members.end() - maxGroupMembers);
    result.push_back(out);
  }
  return result;
}
