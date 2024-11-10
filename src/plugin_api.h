#pragma once
// This is manually updated and minified PluginApi based on the publicly
// available sources at: https://github.com/AutodeskGames/stingray-plugin
// From
// https://github.com/thewhitegoatcb/rawray/blob/master/rawray/PluginApi128.h

#define PLUGIN_DLLEXPORT __declspec(dllexport)

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CApiUnit CApiUnit;
typedef struct CApiWorld CApiWorld;

#ifdef __cplusplus
extern "C"
{
#endif

   /* API IDs for the different services. */
   enum PluginApiID
   {
      PLUGIN_API_ID = 0,
      LUA_API_ID = 1,
      DATA_COMPILER_API_ID = 2,
      DATA_COMPILE_PARAMETERS_API_ID = 3,
      ALLOCATOR_API_ID = 4,
      RESOURCE_MANAGER_API_ID = 5,
      LOGGING_API_ID = 6,
      UNIT_API_ID = 8,
      SCENE_GRAPH_API_ID = 9,
      FUTURE_INPUT_ARCHIVE_API_ID = 10,
      INPUT_ARCHIVE_API_ID = 11,
      APPLICATION_API_ID = 12,
      APPLICATION_OPTIONS_API_ID = 13,
      UNIT_REFERENCE_API_ID = 14,
      ERROR_CONTEXT_API_ID = 15,
      RENDER_INTERFACE_API_ID = 16,
      RAYCAST_API_ID = 17,
      RENDER_CALLBACKS_PLUGIN_API_ID = 18,
      RENDER_OVERRIDES_PLUGIN_API_ID = 19,
      FILESYSTEM_API_ID = 20,
      PLUGIN_MANAGER_API_ID = 21,
      WORLD_API_ID = 22,
      LINE_OBJECT_DRAWER_API_ID = 23,
      PROFILER_API_ID = 24,
      ERROR_API_ID = 25,
      RENDER_BUFFER_API_ID = 26,
      MESH_API_ID = 27,
      INPUT_BUFFER_API_ID = 28,
      RENDER_SCENE_GRAPH_API_ID = 29,
      SOUND_STREAM_SOURCE_API_ID = 30,
      C_API_ID = 31,
      THREAD_API_ID = 32,
      TIMER_API_ID = 33,
      MATERIAL_API_ID = 34,
      SCENE_DATABASE_API_ID = 35,
      STREAM_CAPTURE_API_ID = 36,
      FLOW_NODES_API_ID = 37,
      CAMERA_API_ID = 38,
      END_OF_ENGINE_RESERVED_RANGE = 65535,

      /* API IDs in the range 0--65535 are reserved by the engine. If you want to
         provide your own API in your plugin, we suggest using a hash of the API's
         name as ID. */
   };

   /* This function can be used by the plugin to query for engine APIs. */
   typedef void *(*GetApiFunction)(unsigned api);

   struct PluginApi
   {
#ifdef __cplusplus
      uint32_t version = 65;
      uint32_t flags = 3; // lua plugin
#else
   uint32_t version;
   uint32_t flags;
#endif

      /* Called to initialize the plugin once it has been loaded into memory. For
         plugins loaded at boot, this function is called once all plugins have been
         loaded (so you can query for other plugins in this function). For plugins
         loaded on demand, this function is called as soon as the plugin have been
         loaded. */
      void (*loaded)(GetApiFunction get_engine_api);

      /* Called at the start of a "hot reload" of the plugin DLL. Should return a
              pointer to a serialized "state" for the plugin.*/
      void *(*start_reload)(GetApiFunction get_engine_api);

      /* Called just before the plugin is unloaded. */
      void (*unloaded)();

      /* Called to finalized the "hot reload" after the plugin has been reloaded
         with the new code. Should restore the state from the serialized state data.
         Note that it is the responsibility of the plugin to free the state data. */
      void (*finish_reload)(GetApiFunction get_engine_api, void *state);

      /* Called when the engine sets up the ResourceManager. At this point you can
         use the ResourceManagerApi to add support for resource manager loading of
         your custom data types. */
      void (*setup_resources)(GetApiFunction get_engine_api);

      /* Called when the engine shuts down the ResourceManager. */
      void (*shutdown_resources)();

      /* Called when the engine sets up the game. At this point, you can use the
         functions in the LuaApi to add functions to the engine's Lua API. */
      void (*setup_game)(GetApiFunction get_engine_api);

      /* Called per game frame. */
      void (*update_game)(float dt);

      /* Called when the engine shuts down the game. */
      void (*shutdown_game)();

      /* Called before a world is about to be destroyed and removed from the engines
       * list of worlds. */
      void (*unregister_world)(CApiWorld *world);

      /* Called after the world has been created and is about to be added to the
       * engines list of worlds */
      void (*register_world)(CApiWorld *world);

      /* Returns the hash of the plugin. (Optional) */
      uint64_t (*get_hash)();

      /* Returns the name of the plugin. */
      const char *(*get_name)();

      void (*unkfunc13)();
      void (*unkfunc14)();
      void *(*unkfunc15)(); // returns some func table

      /* Called by the engine to draw debug visuals into the world. */
      void (*debug_draw)(CApiWorld *world, struct StateReflectionStream *srs);
   };

   /* Opaque struct representing a Lua state. */
   typedef struct lua_State lua_State;

   /* Types from Lua. */
   typedef int (*lua_CFunction)(lua_State *L);
   typedef struct lua_Debug lua_Debug;
   typedef double lua_Number;
   typedef ptrdiff_t lua_Integer;
   typedef void (*lua_Hook)(lua_State *L, lua_Debug *ar);
   typedef const char *(*lua_Reader)(lua_State *L, void *ud, size_t *sz);
   typedef int (*lua_Writer)(lua_State *L, const void *p, size_t sz, void *ud);
   typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);
   typedef struct luaL_Reg luaL_Reg;

   /*
           Interface to access Lua services.
   */
   struct LuaApi
   {
      /* -----------------------------------------------------------------------
              Extending Lua
        ----------------------------------------------------------------------- */

      /* Adds a new lua function in the module with the specified name.

         The function will be callable as stingray.{module}.{name}() */
      void (*add_module_function)(const char *module, const char *name,
                                  lua_CFunction f);

      /* Sets a bool constant in a module. */
      void (*set_module_bool)(const char *module, const char *key, int value);

      /* Sets a number constant in a module. */
      void (*set_module_number)(const char *module, const char *key, double value);

      /* Sets a string constant in a module. */
      void (*set_module_string)(const char *module, const char *key,
                                const char *value);

      /* As add_module_function() but prints a deprecation warning when the function
       * is used. */
      void (*deprecated_warning)(const char *module, const char *name,
                                 lua_CFunction f, const char *message);

      /* Adds a new console command. Console commands are Lua functions in the
         stingray.Console module that only takes string parameters. They can be used
         from the console:

                      > memory_resources all

         is equivalent to

                      > Console.memory_resources("all")

         The vararg is a sequence of documentation strings, ended by a nullptr --
         like this:

              add_console_command("memory_resources", memory_resources_f,
                      "Print memory usage per resource",
                      "all", "print memory use of all resources",
                      "sound", "print memory use of sound resources",
                      "texture_categories [details]", "print memory usage per
         texture category", "list <TYPE>", "list memory use of specified types",
                      (void*)nullptr); */
      void (*add_console_command)(const char *command, lua_CFunction f,
                                  const char *desc, ...);

      /* -----------------------------------------------------------------------
              Lua standard functions

              See the Lua API for documentation.
        ----------------------------------------------------------------------- */

      /* State manipulation */
      lua_State *(*newstate)(lua_Alloc f, void *ud);
      void (*close)(lua_State *L);
      lua_State *(*newthread)(lua_State *L);
      lua_CFunction (*atpanic)(lua_State *L, lua_CFunction panicf);

      /* Basic stack manipulation */
      int (*gettop)(lua_State *L);
      void (*settop)(lua_State *L, int idx);
      void (*pushvalue)(lua_State *L, int idx);
      void (*remove)(lua_State *L, int idx);
      void (*insert)(lua_State *L, int idx);
      void (*replace)(lua_State *L, int idx);
      int (*checkstack)(lua_State *L, int sz);
      void (*xmove)(lua_State *from, lua_State *to, int n);

      const void *(*fnx1)(lua_State *L, int idx);

      /* Access functions */
      int (*isnumber)(lua_State *L, int idx);
      int (*isstring)(lua_State *L, int idx);
      int (*iscfunction)(lua_State *L, int idx);
      int (*isuserdata)(lua_State *L, int idx);
      int (*lua_type)(lua_State *L, int idx); // renamed from type
      const char *(*lua_typename)(lua_State *L, int tp);

      int (*equal)(lua_State *L, int idx1, int idx2);
      int (*rawequal)(lua_State *L, int idx1, int idx2);
      int (*lessthan)(lua_State *L, int idx1, int idx2);

      lua_Number (*tonumber)(lua_State *L, int idx);
      lua_Integer (*tointeger)(lua_State *L, int idx);
      int (*toboolean)(lua_State *L, int idx);
      const char *(*tolstring)(lua_State *L, int idx, size_t *len);
      size_t (*objlen)(lua_State *L, int idx);
      lua_CFunction (*tocfunction)(lua_State *L, int idx);
      void *(*touserdata)(lua_State *L, int idx);
      lua_State *(*tothread)(lua_State *L, int idx);
      const void *(*topointer)(lua_State *L, int idx);

      const void *(*fnx2)(lua_State *L, int idx);

      /* Push functions */
      void (*pushnil)(lua_State *L);
      void (*pushnumber)(lua_State *L, lua_Number n);
      void (*pushinteger)(lua_State *L, lua_Integer n);
      void (*pushlstring)(lua_State *L, const char *s, size_t l);
      void (*pushstring)(lua_State *L, const char *s);
      const char *(*pushvfstring)(lua_State *L, const char *fmt, va_list argp);
      const char *(*pushfstring)(lua_State *L, const char *fmt, ...);
      void (*pushcclosure)(lua_State *L, lua_CFunction fn, int n);
      void (*pushboolean)(lua_State *L, int b);
      void (*pushlightuserdata)(lua_State *L, void *p);
      int (*pushthread)(lua_State *L);

      /* Get functions */
      void (*gettable)(lua_State *L, int idx);
      void (*getfield)(lua_State *L, int idx, const char *k);
      void (*rawget)(lua_State *L, int idx);
      void (*rawgeti)(lua_State *L, int idx, int n);
      void (*createtable)(lua_State *L, int narr, int nrec);
      void *(*newuserdata)(lua_State *L, size_t sz);
      int (*getmetatable)(lua_State *L, int objindex);
      void (*getfenv)(lua_State *L, int idx);

      /* Set functions */
      void (*settable)(lua_State *L, int idx);
      void (*setfield)(lua_State *L, int idx, const char *k);
      void (*rawset)(lua_State *L, int idx);
      void (*rawseti)(lua_State *L, int idx, int n);
      int (*setmetatable)(lua_State *L, int objindex);
      int (*setfenv)(lua_State *L, int idx);

      /* Load and call functions */
      void (*call)(lua_State *L, int nargs, int nresults);
      int (*pcall)(lua_State *L, int nargs, int nresults, int errfunc);
      int (*cpcall)(lua_State *L, lua_CFunction func, void *ud);
      int (*load)(lua_State *L, lua_Reader reader, void *dt, const char *chunkname);
      int (*dump)(lua_State *L, lua_Writer writer, void *data);

      /* Coroutine functions */
      int (*yield)(lua_State *L, int nresults);
      int (*resume)(lua_State *L, int narg);
      int (*status)(lua_State *L);

      /* Garbage collection */
      int (*gc)(lua_State *L, int what, int data);

      /* Miscellaneous functions */
      int (*error)(lua_State *L);
      int (*next)(lua_State *L, int idx);
      void (*concat)(lua_State *L, int n);

      /* Debugging */
      int (*getstack)(lua_State *L, int level, lua_Debug *ar);
      int (*getinfo)(lua_State *L, const char *what, lua_Debug *ar);
      const char *(*getlocal)(lua_State *L, const lua_Debug *ar, int n);
      const char *(*setlocal)(lua_State *L, const lua_Debug *ar, int n);
      const char *(*getupvalue)(lua_State *L, int funcindex, int n);
      const char *(*setupvalue)(lua_State *L, int funcindex, int n);

      int (*sethook)(lua_State *L, lua_Hook func, int mask, int count);
      lua_Hook (*gethook)(lua_State *L);
      int (*gethookmask)(lua_State *L);
      int (*gethookcount)(lua_State *L);

      // int (*fnx3) (lua_State* L);

      /* Library functions */
      void (*lib_openlib)(lua_State *L, const char *libname, const luaL_Reg *l,
                          int nup);
      void (*lib_register)(lua_State *L, const char *libname, const luaL_Reg *l);
      int (*lib_getmetafield)(lua_State *L, int obj, const char *e);
      int (*lib_callmeta)(lua_State *L, int obj, const char *e);
      int (*lib_typerror)(lua_State *L, int narg, const char *tname);
      int (*lib_argerror)(lua_State *L, int numarg, const char *extramsg);
      const char *(*lib_checklstring)(lua_State *L, int numArg, size_t *l);
      const char *(*lib_optlstring)(lua_State *L, int numArg, const char *def,
                                    size_t *l);
      lua_Number (*lib_checknumber)(lua_State *L, int numArg);
      lua_Number (*lib_optnumber)(lua_State *L, int nArg, lua_Number def);
      lua_Integer (*lib_checkinteger)(lua_State *L, int numArg);
      lua_Integer (*lib_optinteger)(lua_State *L, int nArg, lua_Integer def);
      void (*lib_checkstack)(lua_State *L, int sz, const char *msg);
      void (*lib_checktype)(lua_State *L, int narg, int t);
      void (*lib_checkany)(lua_State *L, int narg);
      int (*lib_newmetatable)(lua_State *L, const char *tname);
      void *(*lib_checkudata)(lua_State *L, int ud, const char *tname);
      void (*lib_where)(lua_State *L, int lvl);
      int (*lib_error)(lua_State *L, const char *fmt, ...);
      int (*lib_checkoption)(lua_State *L, int narg, const char *def,
                             const char *const lst[]);
      int (*lib_ref)(lua_State *L, int t);
      void (*lib_unref)(lua_State *L, int t, int ref);
      int (*lib_loadfile)(lua_State *L, const char *filename);
      int (*lib_loadbuffer)(lua_State *L, const char *buff, size_t sz,
                            const char *name);
      int (*lib_loadstring)(lua_State *L, const char *s);
      lua_State *(*lib_newstate)(void);
      const char *(*lib_gsub)(lua_State *L, const char *s, const char *p,
                              const char *r);
      const char *(*lib_findtable)(lua_State *L, int idx, const char *fname,
                                   int szhint);
      void (*lib_openlibs)(lua_State *L);

      /* -----------------------------------------------------------------------
              Lua Stingray extensions
        ----------------------------------------------------------------------- */

      /* Gets an index integer from the stack. This will automatically convert
         from 1-based (Lua convention) to 0-based (C convention). */
      int (*getindex)(lua_State *L, int idx);

      /* Pushes an index integer to the stack. This will automatically convert
         from 0-based (C convention) to 1-based (lua convention). */
      void (*pushindex)(lua_State *L, int n);

      /* As getindex() but uses either 0 or 1 based convention on the Lua side,
         depending on user project settings. */
      int (*getindex_0_or_1_based)(lua_State *L, int idx);

      /* As pushindex() but uses either 0 or 1 based convention on the Lua side,
         depending on user project settings. */
      void (*pushindex_0_or_1_based)(lua_State *L, int n);

      /* Pushes a Vector3 to the stack. */
      void (*pushvector3)(lua_State *L, float v[3]);

      /* Pushes a Quaternion to the stack. */
      void (*pushquaternion)(lua_State *L, float q[4]);

      /* Pushes a Matrix4x4 to the stack. */
      void (*pushmatrix4x4)(lua_State *L, float m[16]);

      /* Pushes a Vector3Box to the stack. */
      void (*pushvector3box)(lua_State *L, float v[3]);

      /* Gets a Vector3 at the specified index. */
      float *(*getvector3)(lua_State *L, int i);

      /* Gets a Vector4 at the specified index. */
      float *(*getvector4)(lua_State *L, int i);

      /* Gets a Quaternion at the specified index. */
      float *(*getquaternion)(lua_State *L, int i);

      /* Gets a Matrix4x4 at the specified index. */
      float *(*getvector3box)(lua_State *L, int i);

      /* Gets a Unit at the specified index. */
      CApiUnit *(*getunit)(lua_State *L, int i);

      /* Gets the Lua state where the main scripts execute. */
      lua_State *(*getscriptenvironmentstate)();

      /* Returns true if the stack entry is a table. */
      int (*istable)(lua_State *L, int i);

      /* Returns true if the stack entry is a Vector3. */
      int (*isvector3_)(lua_State *L, int i);

      /* Returns true if the stack entry is a Vector3. */
      int (*isvector3)(lua_State *L, int i);

      /* Returns true if the stack entry is a Quaternion. */
      int (*isquaternion)(lua_State *L, int i);

      /* Returns true if the stack entry is a Matrix4x4. */
      int (*ismatrix4x4)(lua_State *L, int i);

      /* Pops the top value from the Lua stack. */
      void (*pop)(lua_State *L);

      /* Returns true if the stack entry is a UnitReference. */
      int (*isunitreference)(lua_State *L, int i);

      /* Reserved for expansion of the API. */
      void *reserved[32];
   };

   struct LoggingApi
   {
      /* Logs an informational message */
      void (*info)(const char *system, const char *info);

      /* Logs a warning message. */
      void (*warning)(const char *system, const char *warning);

      /* Logs an error message. */
      void (*error)(const char *system, const char *error);
   };

#ifdef __cplusplus
}
#endif
