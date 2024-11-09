#include <winsock2.h>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <map>

#include <PluginApi128.h>
#include <lua/lua.hpp>
#include <string_format.cpp>
#include <starts_with.cpp>

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
std::vector<websocketpp::connection_hdl> close_queue;

static const char *get_name()
{
	return "darktide_ws_plugin";
}

int shutting_down = 0;

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
	if (shutting_down)
		return;

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
	logger->info(get_name(), "Websocket connection open");

	if (shutting_down)
		return;

	auto it = connections.find(hdl);
	if (it == connections.end())
	{
		logger->info(get_name(), "No on_open callback found");
	}
	else
	{
		connections[hdl].is_connected = true;

		lua_State *L = lua->getscriptenvironmentstate();
		lua->rawgeti(L, LUA_REGISTRYINDEX, connections[hdl].on_connect_callback);
		lua->call(L, 0, 0);
	}
}

void on_close(websocketpp::connection_hdl hdl)
{

	logger->info(get_name(), "Websocket connection closed");

	if (shutting_down)
		return;

	auto it = connections.find(hdl);
	if (it == connections.end())
	{
		logger->info(get_name(), "No on_close callback found");
	}
	else
	{
		connections[hdl].is_connected = false;

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
	if (shutting_down)
		return 0;

	websocketpp::connection_hdl hdl = toHandle(L, 1);
	logger->info(get_name(), "connection gced");

	try
	{
		auto connection = connections.at(hdl);
		connection.is_connected = false;
		close_queue.push_back(hdl);
	}
	catch (std::out_of_range e)
	{
		logger->info(get_name(), "connection already closed");
	}
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

static int WebSocket_close(lua_State *L)
{
	websocketpp::connection_hdl hdl = toHandle(L, 1);
	connections[hdl].is_connected = false;
	close_queue.push_back(hdl);
	return 1;
}

static const luaL_Reg WebSockets_methods[] = {
	{"connect", l_connect},
	{0, 0}};

static const luaL_Reg WebSocket_meta[] = {
	{"__gc", WebSocket_gc},
	{"__tostring", WebSocket_tostring},
	{"send", WebSocket_send_message},
	// TODO: state()
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
	for (auto const &hdl : close_queue)
	{

		if (starts_with(connections[hdl].uri, "wss://"))
		{
			auto con = secure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			secure_client.close(con, websocketpp::close::status::going_away, "closing", ec);
			secure_client.poll();

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
			insecure_client.poll();

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}
		}

		connections.erase(hdl);
	}

	close_queue.clear();

	for (auto const &x : connections)
	{
		secure_client.poll_one();
		insecure_client.poll_one();
	};
}

static void shutdown()
{
	shutting_down = 1;

	logger->info(get_name(), "Shutting down");

	try
	{
		secure_client.stop_perpetual();
		insecure_client.stop_perpetual();
	}
	catch (websocketpp::exception const &e)
	{
		logger->info(get_name(), string_format("error stopping perpetual (%s)", e.what()).c_str());
	}

	try
	{

		if (secure_client.is_listening())
		{
			secure_client.stop_listening();
		}
		if (insecure_client.is_listening())
		{
			insecure_client.stop_listening();
		}
	}
	catch (websocketpp::exception const &e)
	{
		logger->info(get_name(), string_format("error stopping listening (%s)", e.what()).c_str());
	}

	for (auto const &hdl : close_queue)
	{
		logger->info(get_name(), string_format("Disconnecting %s", connections[hdl].uri.c_str()).c_str());
		if (starts_with(connections[hdl].uri, "wss://"))
		{
			auto con = secure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			secure_client.close(con, websocketpp::close::status::going_away, "closing", ec);
			secure_client.run();

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}

			con = nullptr;
		}
		else
		{
			auto con = insecure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			insecure_client.close(con, websocketpp::close::status::going_away, "closing", ec);
			insecure_client.run();

			if (ec)
			{
				logger->info(get_name(), ec.message().c_str());
			}

			con = nullptr;
		}

		connections.erase(hdl);
	}

	close_queue.clear();

	for (auto const &x : connections)
	{
		auto hdl = x.first;
		auto meta = x.second;

		logger->info(get_name(), string_format("Disconnecting %s", meta.uri.c_str()).c_str());

		if (starts_with(meta.uri, "wss://"))
		{
			auto con = secure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			secure_client.close(con, websocketpp::close::status::going_away, "closing", ec);
			secure_client.run();

			if (ec)
			{
				logger->info(get_name(), string_format("error closing secure con (%s)", ec.message().c_str()).c_str());
			}

			con = nullptr;
		}
		else
		{
			auto con = insecure_client.get_con_from_hdl(hdl);

			websocketpp::lib::error_code ec;
			insecure_client.close(hdl, websocketpp::close::status::going_away, "closing", ec);
			insecure_client.run();

			if (ec)
			{
				logger->info(get_name(), string_format("error closing insecure con (%s)", ec.message().c_str()).c_str());
			}

			con = nullptr;
		}
	}

	connections.clear();
	hdls.clear();

	secure_client.run();
	insecure_client.run();

	if (!secure_client.stopped())
	{
		logger->info(get_name(), "secure client NOT stopped");
		secure_client.stop();
	}

	if (!insecure_client.stopped())
	{
		logger->info(get_name(), "insecure client NOT stopped");
		insecure_client.stop();
	}

	logger->info(get_name(), "Shutdown complete");

	// TODO: for some reason the process doesn't exit, can't figure it out
	// Boom
	exit(0);
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
