
#include <Windows.h>
#include <cpprest/ws_client.h>

#include <ctime>
#include <iostream>
#include <sstream>

#include "PluginApi128.h"
#include "lua.h"
using namespace std;
using namespace web;
using namespace web::websockets::client;

#include <iostream>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

LuaApi128 *lua{};
LoggingApi *logger{};

static const char *get_name() { return "Darktide Websockets"; }

// int setup_ws() {
//	websocket_client client;
//	client.connect(U("ws://localhost:8000")).then([]() {
//logger->info(get_name(), "Connected to server..."); }).wait();
//
//	websocket_outgoing_message msg;
//	msg.set_utf8_message("I am a UTF-8 string! (Or close enough...)");
//	client.send(msg).then([]() {
//		logger->info(get_name(), "Sent message...");
//		});
//
//	client.receive().then([](websocket_incoming_message msg) {
//		return msg.extract_string();
//		}).then([](std::string body) {
//			logger->info(get_name(), "Recieved message...");
//			logger->info(get_name(), body.c_str());
//
//			}).wait();
//
//     return 0;
// }

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// This message handler will be invoked once for each incoming message. It
// prints the message and then sends a copy of the message back to the server.
void on_message(client *c, websocketpp::connection_hdl hdl, message_ptr msg) {
  logger->info(get_name(), msg->get_payload().c_str());

  websocketpp::lib::error_code ec;

  c->send(hdl, msg->get_payload(), msg->get_opcode(), ec);
  if (ec) {
    logger->info(get_name(), ec.message().c_str());
  }
}

// Create a client endpoint
client c;

client::connection_ptr con;

int setup_ws() {
  std::string uri = "ws://localhost:8000";

  try {
    // Set logging to be pretty verbose (everything except message payloads)
    c.set_access_channels(websocketpp::log::alevel::all);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);

    // Initialize ASIO
    c.init_asio();

    // Register our message handler
    c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));

    websocketpp::lib::error_code ec;
    con = c.get_connection(uri, ec);
    if (ec) {
      logger->info(get_name(), ec.message().c_str());
      return 0;
    }

    // Note that connect here only requests a connection. No network messages
    // are exchanged until the event loop starts running in the next line.
    c.connect(con);

    // c.send(con, "hello", websocketpp::frame::opcode::text, ec);

    // Start the ASIO io_service run loop
    // this will cause a single connection to be made to the server. c.run()
    // will exit when this connection is closed.
    // c.poll();
  } catch (websocketpp::exception const &e) {
    logger->info(get_name(), e.what());
  }

  return 1;
}

static void setup_game(GetApiFunction get_engine_api) {
  lua = (LuaApi128 *)get_engine_api(LUA_API_ID);
  logger = (LoggingApi *)get_engine_api(LOGGING_API_ID);

  logger->info("Darktide Websockets", "aaaaaaaaaa");

  setup_ws();

  // MessageBoxA(NULL, "setup_game", "setup_game", 0);
}

static void loaded(GetApiFunction get_engine_api) {
  // MessageBoxA(NULL, "loaded", "loaded", 0);
}

static void update(float dt) {
  c.poll_one();
  //   MessageBoxA(NULL, "update", "update", 0);
}

static void shutdown() {
  websocketpp::lib::error_code ec;
  c.close(con, websocketpp::close::status::going_away, "closing", ec);
  c.poll();

  if (ec) {
    logger->info(get_name(), ec.message().c_str());
  }
  // MessageBoxA(NULL, "shutdown", "shutdown", 0);
}

extern "C" {
void *get_dynamic_plugin_api(unsigned api) {
  if (api == PLUGIN_API_ID) {
    // MessageBoxA(NULL, "get_dynamic_plugin_api", "get_dynamic_plugin_api", 0);
    static PluginApi128 api{};
    api.get_name = get_name;
    api.setup_game = setup_game;
    api.loaded = loaded;
    api.update_game = update;
    api.shutdown_game = shutdown;
    return &api;
  }
  return nullptr;
}

#if !defined STATIC_PLUGIN_LINKING
PLUGIN_DLLEXPORT void *get_plugin_api(unsigned api) {
  // MessageBoxA(NULL, "get_plugin_api", "get_plugin_api", 0);
  return get_dynamic_plugin_api(api);
}
#endif
}
