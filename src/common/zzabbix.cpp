#include <boost/algorithm/string.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <regex>
#include <set>
#include <zzabbix.h>

using namespace boost::asio;
using namespace boost::asio::ip;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

std::string ZZabbix::generateRequest(TgBot::Url& url, const std::string& payload,
                                     std::string contentType, bool isKeepAlive,
                                     std::vector<std::string> cookies) {
  std::string result;
  if (payload.empty()) {
    result += "GET ";
  } else {
    result += "POST ";
  }
  result += url.path;
  result += url.query.empty() ? "" : "?" + url.query;
  result += " HTTP/1.1\r\n";
  result += "Host: ";
  result += url.host;
  result += "\r\nConnection: ";
  if (isKeepAlive) {
    result += "keep-alive";
  } else {
    result += "close";
  }
  result += "\r\n";
  if (!cookies.empty()) {
    result += "Cookie:";
    for (std::string s : cookies) {
      result += " ";
      result += s;
      result += ";";
    }
    result += "\r\n";
  }
  if (payload.empty()) {
    result += "\r\n";
  } else {
    result += "Content-Type: " + contentType + "\r\n";
    result += "Content-Length: ";
    result += std::to_string(payload.length());
    result += "\r\n\r\n";
    result += payload;
  }
  return result;
};

std::string ZZabbix::extractBody(const std::string& data) const {
  std::string body, header;
  size_t headerEnd = data.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    return data;
  }
  headerEnd += 4;
  header = data.substr(0, headerEnd);
  body   = data.substr(headerEnd);
  if (header.find("Transfer-Encoding: chunked") == std::string::npos) {
    return body;
  } else {
    std::string result;
    size_t ppos = 0;
    size_t cpos = body.find("\r\n");
    while (cpos != std::string::npos) {
      size_t lengthChunk = std::stoi(body.substr(ppos, cpos), 0, 16);
      if (lengthChunk == 0) return result;
      result += body.substr(cpos + 2, lengthChunk);
      ppos = cpos + lengthChunk + 2;
      cpos = body.find("\r\n", ppos);
    }
    return result;
  }
}

boost::property_tree::ptree ZZabbix::parseJson(const std::string& json) const {
  boost::property_tree::ptree tree;
  std::istringstream input(json);
  boost::property_tree::read_json(input, tree);
  return tree;
}

std::string ZZabbix::sendRequest(boost::property_tree::ptree& pt) {
  std::ostringstream buf;
  id_++;
  pt.put("jsonrpc", "2.0");
  pt.put("id", id_);
  if (!authToken_.empty()) pt.put("auth", authToken_);
  write_json(buf, pt, false);

  tcp::resolver resolver(ioService_);
  tcp::resolver::query query(zabbixjsonrpc_.host, "443");
  ssl::context context(ssl::context::tlsv12_client);
  context.set_default_verify_paths();
  ssl::stream<tcp::socket> socket(ioService_, context);
  connect(socket.lowest_layer(), resolver.resolve(query));
  socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
  socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(zabbixjsonrpc_.host));
  socket.handshake(ssl::stream<tcp::socket>::client);

  std::string request = generateRequest(zabbixjsonrpc_, buf.str(), "application/json-rpc", false);
  write(socket, buffer(request.c_str(), request.length()));

  std::string response;
  char buff[65536];
  boost::system::error_code error;
  while (!error) {
    size_t bytes = read(socket, buffer(buff), error);
    response += std::string(buff, bytes);
  }
  return ZZabbix::extractBody(response);
}

bool ZZabbix::auth() {
  ptree request, response;
  request.put("method", "user.login");
  request.put("params.user", user_);
  request.put("params.password", password_);

  response   = ZZabbix::parseJson(sendRequest(request));
  authToken_ = response.get<std::string>("result");

  getSession();
  getApiVersion();

  return true;
}

void ZZabbix::getSession() {
  std::string payload;
  payload += "name=";
  payload += user_;
  payload += "&password=";
  payload += password_;
  payload += "&enter=Sign+in";

  tcp::resolver resolver(ioService_);
  tcp::resolver::query query(zabbixlogin_.host, "443");
  ssl::context context(ssl::context::tlsv12_client);
  context.set_default_verify_paths();
  ssl::stream<tcp::socket> socket(ioService_, context);
  connect(socket.lowest_layer(), resolver.resolve(query));
  socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
  socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(zabbixlogin_.host));
  socket.handshake(ssl::stream<tcp::socket>::client);

  std::string request =
      generateRequest(zabbixlogin_, payload, "application/x-www-form-urlencoded", false);
  write(socket, buffer(request.c_str(), request.length()));

  std::string response;
  char buff[65536];
  boost::system::error_code error;
  while (!error) {
    size_t bytes = read(socket, buffer(buff), error);
    response += std::string(buff, bytes);
  }
  std::smatch sm;
  std::regex exp("(zbx_sessionid=[a-f0-9]{32})");
  if (std::regex_search(response, sm, exp)) {
    zbxSessionid_ = sm[0];
  } else {
    throw ZZabbixException("unable to log in zabbix");
  }
}

void ZZabbix::getApiVersion() {
  ptree payload, params, child;
  std::ostringstream buf;
  std::string sbuf;

  params.push_back(std::make_pair("", child));
  payload.put("method", "apiinfo.version");
  payload.add_child("params", params);

  // whole function is a dirty hack, because boost::property_tree::json_parser
  // cannot create an empty json array like {"params": []}

  id_++;
  payload.put("jsonrpc", "2.0");
  payload.put("id", id_);
  write_json(buf, payload, false);
  sbuf = buf.str();
  boost::replace_all(sbuf, "[\"\"]", "[]");

  tcp::resolver resolver(ioService_);
  tcp::resolver::query query(zabbixjsonrpc_.host, "443");
  ssl::context context(ssl::context::tlsv12_client);
  context.set_default_verify_paths();
  ssl::stream<tcp::socket> socket(ioService_, context);
  connect(socket.lowest_layer(), resolver.resolve(query));
  socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
  socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(zabbixjsonrpc_.host));
  socket.handshake(ssl::stream<tcp::socket>::client);

  std::string request = generateRequest(zabbixjsonrpc_, sbuf, "application/json-rpc", false);
  write(socket, buffer(request.c_str(), request.length()));

  std::string response;
  char buff[65536];
  boost::system::error_code error;
  while (!error) {
    size_t bytes = read(socket, buffer(buff), error);
    response += std::string(buff, bytes);
  }
  apiversion_ = ZZabbix::parseJson(ZZabbix::extractBody(response)).get<std::string>("result");
}

std::vector<std::string> ZZabbix::downloadGraphs(std::vector<std::string> ids) {
  std::time_t time = std::time(nullptr);
  std::vector<std::string> cookies;
  std::vector<std::string> result;
  cookies.push_back(zbxSessionid_);

  tcp::resolver resolver(ioService_);
  tcp::resolver::query query(zabbixlogin_.host, "443");
  ssl::context context(ssl::context::tlsv12_client);
  context.set_default_verify_paths();

  for (std::string id : ids) {
    ssl::stream<tcp::socket> socket(ioService_, context);
    connect(socket.lowest_layer(), resolver.resolve(query));
    socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
    socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
    socket.set_verify_mode(ssl::verify_none);
    socket.set_verify_callback(ssl::rfc2818_verification(zabbixlogin_.host));
    socket.handshake(ssl::stream<tcp::socket>::client);
    std::string filename = "/tmp/" + id + "_" + std::to_string(time) + ".png";
    zabbixchart2_.query  = "graphid=";
    zabbixchart2_.query += id;
    zabbixchart2_.query += "&period=3600&isNow=1&width=500&height=100&legend=1";

    std::string request =
        generateRequest(zabbixchart2_, "", "application/x-www-form-urlencoded", false, cookies);
    write(socket, buffer(request.c_str(), request.length()));

    std::string response;
    char buff[65536];
    boost::system::error_code error;
    while (!error) {
      size_t bytes = read(socket, buffer(buff), error);
      response += std::string(buff, bytes);
    }
    std::ofstream graphimg;
    graphimg.open(filename);
    graphimg << ZZabbix::extractBody(response);
    graphimg.close();
    result.push_back(filename);
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> ZZabbix::getMaintenances(int limit) {
  ptree request, response;
  ptree params;
  std::time_t time = std::time(nullptr);
  std::set<std::string> paramsOutput{"name", "maintenanceid", "active_since", "active_till"};
  std::vector<std::pair<std::string, std::string>> result;

  for (std::string s : paramsOutput) {
    ptree child;
    child.put_value(s);
    params.push_back(std::make_pair("", child));
  }
  request.put("method", "maintenance.get");
  request.put("params.sortfield", "name");
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    int activeSince, activeTill;
    std::string id, name;

    activeSince = subtree.get<int>("active_since");
    activeTill  = subtree.get<int>("active_till");
    id          = subtree.get<std::string>("maintenanceid");

    if (activeSince < time && time < activeTill) {
      name = "🛠 ";
    } else if (time < activeSince) {
      name = "⏰ ";
    } else {
      name = "❌ ";
    }
    name += subtree.get<std::string>("name");
    result.push_back(std::pair<std::string, std::string>(id, name));
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> ZZabbix::getActions(int status, int limit) {
  ptree request, response;
  ptree params;
  std::set<std::string> paramsOutput{"name", "actionid"};
  std::vector<std::pair<std::string, std::string>> result;

  for (std::string s : paramsOutput) {
    ptree child;
    child.put_value(s);
    params.push_back(std::make_pair("", child));
  }
  request.put("method", "action.get");
  request.put("params.sortfield", "name");
  request.add_child("params.output", params);
  request.put("params.filter.status", status);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    std::string id, name;

    id   = subtree.get<std::string>("actionid");
    name = subtree.get<std::string>("name");
    result.push_back(std::pair<std::string, std::string>(id, name));
  }
  return result;
}

std::vector<std::pair<std::string, std::string>> ZZabbix::getProblems(int group, int limit) {
  ptree request, response;
  ptree params;
  std::time_t time = std::time(nullptr);
  std::set<std::string> paramsOutput{"description", "triggerid"};
  std::vector<std::pair<std::string, std::string>> result;

  for (std::string s : paramsOutput) {
    ptree child;
    child.put_value(s);
    params.push_back(std::make_pair("", child));
  }
  request.put("method", "trigger.get");
  request.put("params.groupids", group);
  request.put("params.expandDescription", "1");
  // filter only active disaster
  request.put("params.min_severity", "5");
  request.put("params.monitored", "1");
  request.put("params.time_from", std::to_string(time - 1209600));
  request.put("params.filter.value", "1");
  request.put("params.withLastEventUnacknowledged", "1");
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    std::string id, name;

    id   = subtree.get<std::string>("triggerid");
    name = subtree.get<std::string>("description");
    result.push_back(std::pair<std::string, std::string>(id, name));
  }
  return result;
}

void ZZabbix::ackProblem(std::string id, std::string message) {
  ptree request, response;
  ptree groupids, timeperiods;
  ptree groupidChild, timeperiodChild;

  if (int(apiversion_[1]) >= 4) {
    /*  "params.action" is bitmask field, any combination of values is acceptable
        Possible values:
        1 - close problem;
        2 - acknowledge event;
        4 - add message;
        8 - change severity. */
    request.put("method", "event.acknowledge");
    request.put("params.action", 6);
    request.put("params.eventids", id);
    request.put("params.message", message);
  } else {
    request.put("method", "event.acknowledge");
    request.put("params.eventids", id);
    request.put("params.message", message);
  }

  response        = ZZabbix::parseJson(sendRequest(request));
  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }
}

void ZZabbix::updateStatusAction(std::string id, int status) {
  ptree request, response;

  request.put("method", "action.update");
  request.put("params.actionid", id);
  request.put("params.status", std::to_string(status));

  response        = ZZabbix::parseJson(sendRequest(request));
  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }
}

std::vector<std::pair<std::string, std::string>> ZZabbix::getScreens(int limit) {
  ptree request, response;
  ptree params;
  std::set<std::string> paramsOutput{"name", "screenid"};
  std::vector<std::pair<std::string, std::string>> result;

  for (std::string s : paramsOutput) {
    ptree child;
    child.put_value(s);
    params.push_back(std::make_pair("", child));
  }
  request.put("method", "screen.get");
  request.put("params.sortfield", "name");
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    std::string id, name;

    id   = subtree.get<std::string>("screenid");
    name = subtree.get<std::string>("name");
    result.push_back(std::pair<std::string, std::string>(id, name));
  }
  return result;
}

std::string ZZabbix::getMaintenanceName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "maintenance.get");
  request.put("params.maintenanceids", id);
  request.add_child("params.output", params);
  std::cout << sendRequest(request);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    return v.second.get<std::string>("name");
  }
}

std::string ZZabbix::getScreenName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "screen.get");
  request.put("params.screenids", id);
  request.add_child("params.output", params);
  std::cout << sendRequest(request);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    return v.second.get<std::string>("name");
  }
}

std::string ZZabbix::getActionName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "action.get");
  request.put("params.actionids", id);
  request.add_child("params.output", params);
  std::cout << sendRequest(request);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    return v.second.get<std::string>("name");
  }
}

std::string ZZabbix::getHostGrpName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "hostgroup.get");
  request.put("params.groupids", id);
  request.add_child("params.output", params);
  std::cout << sendRequest(request);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    return v.second.get<std::string>("name");
  }
}

std::string ZZabbix::getEvent(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("eventid");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "event.get");
  request.put("params.objectids", id);
  request.put("params.sortfield", "clock");
  request.put("params.sortorder", "DESC");
  request.add_child("params.output", params);
  std::cout << sendRequest(request);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    return v.second.get<std::string>("eventid");
  }
}

std::vector<std::pair<std::string, std::string>> ZZabbix::getHostGrp(int filter, int limit) {
  ptree request, response;
  ptree params;
  std::set<std::string> paramsOutput{"name", "groupid"};
  std::vector<std::pair<std::string, std::string>> result;

  for (std::string s : paramsOutput) {
    ptree child;
    child.put_value(s);
    params.push_back(std::make_pair("", child));
  }
  request.put("method", "hostgroup.get");
  request.put("params.sortfield", "name");
  request.put("params.monitored_hosts", "1");
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    int activeSince, activeTill;
    std::string id, name;

    id = subtree.get<std::string>("groupid");
    name += subtree.get<std::string>("name");
    if (filter == 1) {
      if (getProblems(std::stoi(id)).size() == 0) continue;
    }
    result.push_back(std::pair<std::string, std::string>(id, name));
  }
  return result;
}

std::vector<std::string> ZZabbix::getScreenGraphs(std::string id, int limit) {
  ptree request, response;
  ptree params, paramsChild;
  std::vector<std::string> result;

  paramsChild.put_value("resourceid");
  params.push_back(std::make_pair("", paramsChild));

  request.put("method", "screenitem.get");
  request.put("params.screenids", id);
  request.add_child("params.output", params);
  request.put("params.filter.resourcetype", 0);

  response = ZZabbix::parseJson(sendRequest(request));

  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }

  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    result.push_back(subtree.get<std::string>("resourceid"));
  }
  return result;
}

void ZZabbix::createMaintenance(std::string id, std::string name) {
  ptree request, response;
  ptree groupids, timeperiods;
  ptree groupidChild, timeperiodChild;
  std::time_t time = std::time(nullptr);

  groupidChild.put_value(id);
  groupids.push_back(std::make_pair("", groupidChild));

  timeperiodChild.put("timeperiod_type", 0);
  timeperiodChild.put("period", 3600);
  timeperiodChild.put("start_date", time);
  timeperiods.push_back(std::make_pair("", timeperiodChild));

  request.put("method", "maintenance.create");
  request.put("params.name", name);
  request.put("params.active_since", time);
  request.put("params.active_till", time + 86400);
  request.add_child("params.groupids", groupids);
  request.add_child("params.timeperiods", timeperiods);

  response        = ZZabbix::parseJson(sendRequest(request));
  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }
}

void ZZabbix::renewMaintenance(std::string id) {
  ptree request, response;
  ptree groupids, timeperiods;
  ptree groupidChild, timeperiodChild;
  std::time_t time = std::time(nullptr);

  timeperiodChild.put("timeperiod_type", 0);
  timeperiodChild.put("period", 3600);
  timeperiodChild.put("start_date", time);
  timeperiods.push_back(std::make_pair("", timeperiodChild));

  request.put("method", "maintenance.update");
  request.put("params.maintenanceid", id);
  request.put("params.active_since", time);
  request.put("params.active_till", time + 86400);
  request.add_child("params.timeperiods", timeperiods);

  response        = ZZabbix::parseJson(sendRequest(request));
  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }
}

void ZZabbix::deleteMaintenance(std::string id) {
  ptree request, response;
  ptree ids, idsChild;

  idsChild.put_value(id);
  ids.push_back(std::make_pair("", idsChild));

  request.put("method", "maintenance.delete");
  request.add_child("params", ids);

  response        = ZZabbix::parseJson(sendRequest(request));
  std::string err = response.get<std::string>("error.data", "");
  if (!err.empty()) {
    throw ZZabbixException(response.get<std::string>("error.data", ""));
  }
}