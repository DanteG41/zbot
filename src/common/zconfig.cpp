#include <simpleini/SimpleIni.h>
#include <zconfig.h>

void ZConfig::load(const char* sectionName, std::map<std::string, std::string>& p) {
  params_ = p;
  CSimpleIniA ini_file;
  ini_file.SetUnicode();
  ini_file.LoadFile(configFile);
  for (std::pair<const std::string, std::string> s : params_) {
    s.second = ini_file.GetValue(sectionName, s.first.c_str(), s.second.c_str());
  }
}

void ZConfig::load(std::map<std::string, std::string>& p) { load("main", p); }

void ZConfig::getParam(const char* name, int& val) {
  std::map<std::string, std::string>::iterator i = params_.find(name);
  i == params_.end() ? throw ZConfigException("Param int not found") : val = std::stoi(i->second);
}
void ZConfig::getParam(const char* name, std::string& val) {
  std::map<std::string, std::string>::iterator i = params_.find(name);
  i == params_.end() ? throw ZConfigException("Param string not found") : val = i->second;
}