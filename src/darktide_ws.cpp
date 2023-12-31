
#include <cpprest/ws_client.h>
#include <cpprest/json.h>
#include <cpprest/asyncrt_utils.h>
#include <AtlBase.h>
#include <atlconv.h>
#include <Windows.h>
#include <ctime>
#include <iostream>
#include <sstream>

#include "PluginApi128.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include <iostream>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

using namespace std;
using namespace web;
using namespace utility;
using namespace web::websockets::client;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

LuaApi128 *lua{};
LoggingApi *logger{};

client c;
client::connection_ptr con;

static const char *get_name() { return "Darktide Websockets"; }

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

void on_message(client *c, websocketpp::connection_hdl hdl, message_ptr msg) {
  std::string message = msg->get_payload().c_str();
  logger->info(get_name(), message.c_str());

  lua_State* L = lua->getscriptenvironmentstate();
  lua->getfield(L, LUA_GLOBALSINDEX, "_darktide_ws_callback"); // get global function
  lua->pushstring(L, message.c_str()); // push 1st argument
  lua->call(L, 1, 0); // call with 1 arg, 0 results
}

static context_ptr on_tls_init() {
	// establishes a SSL connection
	context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

	try {
		ctx->set_options(boost::asio::ssl::context::default_workarounds |
			boost::asio::ssl::context::no_sslv2 |
			boost::asio::ssl::context::no_sslv3 |
			boost::asio::ssl::context::single_dh_use);
	}
	catch (std::exception& e) {
		logger->info(get_name(), e.what());
	}
	return ctx;
}

void on_open(websocketpp::connection_hdl hdl) {
	logger->info(get_name(), "Websocket connection open");
}

void on_close(websocketpp::connection_hdl hdl) {
	logger->info(get_name(), "Websocket connection closed");
}

int setup_ws() {
  std::string uri = "wss://ws.darkti.de";

  try {
    // Set logging to be pretty verbose (everything except message payloads)
    c.set_access_channels(websocketpp::log::alevel::all);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);

    c.init_asio();
	c.set_tls_init_handler(bind(&on_tls_init));
    c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));
	c.set_open_handler(on_open);
	c.set_close_handler(on_close);

    websocketpp::lib::error_code ec;
    con = c.get_connection(uri, ec);
    if (ec) {
      logger->info(get_name(), ec.message().c_str());
      return 0;
    }

    // Note that connect here only requests a connection. No network messages
    // are exchanged until the event loop starts running 
    c.connect(con);
  } catch (websocketpp::exception const &e) {
    logger->info(get_name(), e.what());
  }

  return 1;
}

static int l_join_room(lua_State* L) {
	if (lua->isstring(L, 1)) {
		std::string room = lua->tolstring(L, 1, nullptr);

		CA2W ca2w(room.c_str());
		wstring room_wstr = ca2w;

		json::value message_json = json::value::object();
		message_json[L"type"] = json::value::string(U("join"));
		message_json[L"room"] = json::value::string(room_wstr);


		std::string message = utility::conversions::to_utf8string(message_json.serialize());

		websocketpp::lib::error_code ec;
		c.send(con, message, websocketpp::frame::opcode::text, ec);

		if (ec) {
			logger->info(get_name(), ec.message().c_str());
		}

		lua->pushboolean(L, 1);  // success
	}
	else {
		lua->pushboolean(L, 0);  // error
	}
	return 1;
}

static int l_send_message(lua_State* L) {
	if (lua->isstring(L, 1)) {
		std::string data = lua->tolstring(L, 1, nullptr);

		CA2W ca2w(data.c_str());
		wstring data_wstr = ca2w;

		json::value message_json = json::value::object();
		message_json[L"type"] = json::value::string(U("data"));
		message_json[L"data"] = json::value::string(data_wstr);


		std::string message = utility::conversions::to_utf8string(message_json.serialize());

		websocketpp::lib::error_code ec;
		c.send(con, message, websocketpp::frame::opcode::text, ec);

		if (ec) {
			logger->info(get_name(), ec.message().c_str());
		}

		lua->pushboolean(L, 1);  // success
	}
	else {
		lua->pushboolean(L, 0);  // error
	}
	return 1;
}


static void setup_game(GetApiFunction get_engine_api) {
  lua = (LuaApi128 *)get_engine_api(LUA_API_ID);
  logger = (LoggingApi *)get_engine_api(LOGGING_API_ID);

  setup_ws();


  lua->set_module_number("DarktideWs", "VERSION", 1);
  lua->add_module_function("DarktideWs", "join_room", l_join_room);
  lua->add_module_function("DarktideWs", "send_message", l_send_message);

  // MessageBoxA(NULL, "setup_game", "setup_game", 0);
}

static void loaded(GetApiFunction get_engine_api) {
  // MessageBoxA(NULL, "loaded", "loaded", 0);
}


float next_heartbeat = 0;

static void update(float dt) {

	next_heartbeat -= dt;

	if (next_heartbeat < 0) {
		next_heartbeat = 55;

		logger->info(get_name(), "Doki");
		websocketpp::lib::error_code ec;
		c.send(con, "{\"type\":\"doki\"}", websocketpp::frame::opcode::text, ec);

		if (ec) {
			logger->info(get_name(), ec.message().c_str());
		}
	}

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
