#include <winsock2.h>
#include <Windows.h>
#include <ctime>
#include <iostream>
#include <sstream>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <map>

#include <PluginApi128.h>
#include <json.hpp>
#include <lua/lua.hpp>
#include <string_format.cpp>
#include "bits/unique_ptr.h"
// #include <script.lua.h>

using namespace std;

using json = nlohmann::json;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

LuaApi128 *lua{};
LoggingApi *logger{};

struct ConnectionMeta
{
	std::string uri;
	bool is_connected;
	int on_message_callback;
};

websocketpp::client<websocketpp::config::asio_tls_client> secure_client;
websocketpp::client<websocketpp::config::asio_client> insecure_client;
std::map<websocketpp::connection_hdl, ConnectionMeta, std::owner_less<websocketpp::connection_hdl>> connections;
std::vector<websocketpp::connection_hdl> hdls;

static const char *get_name()
{
	return "darktide_ws_plugin";
}

static std::shared_ptr<boost::asio::ssl::context> on_tls_init()
{
	// establishes a SSL connection
	std::shared_ptr<boost::asio::ssl::context> ctx = std::make_shared<boost::asio::ssl::context>(
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

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;
void on_message(websocketpp::connection_hdl hdl, message_ptr msg)
{
	std::string message = msg->get_payload().c_str();
	logger->info(get_name(), message.c_str());

	auto it = connections.find(hdl);
	if (it == connections.end())
	{
		logger->info(get_name(), "Message received on connection but no handler");
	}
	else
	{
		lua_State *L = lua->getscriptenvironmentstate();
		lua->rawgeti(L, LUA_REGISTRYINDEX, connections[hdl].on_message_callback);
		lua->pushstring(L, message.c_str());
		lua->call(L, 1, 0);
	}
}

void on_open(websocketpp::connection_hdl hdl)
{
	connections[hdl].is_connected = true;
	logger->info(get_name(), "Websocket connection open");
}

void on_close(websocketpp::connection_hdl hdl)
{
	connections[hdl].is_connected = false;
	logger->info(get_name(), "Websocket connection closed");
}

static bool starts_with(const std::string str, const std::string prefix)
{
	return ((prefix.size() <= str.size()) && std::equal(prefix.begin(), prefix.end(), str.begin()));
}

static int pushHandle(lua_State *L, websocketpp::connection_hdl hdl)
{
	hdls.push_back(hdl);
	int curr_size = hdls.size() - 1;

	int *ud = (int *)lua->newuserdata(L, sizeof(int));
	*ud = curr_size;
	lua->lib_newmetatable(L, "ConnectionHandle");
	lua->setmetatable(L, -2);
	return 1;
}

static int l_connect(lua_State *L)
{

	const char *arg_1_type = lua->lua_typename(L, lua->type(L, 1));
	const char *arg_2_type = lua->lua_typename(L, lua->type(L, 2));

	if (lua->type(L, 1) != LUA_TSTRING)
	{
		logger->error(get_name(), string_format("connect: first argument is not a string (%s)", arg_1_type).c_str());
		lua->pushboolean(L, 0); // error
		return 1;
	}

	if (lua->type(L, 2) != LUA_TFUNCTION)
	{
		logger->error(get_name(), string_format("connect: second argument is not a function (%s)", arg_2_type).c_str());
		lua->pushboolean(L, 0); // error
		return 1;
	}

	std::string uri = lua->tolstring(L, 1, nullptr);
	lua->pushvalue(L, 2);
	int callback_reference = lua->lib_ref(L, LUA_REGISTRYINDEX);

	try
	{

		if (starts_with(uri, "wss://"))
		{
			websocketpp::lib::error_code ec;
			auto con = secure_client.get_connection(uri, ec);

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
				return 0;
			}

			// Note that connect here only requests a connection. No network messages
			// are exchanged until the event loop starts running
			auto connection = secure_client.connect(con);
			auto hdl = connection->get_handle();

			ConnectionMeta m{};
			m.is_connected = false;
			m.uri = uri;
			m.on_message_callback = callback_reference;
			connections[hdl] = m;
			pushHandle(L, hdl);
			return 1;
		}
		else
		{
			websocketpp::lib::error_code ec;
			auto con = insecure_client.get_connection(uri, ec);

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
				return 0;
			}

			// Note that connect here only requests a connection. No network messages
			// are exchanged until the event loop starts running
			auto connection = insecure_client.connect(con);
			auto hdl = connection->get_handle();

			ConnectionMeta m{};
			m.is_connected = false;
			m.uri = uri;
			m.on_message_callback = callback_reference;
			connections[hdl] = m;
			pushHandle(L, connection);
			return 1;
		}
	}
	catch (websocketpp::exception const &e)
	{
		logger->info(get_name(), e.what());
		lua->pushboolean(L, 0); // error
		return 1;
	}

	// lua->createtable(L, 0, 1);
	// lua_pushcfunction(L, l_sin);
	// lua->setfield(L, -2, "field1");
	// lua->pushcclosure(L, l_sin, 1);
	// lua->setfield(L, -1, "send");
	// lua->settable(L, -1);
	// lua->pushboolean(L, 1);
	return 0;
}

static websocketpp::connection_hdl toHandle(lua_State *L, int index)
{
	int *hdls_idx = (int *)lua->touserdata(L, index);
	websocketpp::connection_hdl handle = hdls[*hdls_idx];
	// if (handle == NULL)
	// lua->lib_typerror(L, index, "ConnectionHandle");
	return handle;
}

static const luaL_Reg ConnectionHandle_methods[] = {
	{"connect", l_connect},
	{0, 0}};

static int ConnectionHandle_gc(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);
	logger->info(get_name(), "connection gced");
	return 0;
}

static int ConnectionHandle_tostring(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);
	// lua->pushfstring(L, "Connection to: %p", connections[hdl].uri);
	lua->pushfstring(L, "Connection to: %p", "blahhh");
	return 1;
}

static const luaL_Reg ConnectionHandle_meta[] = {
	{"__gc", ConnectionHandle_gc},
	{"__tostring", ConnectionHandle_tostring},
	{0, 0}};

int ConnectionHandle_register(lua_State *L)
{
	lua->lib_openlib(L, "ConnectionHandle", ConnectionHandle_methods, 0); /* create methods table,
												 add it to the globals */
	lua->lib_newmetatable(L, "ConnectionHandle");						  /* create metatable for Image,
															 add it to the Lua registry */
	lua->lib_openlib(L, 0, ConnectionHandle_meta, 0);					  /* fill metatable */
	lua->pushstring(L, "__index");
	lua->pushvalue(L, -3); /* dup methods table*/
	lua->rawset(L, -3);	   /* metatable.__index = methods */
	lua->pushstring(L, "__metatable");
	lua->pushvalue(L, -3); /* dup methods table*/
	lua->rawset(L, -3);	   /* hide metatable:
							  metatable.__metatable = methods */
	lua->pop(L);		   /* drop metatable */
	return 1;			   /* return methods on the stack */
}

// static int l_send_message(lua_State *L)
// {
// 	void *handle = lua->touserdata(L, 1);
// 	secure_client.get_con_from_hdl(*handle);

// 	if (!is_connected)
// 	{

// 		logger->info(get_name(), "Can't send, not connected");
// 		lua->pushboolean(L, 0); // error
// 		return 1;
// 	}

// 	if (lua->isstring(L, 1))
// 	{
// 		std::string data = lua->tolstring(L, 1, nullptr);

// 		json message_json = {
// 			{"type", "data"},
// 			{"data", data},
// 		};
// 		std::string message = message_json.dump();

// 		websocketpp::lib::error_code ec;
// 		c.send(con, message, websocketpp::frame::opcode::text, ec);

// 		if (ec)
// 		{
// 			logger->info(get_name(), ec.message().c_str());
// 		}

// 		lua->pushboolean(L, 1); // success
// 	}
// 	else
// 	{
// 		lua->pushboolean(L, 0); // error
// 	}
// 	return 1;
// }

static void setup_game(GetApiFunction get_engine_api)
{
	lua = (LuaApi128 *)get_engine_api(LUA_API_ID);
	logger = (LoggingApi *)get_engine_api(LOGGING_API_ID);

	// Disable logging
	secure_client.clear_access_channels(websocketpp::log::alevel::all);
	secure_client.init_asio();
	secure_client.set_tls_init_handler(bind(&on_tls_init));
	secure_client.set_message_handler(bind(&on_message, ::_1, ::_2));
	secure_client.set_open_handler(bind(&on_open, ::_1));
	secure_client.set_close_handler(bind(&on_close, ::_1));

	// Disable logging
	insecure_client.clear_access_channels(websocketpp::log::alevel::all);
	insecure_client.init_asio();
	insecure_client.set_message_handler(bind(&on_message, ::_1, ::_2));
	insecure_client.set_open_handler(bind(&on_open, ::_1));
	insecure_client.set_close_handler(bind(&on_close, ::_1));

	// lua->set_module_number("WebSockets", "VERSION", 1);
	// lua->add_module_function("WebSockets", "connect", l_connect);
	// lua->add_module_function("WebSockets", "send", l_send);

	lua_State *L = lua->getscriptenvironmentstate();
	ConnectionHandle_register(L);
}

static void loaded(GetApiFunction get_engine_api)
{
}

static void update(float dt)
{
	for (auto const &x : connections)
	{
		secure_client.poll_one();
		insecure_client.poll_one();
	};
}

static void shutdown()
{
	for (auto const &x : connections)
	{
		auto hdl = x.first;
		auto meta = x.second;

		if (starts_with(meta.uri, "wss://"))
		{
			auto con = secure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			secure_client.close(con, websocketpp::close::status::going_away, "closing", ec);

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}
		}

		else
		{
			auto con = insecure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			insecure_client.close(con, websocketpp::close::status::going_away, "closing", ec);

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}
		}
	}
	secure_client.poll();
	insecure_client.poll();
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
