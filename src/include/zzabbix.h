#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <tgbot/net/Url.h>
#include <vector>

class ZZabbix {
private:
  mutable boost::asio::io_service ioService_;
  const char *user_, *password_, *server_;
  std::string authToken_;
  TgBot::Url url_;

  long id_ = 0;
  std::string sendRequest(boost::property_tree::ptree& pt);
  std::string generateRequest(const std::string& payload, bool isKeepAlive);
  std::string extractBody(const std::string& data) const;
  boost::property_tree::ptree parseJson(const std::string& json) const;

public:
  ZZabbix(const char* s, const char* u, const char* p) : url_(s), user_(u), password_(p){};
  std::string listMaintenance();
  bool auth();
  std::vector<std::pair<std::string, std::string>> getMaintenances(int limit = 100);
  std::vector<std::pair<std::string, std::string>> getHostGrp(int limit = 100);
  std::string getMaintenanceName(std::string id);
  void createMaintenance(std::string id, std::string name);
  void renewMaintenance(std::string id);
  void deleteMaintenance(std::string id);
};

class ZZabbixException {
private:
  std::string m_error;

public:
  ZZabbixException(std::string error) : m_error(error) {}
  const char* getError() { return m_error.c_str(); }
};