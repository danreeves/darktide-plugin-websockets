#include <winsock2.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <map>

#include <PluginApi128.h>
#include <lua/lua.hpp>
#include <string_format.cpp>
#include <starts_with.cpp>
// #include <script.lua.h>

using namespace std;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

LuaApi128 *lua{};
LoggingApi *logger{};

struct ConnectionMeta
{
	std::string uri;
	bool is_connected;
	int on_connect_callback;
	int on_message_callback;
	int on_close_callback;
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
		logger->info(get_name(), "No on_message callback found");
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

	auto it = connections.find(hdl);
	if (it == connections.end())
	{
		logger->info(get_name(), "No on_open callback found");
	}
	else
	{
		lua_State *L = lua->getscriptenvironmentstate();
		lua->rawgeti(L, LUA_REGISTRYINDEX, connections[hdl].on_connect_callback);
		lua->call(L, 0, 0);
	}
}

void on_close(websocketpp::connection_hdl hdl)
{
	connections[hdl].is_connected = false;
	logger->info(get_name(), "Websocket connection closed");

	auto it = connections.find(hdl);
	if (it == connections.end())
	{
		logger->info(get_name(), "No on_close callback found");
	}
	else
	{
		lua_State *L = lua->getscriptenvironmentstate();
		lua->rawgeti(L, LUA_REGISTRYINDEX, connections[hdl].on_close_callback);
		lua->call(L, 0, 0);
	}
}

static int pushHandle(lua_State *L, websocketpp::connection_hdl hdl)
{
	hdls.push_back(hdl);
	int curr_size = hdls.size() - 1;

	int *ud = (int *)lua->newuserdata(L, sizeof(int));
	*ud = curr_size;
	lua->lib_newmetatable(L, "WebSocket");
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

	if (lua->type(L, 3) != LUA_TFUNCTION)
	{
		logger->error(get_name(), string_format("connect: second argument is not a function (%s)", arg_2_type).c_str());
		lua->pushboolean(L, 0); // error
		return 1;
	}

	if (lua->type(L, 4) != LUA_TFUNCTION)
	{
		logger->error(get_name(), string_format("connect: second argument is not a function (%s)", arg_2_type).c_str());
		lua->pushboolean(L, 0); // error
		return 1;
	}

	std::string uri = lua->tolstring(L, 1, nullptr);
	lua->pushvalue(L, 2);
	int on_connect_callback = lua->lib_ref(L, LUA_REGISTRYINDEX);
	lua->pushvalue(L, 3);
	int on_message_callback = lua->lib_ref(L, LUA_REGISTRYINDEX);
	lua->pushvalue(L, 4);
	int on_close_callback = lua->lib_ref(L, LUA_REGISTRYINDEX);

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
			m.on_connect_callback = on_connect_callback;
			m.on_message_callback = on_message_callback;
			m.on_close_callback = on_close_callback;
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
			m.on_connect_callback = on_connect_callback;
			m.on_message_callback = on_message_callback;
			m.on_close_callback = on_close_callback;
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

	return 0;
}

static websocketpp::connection_hdl toHandle(lua_State *L, int index)
{
	int *hdls_idx = (int *)lua->touserdata(L, index);
	websocketpp::connection_hdl handle = hdls[*hdls_idx];
	return handle;
}

static int WebSocket_gc(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);
	logger->info(get_name(), "connection gced");
	// TODO: Disconnect it
	return 0;
}

static int WebSocket_tostring(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);
	lua->pushstring(L, string_format("[WebSocket: %s]", connections[hdl].uri.c_str()).c_str());
	return 1;
}

// TODO: Queue sends before is_connected
static int WebSocket_send_message(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);

	if (!connections[hdl].is_connected)
	{

		logger->info(get_name(), "Can't send, not connected");
		lua->pushboolean(L, 0); // error
		return 1;
	}

	if (lua->isstring(L, 2))
	{
		std::string message = lua->tolstring(L, 2, nullptr);

		websocketpp::lib::error_code ec;

		if (starts_with(connections[hdl].uri, "wss://"))
		{
			auto con = secure_client.get_con_from_hdl(hdl);
			secure_client.send(con, message, websocketpp::frame::opcode::text, ec);
		}
		else
		{

			auto con = insecure_client.get_con_from_hdl(hdl);
			insecure_client.send(con, message, websocketpp::frame::opcode::text, ec);
		}

		if (ec)
		{
			logger->info(get_name(), ec.message().c_str());
		}

		lua->pushboolean(L, 1); // success
		return 1;
	}
	else
	{
		lua->pushboolean(L, 0); // error
		return 1;
	}
}

// TODO: add meta to close but actually close in update loop
// 		 block sending while queued to close
static int WebSocket_close(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);

	connections[hdl].is_connected = false;

	if (starts_with(connections[hdl].uri, "wss://"))
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

	return 1;
}

static const luaL_Reg WebSockets_methods[] = {
	{"connect", l_connect},
	{0, 0}};

static const luaL_Reg WebSocket_meta[] = {
	{"__gc", WebSocket_gc},
	{"__tostring", WebSocket_tostring},
	{"send", WebSocket_send_message},
	{"close", WebSocket_close},
	{0, 0}};

int WebSocket_register(lua_State *L)
{
	// create methods table
	lua->lib_openlib(L, "WebSockets", WebSockets_methods, 0);

	// create meta table for WebSocket
	lua->lib_newmetatable(L, "WebSocket");
	lua->lib_openlib(L, 0, WebSocket_meta, 0); /* fill metatable */

	// set WebSocket.__index = WebSocket
	lua->pushstring(L, "__index");
	lua->pushvalue(L, -2);
	lua->rawset(L, -3);

	// set WebSocket.__metatable = WebSocket
	lua->pushstring(L, "__metatable");
	lua->pushvalue(L, -2);
	lua->rawset(L, -3);

	lua->pop(L);
	lua->pop(L);
	return 0;
}

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

	lua_State *L = lua->getscriptenvironmentstate();
	WebSocket_register(L);
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
		return get_dynamic_plugin_api(api);
	}
}
