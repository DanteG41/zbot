#include <map>
#include <string>

class ZConfig {
private:
  std::map<std::string, std::string> params_;

public:
  std::string configFile = "/etc/zbot/zbot.ini";

  ZConfig(){};
  ZConfig(const char* c) : configFile(c){};
  ZConfig(std::string s) : configFile(s){};

  void load(std::map<std::string, std::string>& p);
  void load(const char* sectionName, std::map<std::string, std::string>& p);
  void getParam(const char* name, int&);
  void getParam(const char* name, std::string&);
  void getParam(const char* name, float& val);
  void getParam(const char* name, bool&);
};

class ZConfigException {
private:
  std::string m_error;

public:
  ZConfigException(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};