#include <simpleini/SimpleIni.h>
#include <zconfig.h>

void ZConfig::load() {
  CSimpleIniA ini_file;
  ini_file.SetUnicode();
  ini_file.LoadFile(configFile);
  for (paramsIter_ = params_.begin(); paramsIter_ != params_.end(); paramsIter_++) {
    paramsIter_->second =
        ini_file.GetValue("zbot", paramsIter_->first.c_str(), paramsIter_->second.c_str());
  }
}

void ZConfig::getParam(const char *name, int &val) {
  paramsIter_ = params_.find(name);
  paramsIter_ == params_.end() ? throw ZConfigException("Param int not found")
                               : val = std::stoi(paramsIter_->second);
}
void ZConfig::getParam(const char *name, std::string &val) {
  paramsIter_ = params_.find(name);
  paramsIter_ == params_.end() ? throw ZConfigException("Param string not found")
                               : val = paramsIter_->second;
}