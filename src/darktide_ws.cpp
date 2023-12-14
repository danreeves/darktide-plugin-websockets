#include <ctime>
#include <sstream>

#include "lua.h"
#include "PluginApi128.h"

// #include <Windows.h>

LuaApi128 *lua{};
LoggingApi *logger{};

static void setup_game(GetApiFunction get_engine_api)
{
	lua = (LuaApi128 *)get_engine_api(LUA_API_ID);
	logger = (LoggingApi *)get_engine_api(LOGGING_API_ID);

	// MessageBoxA(NULL, "setup_game", "setup_game", 0);
}

static const char *get_name()
{
	return "Darktide Websockets";
}

static void loaded(GetApiFunction get_engine_api)
{
	// MessageBoxA(NULL, "loaded", "loaded", 0);
}

static void update(float dt)
{
	// MessageBoxA(NULL, "update", "update", 0);
}

static void shutdown()
{
	// MessageBoxA(NULL, "shutdown", "shutdown", 0);
}

extern "C"
{
	void *get_dynamic_plugin_api(unsigned api)
	{
		if (api == PLUGIN_API_ID)
		{

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
	PLUGIN_DLLEXPORT void *get_plugin_api(unsigned api)
	{
		// MessageBoxA(NULL, "get_plugin_api", "get_plugin_api", 0);
		return get_dynamic_plugin_api(api);
	}
#endif
}
