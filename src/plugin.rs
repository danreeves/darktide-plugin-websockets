use std::mem;

use crate::stingray_sdk::{
    lua_State, GetApiFunction, LoggingApi, LuaApi, LuaType, LUA_REGISTRYINDEX,
};
use crate::{PLUGIN, PLUGIN_NAME};
pub(crate) struct Plugin {
    pub log: LoggingApi,
    pub lua: LuaApi,
}

extern "C" fn l_connect(l: *mut lua_State) -> i32 {
    // Safety: Plugin must have been initialized for this to be registered
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };
    plugin.log.info(PLUGIN_NAME, "[l_connect]");

    let param_1_type = plugin.lua.lua_type(l, 1);
    let param_2_type = plugin.lua.lua_type(l, 2);

    if param_1_type != LuaType::String {
        plugin.log.error(
            PLUGIN_NAME,
            format!("connect: first argument is not a string ({})", param_1_type),
        );
        return 0;
    }

    if param_2_type != LuaType::Function {
        plugin.log.error(
            PLUGIN_NAME,
            format!(
                "connect: second argument is not a function ({})",
                param_2_type
            ),
        );
        return 0;
    }

    if let Some(uri) = plugin.lua.tolstring(l, 1) {
        plugin.lua.pushvalue(l, 2);
        let message_callback_id = plugin.lua.lib_ref(l, LUA_REGISTRYINDEX);

        if let Ok(conns) = CONNECTIONS.lock().as_mut() {
            let connection_id =
                conns.add_connection(uri.to_string_lossy().to_string(), message_callback_id);

            let userdata = plugin.lua.newuserdata(l, mem::size_of::<i32>()) as *mut i32;
            unsafe { userdata.write(connection_id) };
            return 1;
        } else {
            plugin
                .log
                .error(PLUGIN_NAME, "[l_connect] Error getting connections lock");
            return 0;
        }
    } else {
        return 0;
    }
}

extern "C" fn l_send(l: *mut lua_State) -> i32 {
    // Safety: Plugin must have been initialized for this to be registered
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };
    plugin.log.info(PLUGIN_NAME, "[l_send]");
    // if let Ok(conns) = CONNECTIONS.lock().as_mut() {
    //     if let Some(message) = plugin.lua.tolstring(l, 2) {
    //         let userdata = plugin.lua.touserdata(l, 1);
    //         let index = unsafe { *(userdata as *const i32) };
    //         if let Some(connection) = conns.connections.get_mut(&index) {
    //             if let Ok(_) = connection
    //                 .websocket
    //                 .write(Message::Text(message.to_string_lossy().to_string()))
    //             {
    //                 // Message sent successfully
    //                 plugin.lua.pushboolean(l, true);
    //                 return 1;
    //             }
    //             // Failed to send message
    //             plugin
    //                 .log
    //                 .error(PLUGIN_NAME, "[l_send] Failed to send message");
    //             plugin.lua.pushboolean(l, false);
    //             return 1;
    //         }
    //     }
    // } else {
    //     plugin
    //         .log
    //         .error(PLUGIN_NAME, "[l_send] Error getting connections lock");
    // }
    // No message passed to send
    plugin
        .log
        .error(PLUGIN_NAME, "[l_send] No message passed to send");
    plugin.lua.pushboolean(l, false);
    return 1;
}

extern "C" fn l_close(l: *mut lua_State) -> i32 {
    // Safety: Plugin must have been initialized for this to be registered
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };
    plugin.log.info(PLUGIN_NAME, "[l_close]");
    let userdata = plugin.lua.touserdata(l, 1);
    let index = unsafe { *(userdata as *const i32) };
    // if let Ok(conns) = CONNECTIONS.lock().as_mut() {
    //     if let Some(connection) = conns.connections.get_mut(&index) {
    //         if let Ok(_) = connection.websocket.close(None) {
    //             return 0;
    //         }
    //     }
    // } else {
    //     plugin
    //         .log
    //         .error(PLUGIN_NAME, "[l_close] Error getting connections lock");
    // }
    return 0;
}

impl Plugin {
    pub fn new(get_engine_api: GetApiFunction) -> Self {
        let log = LoggingApi::get(get_engine_api);
        let lua = LuaApi::get(get_engine_api);
        Self { log, lua }
    }

    pub fn setup_game(&self) {
        self.log.info(PLUGIN_NAME, format!("[setup_game]"));

        self.lua
            .add_module_function("WebSockets", "connect", l_connect);
        self.lua.add_module_function("WebSockets", "send", l_send);
        self.lua.add_module_function("WebSockets", "close", l_close);
    }

    pub fn update_game(&self, _dt: f32) {
        let l = self.lua.getscriptenvironmentstate();

        let mut to_remove = Vec::new();

        if let Ok(conns) = CONNECTIONS.lock().as_mut() {
            conns
                .connections
                .iter_mut()
                .for_each(|(index, connection)| {
                    match connection.websocket.flush() {
                        Ok(_) => {}
                        Err(e) => {
                            self.log.info(PLUGIN_NAME, format!("Error flushing {}", e));
                            to_remove.push(*index);
                        }
                    }

                    self.log.info(
                        PLUGIN_NAME,
                        format!("Reading message from {}", connection.uri),
                    );
                    if connection.websocket.can_read() {
                        match connection.websocket.read() {
                            Ok(message) => {
                                if let Message::Text(text) = message {
                                    self.log.info(PLUGIN_NAME, format!("{}", text));
                                    // self.lua.rawgeti(
                                    //     l,
                                    //     LUA_REGISTRYINDEX,
                                    //     connection.message_callback_id,
                                    // );
                                    // self.lua.pushstring(l, text);
                                    // self.lua.call(l, 1, 0);
                                } else {
                                    self.log.info(PLUGIN_NAME, "Message is not text");
                                }
                            }
                            Err(e) => {
                                self.log.info(PLUGIN_NAME, format!("Error reading {}", e));
                                to_remove.push(*index);
                            }
                        }
                    }
                });
        } else {
            self.log.error(
                PLUGIN_NAME,
                "[update_game first loop] Error getting connections lock",
            );
        }

        for index in to_remove {
            match CONNECTIONS.lock().as_mut() {
                Ok(conns) => {
                    if conns.connections.contains_key(&index) {
                        conns.connections.remove(&index);
                    }
                }
                Err(e) => {
                    self.log.error(
                        PLUGIN_NAME,
                        format!(
                            "[update_game to_remove] Error getting connections lock: {}",
                            e
                        ),
                    );
                }
            }
        }
    }

    pub fn shutdown_game(&self) {
        self.log.info(PLUGIN_NAME, "[shutdown_game]");
    }
}

impl std::fmt::Debug for Plugin {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("PluginApi")
    }
}
