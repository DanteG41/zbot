#include <boost/asio/ssl.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <set>
#include <zzabbix.h>

using namespace boost::asio;
using namespace boost::asio::ip;
using boost::property_tree::ptree;
using boost::property_tree::read_json;
using boost::property_tree::write_json;

std::string ZZabbix::generateRequest(const std::string& payload, bool isKeepAlive) {
  std::string result;
  if (payload.empty()) {
    result += "GET ";
  } else {
    result += "POST ";
  }
  result += url_.path;
  result += url_.query.empty() ? "" : "?" + url_.query;
  result += " HTTP/1.1\r\n";
  result += "Host: ";
  result += url_.host;
  result += "\r\nConnection: ";
  if (isKeepAlive) {
    result += "keep-alive";
  } else {
    result += "close";
  }
  result += "\r\n";
  if (payload.empty()) {
    result += "\r\n";
  } else {
    result += "Content-Type: application/json-rpc\r\n";
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
  tcp::resolver::query query(url_.host, "443");
  ssl::context context(ssl::context::tlsv12_client);
  context.set_default_verify_paths();
  ssl::stream<tcp::socket> socket(ioService_, context);
  connect(socket.lowest_layer(), resolver.resolve(query));
  socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
  socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(url_.host));
  socket.handshake(ssl::stream<tcp::socket>::client);

  std::string request = generateRequest(buf.str(), false);
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
  return true;
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

std::vector<std::pair<std::string, std::string>> ZZabbix::getHostGrp(int limit) {
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
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : response.get_child("result")) {
    const std::string& key = v.first;
    const ptree& subtree   = v.second;
    int activeSince, activeTill;
    std::string id, name;

    id = subtree.get<std::string>("groupid");
    name += subtree.get<std::string>("name");
    result.push_back(std::pair<std::string, std::string>(id, name));
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