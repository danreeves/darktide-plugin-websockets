/** Compile with:
gcc -Ilib -Isrc -I"C:\msys64\mingw64\include" -L"C:\msys64\mingw64\lib" -shared -o darktide_ws_pluginw64.dll .\src\plugin.cpp -lstdc++ -lwsock32 -lssl -lcrypto --std=c++11 -lws2_32 -s
*/
#include <winsock2.h>
#include <Windows.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>

#include <PluginApi128.h>
#include <json.hpp>
#include <lua/lauxlib.h>
#include <string_format.cpp>

using namespace std;

using json = nlohmann::json;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
typedef std::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

LuaApi128 *lua{};
LoggingApi *logger{};

client c;
client::connection_ptr con;

static const char *get_name() { return "darktide_ws_plugin64"; }

void on_message(client *c, websocketpp::connection_hdl hdl, message_ptr msg)
{
	std::string message = msg->get_payload().c_str();
	logger->info(get_name(), message.c_str());

	lua_State *L = lua->getscriptenvironmentstate();
	lua->getfield(L, LUA_GLOBALSINDEX, "_darktide_ws_callback"); // get global function

	const char *type = lua->lua_typename(L, 0);
	if (type != "function")
	{
		logger->info(get_name(), string_format("_darktide_ws_callback not defined or not callable (%s)", type).c_str());
		return;
	}

	lua->pushstring(L, message.c_str()); // push 1st argument
	lua->call(L, 1, 0);					 // call with 1 arg, 0 results
}

static context_ptr on_tls_init()
{
	// establishes a SSL connection
	context_ptr ctx = std::make_shared<boost::asio::ssl::context>(
		boost::asio::ssl::context::sslv23);

	try
	{
		ctx->set_options(boost::asio::ssl::context::default_workarounds |
						 boost::asio::ssl::context::no_sslv2 |
						 boost::asio::ssl::context::no_sslv3 |
						 boost::asio::ssl::context::single_dh_use);
	}
	catch (std::exception &e)
	{
		logger->info(get_name(), e.what());
	}
	return ctx;
}

bool is_connecting = false;
bool is_connected = false;

void on_open(websocketpp::connection_hdl hdl)
{
	is_connecting = false;
	is_connected = true;
	logger->info(get_name(), "Websocket connection open");
}

void on_close(websocketpp::connection_hdl hdl)
{
	is_connecting = false;
	is_connected = false;
	logger->info(get_name(), "Websocket connection closed");
}

int setup_ws()
{
	std::string uri = "wss://ws.darkti.de";

	try
	{
		// Disable logging
		c.clear_access_channels(websocketpp::log::alevel::all);
		c.init_asio();
		c.set_tls_init_handler(bind(&on_tls_init));
		c.set_message_handler(bind(&on_message, &c, ::_1, ::_2));
		c.set_open_handler(on_open);
		c.set_close_handler(on_close);

		websocketpp::lib::error_code ec;
		con = c.get_connection(uri, ec);

		if (ec)
		{
			logger->info(get_name(), ec.message().c_str());
			return 0;
		}

		// Note that connect here only requests a connection. No network messages
		// are exchanged until the event loop starts running
		c.connect(con);
		is_connecting = true;
	}
	catch (websocketpp::exception const &e)
	{
		logger->info(get_name(), e.what());
	}

	return 1;
}

static int l_join_room(lua_State *L)
{

	if (!is_connected)
	{

		logger->info(get_name(), "Can't join, not connected");
		lua->pushboolean(L, 0); // error
		return 1;
	}

	if (lua->isstring(L, 1))
	{
		std::string room = lua->tolstring(L, 1, nullptr);

		json message_json = {
			{"type", "join"},
			{"room", room},
		};
		std::string message = message_json.dump();

		websocketpp::lib::error_code ec;
		c.send(con, message, websocketpp::frame::opcode::text, ec);

		if (ec)
		{
			logger->info(get_name(), ec.message().c_str());
		}

		lua->pushboolean(L, 1); // success
	}
	else
	{
		lua->pushboolean(L, 0); // error
	}

	return 1;
}

static int l_send_message(lua_State *L)
{
	if (!is_connected)
	{

		logger->info(get_name(), "Can't send, not connected");
		lua->pushboolean(L, 0); // error
		return 1;
	}

	if (lua->isstring(L, 1))
	{
		std::string data = lua->tolstring(L, 1, nullptr);

		json message_json = {
			{"type", "data"},
			{"data", data},
		};
		std::string message = message_json.dump();

		websocketpp::lib::error_code ec;
		c.send(con, message, websocketpp::frame::opcode::text, ec);

		if (ec)
		{
			logger->info(get_name(), ec.message().c_str());
		}

		lua->pushboolean(L, 1); // success
	}
	else
	{
		lua->pushboolean(L, 0); // error
	}
	return 1;
}

static void setup_game(GetApiFunction get_engine_api)
{
	lua = (LuaApi128 *)get_engine_api(LUA_API_ID);
	logger = (LoggingApi *)get_engine_api(LOGGING_API_ID);

	setup_ws();

	lua->set_module_number("DarktideWs", "VERSION", 1);
	lua->add_module_function("DarktideWs", "join_room", l_join_room);
	lua->add_module_function("DarktideWs", "send_message", l_send_message);

	// logger->info(get_name(), "setup_game");
	// MessageBoxA(NULL, "setup_game!!!!!!!!!!", "setup_game", 0);
}

static void loaded(GetApiFunction get_engine_api)
{
}

float next_heartbeat = 0;
static void update(float dt)
{
	if (is_connected)
	{
		next_heartbeat -= dt;

		if (next_heartbeat < 0)
		{
			next_heartbeat = 55;

			logger->info(get_name(), "Doki");
			websocketpp::lib::error_code ec;
			c.send(con, "{\"type\":\"doki\"}", websocketpp::frame::opcode::text, ec);

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}
		}
	}

	if (is_connecting || is_connected)
	{
		c.poll_one();
	}
}

static void shutdown()
{
	websocketpp::lib::error_code ec;
	c.close(con, websocketpp::close::status::going_away, "closing", ec);
	c.poll();

	if (ec)
	{
		logger->info(get_name(), ec.message().c_str());
	}
}

extern "C"
{
	void *get_dynamic_plugin_api(unsigned api)
	{
		if (api == PLUGIN_API_ID)
		{
			// MessageBoxA(NULL, "get_dynamic_plugin_api!!!!!", "get_dynamic_plugin_api", 0);
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

	__declspec(dllexport) void *get_plugin_api(unsigned api)
	{
		//   MessageBoxA(NULL, "get_plugin_api base!!!!!", "get_plugin_api", 0);
		return get_dynamic_plugin_api(api);
	}
}
