use std::ffi::{c_char, CString};
use std::sync::OnceLock;

mod plugin;
mod stingray_sdk;

use plugin::Plugin;
use stingray_sdk::{GetApiFunction, PluginApi, PluginApiID};

pub const PLUGIN_NAME: &str = "TestingRustPlugin";

static PLUGIN: OnceLock<Plugin> = OnceLock::new();

#[no_mangle]
pub extern "C" fn get_name() -> *const c_char {
    let s = CString::new(PLUGIN_NAME).expect("Failed to create CString from plugin name");
    s.as_ptr()
}

#[no_mangle]
pub extern "C" fn setup_game(get_engine_api: GetApiFunction) {
    let plugin = Plugin::new(get_engine_api);
    plugin.setup_game();
    PLUGIN
        .set(plugin)
        .expect("Failed to initalize global plugin object.");
}

#[no_mangle]
pub extern "C" fn shutdown_game() {
    // Safety: The engine ensures that `setup_game` was called before this, so `PLUGIN` has been
    // initialized.
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };
    plugin.shutdown_game();
}

#[no_mangle]
pub extern "C" fn update_game(dt: f32) {
    // Safety: The engine ensures that `setup_game` was called before this, so `PLUGIN` has been
    // initialized.
    let plugin = unsafe { PLUGIN.get().unwrap_unchecked() };
    plugin.update_game(dt);
}

#[no_mangle]
pub extern "C" fn get_plugin_api(id: PluginApiID) -> *mut PluginApi {
    if id == PluginApiID::PLUGIN_API_ID {
        let api = PluginApi {
            get_name: Some(get_name),
            setup_game: Some(setup_game),
            update_game: Some(update_game),
            shutdown_game: Some(shutdown_game),
            ..Default::default()
        };

        Box::into_raw(Box::new(api))
    } else {
        std::ptr::null_mut()
    }
}
