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
  std::string zbxSessionid_;
  std::string apiversion_;
  TgBot::Url zabbixjsonrpc_;
  TgBot::Url zabbixlogin_;
  TgBot::Url zabbixchart2_;

  long id_ = 0;
  void getSession();
  void getApiVersion();
  std::string sendRequest(boost::property_tree::ptree& pt);
  std::string generateRequest(TgBot::Url& url, const std::string& payload, std::string contentType,
                              bool isKeepAlive,
                              std::vector<std::string> cookies = std::vector<std::string>());
  std::string extractBody(const std::string& data) const;
  boost::property_tree::ptree parseJson(const std::string& json) const;

public:
  ZZabbix(const char* s, const char* u, const char* p)
      : zabbixjsonrpc_(s + std::string("api_jsonrpc.php")),
        zabbixlogin_(s + std::string("index.php")), zabbixchart2_(s + std::string("chart2.php")),
        user_(u), password_(p){};
  bool auth();
  std::vector<std::pair<std::string, std::string>> getMaintenances(int limit = 100);
  std::vector<std::pair<std::string, std::string>> getActions(int status, int limit = 100);
  std::vector<std::pair<std::string, std::string>> getProblems(int group, int limit = 100);
  std::vector<std::pair<std::string, std::string>> getScreens(int limit = 100);
  std::vector<std::pair<std::string, std::string>> getHostGrp(int filter = 0, int limit = 100);
  std::vector<std::string> getScreenGraphs(std::string id, int limit = 100);
  std::vector<std::string> downloadGraphs(std::vector<std::string> ids);
  std::string getMaintenanceName(std::string id);
  std::string getScreenName(std::string id);
  std::string getActionName(std::string id);
  std::string getHostGrpName(std::string id);
  std::string getEvent(std::string id);
  void createMaintenance(std::string id, std::string name);
  void ackProblem(std::string id, std::string message);
  void updateStatusAction(std::string id, int status);
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