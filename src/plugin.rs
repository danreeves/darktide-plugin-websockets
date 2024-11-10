use crate::stingray_sdk::{lua_State, GetApiFunction, LoggingApi, LuaApi};
use crate::{PLUGIN, PLUGIN_NAME};
use std::collections::HashMap;
use std::mem;
use std::sync::{LazyLock, Mutex};

const LUA_REGISTRYINDEX: i32 = -10000;

pub struct Connection {
    uri: String,
    connect_callback_id: i32,
    message_callback_id: i32,
    close_callback_id: i32,
}

pub struct ConnectionList {
    connection_index: i32,
    connections: HashMap<i32, Connection>,
}

impl ConnectionList {
    pub fn add_connection(
        &mut self,
        uri: String,
        connect_callback_id: i32,
        message_callback_id: i32,
        close_callback_id: i32,
    ) -> i32 {
        self.connection_index += 1;
        let new_connection = Connection {
            uri,
            connect_callback_id,
            message_callback_id,
            close_callback_id,
        };
        self.connections
            .insert(self.connection_index, new_connection);

        self.connection_index
    }
}

static CONNECTIONS: LazyLock<Mutex<ConnectionList>> = LazyLock::new(|| {
    Mutex::new(ConnectionList {
        connection_index: 0,
        connections: HashMap::new(),
    })
});

pub(crate) struct Plugin {
    pub log: LoggingApi,
    pub lua: LuaApi,
}

extern "C" fn l_connect(l: *mut lua_State) -> i32 {
    // Safety: Plugin must have been initialized for this to be registered
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };

    if let Some(uri) = plugin.lua.tolstring(l, 1) {
        // TODO: need the lua.type function in order to type check these

        plugin.lua.pushvalue(l, 2);
        let connect_callback_id = plugin.lua.lib_ref(l, LUA_REGISTRYINDEX);
        plugin.lua.pushvalue(l, 3);
        let message_callback_id = plugin.lua.lib_ref(l, LUA_REGISTRYINDEX);
        plugin.lua.pushvalue(l, 4);
        let close_callback_id = plugin.lua.lib_ref(l, LUA_REGISTRYINDEX);

        let connection_id = CONNECTIONS.lock().unwrap().add_connection(
            uri.to_string_lossy().to_string(),
            connect_callback_id,
            message_callback_id,
            close_callback_id,
        );

        let userdata = plugin.lua.newuserdata(l, mem::size_of::<i32>()) as *mut i32;
        unsafe { userdata.write(connection_id) };
        1
    } else {
        0
    }
}

extern "C" fn l_send(l: *mut lua_State) -> i32 {
    // Safety: Plugin must have been initialized for this to be registered
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };

    let userdata = plugin.lua.touserdata(l, 1);
    let index = unsafe { *(userdata as *const i32) };
    if let Some(connection) = &CONNECTIONS.lock().unwrap().connections.get(&index) {
        plugin.lua.pushstring(
            l,
            format!("[l_send] This is connected to {}", &connection.uri),
        );
        return 1;
    }
    return 0;
}

impl Plugin {
    pub fn new(get_engine_api: GetApiFunction) -> Self {
        let log = LoggingApi::get(get_engine_api);
        let lua = LuaApi::get(get_engine_api);
        Self { log, lua }
    }

    pub fn setup_game(&self) {
        self.log
            .info(PLUGIN_NAME, format!("[setup_game] Initialising"));

        self.lua
            .add_module_function("WebSockets", "connect", l_connect);
        self.lua.add_module_function("WebSockets", "send", l_send);
    }

    pub fn update_game(&self, _dt: f32) {}

    pub fn shutdown_game(&self) {
        self.log.info(PLUGIN_NAME, "[shutdown_game] Shutting down");
    }
}

impl std::fmt::Debug for Plugin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("PluginApi")
    }
}
