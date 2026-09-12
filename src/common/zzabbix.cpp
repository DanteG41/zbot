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

/* Markers for the json values that are not strings. */
static const char* const jsonTrue  = "__zbx_true__";
static const char* const jsonFalse = "__zbx_false__";

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
  try {
    boost::property_tree::read_json(input, tree);
  } catch (std::exception& e) {
    throw ZZabbixException(std::string("unable to parse the zabbix answer: ") + e.what());
  }
  return tree;
}

/* The answer of a json-rpc call holds either a result or an error object. Report
the error as a ZZabbixException instead of letting the property tree throw its own
exception, which no caller expects and which terminates the daemon. */
std::string ZZabbix::getErrorMessage(const ptree& response) const {
  std::string message = response.get<std::string>("error.message", "");
  std::string data    = response.get<std::string>("error.data", "");

  if (!data.empty()) message += message.empty() ? data : " " + data;
  return message.empty() ? "the answer holds no result" : message;
}

const ptree& ZZabbix::getResult(const ptree& response) const {
  boost::optional<const ptree&> result = response.get_child_optional("result");

  if (!result) throw ZZabbixException("zabbix: " + getErrorMessage(response));
  return *result;
}

static std::string urlEncode(const std::string& value) {
  static const char* hex = "0123456789ABCDEF";
  std::string encoded;

  for (unsigned char c : value) {
    if (isalnum(c) or c == '-' or c == '_' or c == '.' or c == '~') {
      encoded.push_back(c);
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[c >> 4]);
      encoded.push_back(hex[c & 0x0F]);
    }
  }
  return encoded;
}

static std::string statusLine(const std::string& response) {
  size_t end = response.find("\r\n");
  return end == std::string::npos ? response.substr(0, 64) : response.substr(0, end);
}

/* Collect the cookies the answer sets, skipping the ones it deletes. */
static std::vector<std::string> parseCookies(const std::string& response) {
  static const std::regex exp("Set-Cookie: *([^=;\r\n]+)=([^;\r\n]*)", std::regex::icase);
  std::vector<std::string> cookies;

  for (std::sregex_iterator it(response.begin(), response.end(), exp), last; it != last; ++it) {
    std::string name  = (*it)[1];
    std::string value = (*it)[2];

    if (value.empty() or value == "deleted") continue;
    cookies.push_back(name + "=" + value);
  }
  return cookies;
}

/* The web interface names the session cookie zbx_sessionid up to Zabbix 5.0 and
zbx_session in the newer versions, where the value is no longer a plain hex id. */
static std::string findSessionCookie(const std::vector<std::string>& cookies) {
  for (const std::string& cookie : cookies) {
    if (cookie.compare(0, 11, "zbx_session") == 0) return cookie;
  }
  return std::string();
}

static std::string cookieNames(const std::vector<std::string>& cookies) {
  std::string names;

  for (const std::string& cookie : cookies) {
    if (!names.empty()) names += ", ";
    names += cookie.substr(0, cookie.find('='));
  }
  return names.empty() ? "none" : names;
}

/* Read the value of a hidden form field, used for the csrf token that the login
form carries since Zabbix 6.4. */
static std::string findInputValue(const std::string& html, const std::string& name) {
  std::smatch sm;
  std::regex byName("name=[\"']" + name + "[\"'][^>]*value=[\"']([^\"']*)[\"']");
  std::regex byValue("value=[\"']([^\"']*)[\"'][^>]*name=[\"']" + name + "[\"']");

  if (std::regex_search(html, sm, byName)) return sm[1];
  if (std::regex_search(html, sm, byValue)) return sm[1];
  return std::string();
}

/* One TLS request to zabbix. The whole answer is returned, headers included, so
that the caller can look at the status line and at the cookies. */
std::string ZZabbix::sendWebRequest(TgBot::Url& url, const std::string& payload,
                                    const std::string& contentType,
                                    const std::vector<std::string>& cookies) {
  std::string response;

  try {
    tcp::resolver resolver(ioService_);
    tcp::resolver::query query(url.host, "443");
    ssl::context context(ssl::context::tlsv12_client);
    context.set_default_verify_paths();
    ssl::stream<tcp::socket> socket(ioService_, context);
    connect(socket.lowest_layer(), resolver.resolve(query));
    socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
    socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));
    socket.set_verify_mode(ssl::verify_none);
    socket.set_verify_callback(ssl::rfc2818_verification(url.host));
    socket.handshake(ssl::stream<tcp::socket>::client);

    std::string request = generateRequest(url, payload, contentType, false, cookies);
    write(socket, buffer(request.c_str(), request.length()));

    char buff[65536];
    boost::system::error_code error;
    while (!error) {
      size_t bytes = read(socket, buffer(buff), error);
      response += std::string(buff, bytes);
    }
  } catch (std::exception& e) {
    throw ZZabbixException("unable to reach zabbix on " + url.host + ": " + e.what());
  }
  return response;
}

/* boost::property_tree writes every value as a string, while the api wants real
json for the flags and for an empty array. Both are put into the tree as a marker
and turned into json here. */
std::string ZZabbix::toJson(ptree& pt) const {
  std::ostringstream buf;
  std::string json;

  write_json(buf, pt, false);
  json = buf.str();
  boost::replace_all(json, "\"" + std::string(jsonTrue) + "\"", "true");
  boost::replace_all(json, "\"" + std::string(jsonFalse) + "\"", "false");
  boost::replace_all(json, "[\"\"]", "[]");
  return json;
}

std::string ZZabbix::sendRequest(boost::property_tree::ptree& pt) {
  id_++;
  pt.put("jsonrpc", "2.0");
  pt.put("id", id_);
  if (!authToken_.empty()) pt.put("auth", authToken_);

  return ZZabbix::extractBody(sendWebRequest(zabbixjsonrpc_, toJson(pt), "application/json-rpc",
                                             std::vector<std::string>()));
}

/* The version reads as major.minor.patch, an api that is older than the asked
for version is reported as false. */
bool ZZabbix::apiAtLeast(int major, int minor) const {
  size_t dot = apiversion_.find('.');

  if (dot == std::string::npos) return false;
  try {
    int apiMajor = std::stoi(apiversion_.substr(0, dot));
    int apiMinor = std::stoi(apiversion_.substr(dot + 1));
    return apiMajor > major or (apiMajor == major and apiMinor >= minor);
  } catch (std::exception&) {
    return false;
  }
}

bool ZZabbix::auth() {
  ptree request, response;

  /* Zabbix renamed the login parameter from user to username in 5.4 and dropped
  the old name in 6.4, so ask with the current name and fall back to the old one
  when the server does not know it. */
  request.put("method", "user.login");
  request.put("params.username", user_);
  request.put("params.password", password_);
  response = ZZabbix::parseJson(sendRequest(request));

  if (!response.get_child_optional("result") and
      ZZabbix::getErrorMessage(response).find("username") != std::string::npos) {
    ptree legacy;
    legacy.put("method", "user.login");
    legacy.put("params.user", user_);
    legacy.put("params.password", password_);
    response = ZZabbix::parseJson(sendRequest(legacy));
  }

  authToken_ = ZZabbix::getResult(response).get_value<std::string>();

  /* Only the graph download needs a web session, so a failure here must not keep
  the bot from starting. The reason is kept for the caller to report. */
  sessionError_.clear();
  try {
    getSession();
  } catch (ZZabbixException& e) {
    sessionError_ = e.getError();
  }
  getApiVersion();

  return true;
}

void ZZabbix::getSession() {
  /* Since Zabbix 6.4 the login form carries a csrf token, and the form itself
  sets the cookie the token belongs to, so the page has to be fetched before the
  credentials can be posted. Older versions simply ignore the extra field. */
  std::vector<std::string> empty;
  std::string form                 = sendWebRequest(zabbixlogin_, "", "", empty);
  std::vector<std::string> cookies = parseCookies(form);
  std::string token                = findInputValue(form, "_csrf_token");

  std::string payload = "name=" + urlEncode(user_) + "&password=" + urlEncode(password_) +
                        "&enter=" + urlEncode("Sign in");

  if (!token.empty()) payload += "&_csrf_token=" + urlEncode(token);

  std::string answer =
      sendWebRequest(zabbixlogin_, payload, "application/x-www-form-urlencoded", cookies);
  std::vector<std::string> answerCookies = parseCookies(answer);
  std::string session                    = findSessionCookie(answerCookies);

  /* A successful login redirects to the frontend. It may keep the cookie the
  login page has set instead of sending a new one. */
  if (session.empty() and answer.find("\r\nLocation:") != std::string::npos)
    session = findSessionCookie(cookies);
  if (session.empty())
    throw ZZabbixException("unable to log in zabbix web interface, answer: " + statusLine(answer) +
                           ", cookies: " + cookieNames(answerCookies) +
                           (token.empty() ? ", no csrf token in the form" : ""));
  zbxSessionid_ = session;
}

void ZZabbix::getApiVersion() {
  ptree payload, params, child;

  /* apiinfo.version takes no parameters and no authentication, the empty array
  is written by toJson. */
  params.push_back(std::make_pair("", child));
  payload.put("method", "apiinfo.version");
  payload.add_child("params", params);
  id_++;
  payload.put("jsonrpc", "2.0");
  payload.put("id", id_);

  std::string response = sendWebRequest(zabbixjsonrpc_, toJson(payload), "application/json-rpc",
                                        std::vector<std::string>());
  ptree answer = ZZabbix::parseJson(ZZabbix::extractBody(response));
  apiversion_  = ZZabbix::getResult(answer).get_value<std::string>();
}

std::vector<std::string> ZZabbix::downloadGraphs(std::vector<std::string> ids) {
  std::time_t time = std::time(nullptr);
  std::vector<std::string> cookies;
  std::vector<std::string> result;

  if (zbxSessionid_.empty())
    throw ZZabbixException("no zabbix web session: " +
                           (sessionError_.empty() ? std::string("not logged in") : sessionError_));
  cookies.push_back(zbxSessionid_);

  for (std::string id : ids) {
    std::string filename = "/tmp/" + id + "_" + std::to_string(time) + ".png";
    zabbixchart2_.query  = "graphid=";
    zabbixchart2_.query += id;
    /* chart2.php takes the time range as from and to since Zabbix 3.4, the old
    period and isNow parameters are ignored. */
    zabbixchart2_.query += "&from=now-1h&to=now&width=500&height=100&legend=1";

    std::string response =
        sendWebRequest(zabbixchart2_, "", "application/x-www-form-urlencoded", cookies);
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
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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

  if (apiAtLeast(4, 0)) {
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
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
    return v.second.get<std::string>("name");
  }
  return std::string();
}

std::string ZZabbix::getScreenName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "screen.get");
  request.put("params.screenids", id);
  request.add_child("params.output", params);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
    return v.second.get<std::string>("name");
  }
  return std::string();
}

std::string ZZabbix::getActionName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "action.get");
  request.put("params.actionids", id);
  request.add_child("params.output", params);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
    return v.second.get<std::string>("name");
  }
  return std::string();
}

std::string ZZabbix::getHostGrpName(std::string id) {
  ptree request, response;
  ptree params, paramsChild;
  paramsChild.put_value("name");
  params.push_back(std::make_pair("", paramsChild));
  request.put("method", "hostgroup.get");
  request.put("params.groupids", id);
  request.add_child("params.output", params);
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
    return v.second.get<std::string>("name");
  }
  return std::string();
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
  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
    return v.second.get<std::string>("eventid");
  }
  return std::string();
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
  /* The flag was renamed in Zabbix 6.0 and has to be a json boolean. */
  request.put(apiAtLeast(6, 0) ? "params.with_monitored_hosts" : "params.monitored_hosts",
              jsonTrue);
  request.add_child("params.output", params);

  response = ZZabbix::parseJson(sendRequest(request));
  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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

  for (ptree::value_type const& v : ZZabbix::getResult(response)) {
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