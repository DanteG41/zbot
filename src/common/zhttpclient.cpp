#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <zhttpclient.h>

using namespace boost::asio;
using boost::asio::ip::tcp;

std::string ZHttpClient::makeRequest(const TgBot::Url& url,
                                     const std::vector<TgBot::HttpReqArg>& args) const {
  io_context io;
  ssl::context context(ssl::context::tlsv12_client);
  ssl::stream<tcp::socket> socket(io, context);
  tcp::resolver resolver(io);
  tcp::resolver::results_type endpoints;
  std::string request = parser_.generateRequest(url, args, false);
  std::string response;
  boost::system::error_code result;

  context.set_default_verify_paths();
  socket.set_verify_mode(ssl::verify_none);
  socket.set_verify_callback(ssl::rfc2818_verification(url.host));

  /* Every step is started as an asynchronous operation and then waited for no
  longer than the timeout. A step that does not finish in time leaves the socket
  closed, so the caller gets an error instead of waiting forever. */
  auto wait = [&](const char* step, bool endOfStreamIsFine) {
    io.restart();
    io.run_for(std::chrono::seconds(timeout_));
    if (result == error::would_block) {
      boost::system::error_code ignored;
      socket.lowest_layer().close(ignored);
      io.restart();
      io.run();
      throw std::runtime_error(std::string("telegram: ") + step + " timed out after " +
                               std::to_string(timeout_) + " seconds");
    }
    if (!result) return;
    if (endOfStreamIsFine and (result == error::eof || result == ssl::error::stream_truncated))
      return;
    throw boost::system::system_error(result, step);
  };

  result = error::would_block;
  resolver.async_resolve(url.host, "443",
                         [&](const boost::system::error_code& e,
                             const tcp::resolver::results_type& found) {
                           result    = e;
                           endpoints = found;
                         });
  wait("name lookup", false);

  result = error::would_block;
  async_connect(socket.lowest_layer(), endpoints,
                [&](const boost::system::error_code& e, const tcp::endpoint&) { result = e; });
  wait("connect", false);

  socket.lowest_layer().set_option(socket_base::send_buffer_size(65536));
  socket.lowest_layer().set_option(socket_base::receive_buffer_size(65536));

  result = error::would_block;
  socket.async_handshake(ssl::stream<tcp::socket>::client,
                         [&](const boost::system::error_code& e) { result = e; });
  wait("handshake", false);

  result = error::would_block;
  async_write(socket, buffer(request),
              [&](const boost::system::error_code& e, std::size_t) { result = e; });
  wait("write", false);

  result = error::would_block;
  async_read(socket, dynamic_buffer(response),
             [&](const boost::system::error_code& e, std::size_t) { result = e; });
  wait("read", true);

  return parser_.extractBody(response);
}

ZHttpClient& zbot::httpClient() {
  static ZHttpClient client;
  return client;
}
