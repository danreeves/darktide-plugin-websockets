#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(clippy::type_complexity)]
#![allow(unused)]

pub mod bindings {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::raw::c_void;

use bindings::lua_CFunction;
pub use bindings::lua_State;
pub use bindings::GetApiFunction;
pub use bindings::PluginApi;
pub use bindings::PluginApiID;

impl std::default::Default for PluginApi {
    fn default() -> Self {
        Self {
            version: 65,
            flags: 3,
            setup_game: None,
            update_game: None,
            shutdown_game: None,
            get_name: None,
            loaded: None,
            start_reload: None,
            unloaded: None,
            finish_reload: None,
            setup_resources: None,
            shutdown_resources: None,
            unregister_world: None,
            register_world: None,
            get_hash: None,
            unkfunc13: None,
            unkfunc14: None,
            unkfunc15: None,
            debug_draw: None,
        }
    }
}

#[cfg(debug_assertions)]
fn get_engine_api(f: GetApiFunction, id: PluginApiID) -> *mut c_void {
    let f = f.expect("'GetApiFunction' is always passed by the engine");
    unsafe { f(id as u32) }
}

#[cfg(not(debug_assertions))]
fn get_engine_api(f: GetApiFunction, id: PluginApiID) -> *mut c_void {
    // `Option::unwrap` still generates several instructions in
    // optimized code.
    let f = unsafe { f.unwrap_unchecked() };

    unsafe { f(id as u32) }
}

pub struct LoggingApi {
    info: unsafe extern "C" fn(*const c_char, *const c_char),
    warning: unsafe extern "C" fn(*const c_char, *const c_char),
    error: unsafe extern "C" fn(*const c_char, *const c_char),
}

impl LoggingApi {
    pub fn get(f: GetApiFunction) -> Self {
        let api = unsafe {
            let api = get_engine_api(f, PluginApiID::LOGGING_API_ID);
            api as *mut bindings::LoggingApi
        };

        unsafe {
            Self {
                info: (*api).info.unwrap_unchecked(),
                warning: (*api).warning.unwrap_unchecked(),
                error: (*api).error.unwrap_unchecked(),
            }
        }
    }

    pub fn info(&self, system: impl Into<Vec<u8>>, message: impl Into<Vec<u8>>) {
        let system = CString::new(system).expect("Invalid CString");
        let message = CString::new(message).expect("Invalid CString");
        unsafe {
            (self.info)(system.as_ptr(), message.as_ptr());
        }
    }

    pub fn warning(&self, system: impl Into<Vec<u8>>, message: impl Into<Vec<u8>>) {
        let system = CString::new(system).expect("Invalid CString");
        let message = CString::new(message).expect("Invalid CString");
        unsafe {
            (self.warning)(system.as_ptr(), message.as_ptr());
        }
    }

    pub fn error(&self, system: impl Into<Vec<u8>>, message: impl Into<Vec<u8>>) {
        let system = CString::new(system).expect("Invalid CString");
        let message = CString::new(message).expect("Invalid CString");
        unsafe {
            (self.error)(system.as_ptr(), message.as_ptr());
        }
    }
}

#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LuaType {
    None = -1,
    Nil = 0,
    Boolean = 1,
    LightUserdata = 2,
    Number = 3,
    String = 4,
    Table = 5,
    Function = 6,
    Userdata = 7,
    Thread = 8,
    Unknown(i32),
}

impl From<i32> for LuaType {
    fn from(value: i32) -> Self {
        match value {
            -1 => Self::None,
            0 => Self::Nil,
            1 => Self::Boolean,
            2 => Self::LightUserdata,
            3 => Self::Number,
            4 => Self::String,
            5 => Self::Table,
            6 => Self::Function,
            7 => Self::Userdata,
            8 => Self::Thread,
            _ => Self::Unknown(value),
        }
    }
}

impl std::fmt::Display for LuaType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::None => write!(f, "None"),
            Self::Nil => write!(f, "Nil"),
            Self::Boolean => write!(f, "Boolean"),
            Self::LightUserdata => write!(f, "LightUserdata"),
            Self::Number => write!(f, "Number"),
            Self::String => write!(f, "String"),
            Self::Table => write!(f, "Table"),
            Self::Function => write!(f, "Function"),
            Self::Userdata => write!(f, "Userdata"),
            Self::Thread => write!(f, "Thread"),
            Self::Unknown(val) => write!(f, "Unkown({})", val),
        }
    }
}

pub struct LuaApi {
    add_module_function: unsafe extern "C" fn(*const c_char, *const c_char, lua_CFunction),
    tolstring: unsafe extern "C" fn(*mut lua_State, i32, *mut usize) -> *const c_char,
    pushstring: unsafe extern "C" fn(*mut lua_State, *const c_char),
    getscriptenvironmentstate: unsafe extern "C" fn() -> *mut lua_State,
    rawgeti: unsafe extern "C" fn(*mut lua_State, i32, i32),
    call: unsafe extern "C" fn(*mut lua_State, i32, i32),
    newuserdata: unsafe extern "C" fn(*mut lua_State, usize) -> *mut c_void,
    lib_newmetatable: unsafe extern "C" fn(*mut lua_State, *const c_char) -> i32,
    setmetatable: unsafe extern "C" fn(*mut lua_State, i32) -> i32,
    lua_typename: unsafe extern "C" fn(*mut lua_State, i32) -> *const c_char,
    lua_type: unsafe extern "C" fn(*mut lua_State, i32) -> i32,
    pushboolean: unsafe extern "C" fn(*mut lua_State, i32),
    pushvalue: unsafe extern "C" fn(*mut lua_State, i32),
    touserdata: unsafe extern "C" fn(*mut lua_State, i32) -> *mut c_void,
    lib_openlib:
        unsafe extern "C" fn(*mut lua_State, *const c_char, *const bindings::luaL_Reg, i32),
    rawset: unsafe extern "C" fn(*mut lua_State, i32),
    pop: unsafe extern "C" fn(*mut lua_State),
    lib_ref: unsafe extern "C" fn(*mut lua_State, i32) -> i32,
}

impl LuaApi {
    pub fn get(f: GetApiFunction) -> Self {
        let api = unsafe {
            let api = get_engine_api(f, PluginApiID::LUA_API_ID);
            api as *mut bindings::LuaApi
        };

        unsafe {
            Self {
                add_module_function: (*api).add_module_function.unwrap_unchecked(),
                tolstring: (*api).tolstring.unwrap_unchecked(),
                pushstring: (*api).pushstring.unwrap_unchecked(),
                getscriptenvironmentstate: (*api).getscriptenvironmentstate.unwrap_unchecked(),
                rawgeti: (*api).rawgeti.unwrap_unchecked(),
                call: (*api).call.unwrap_unchecked(),
                newuserdata: (*api).newuserdata.unwrap_unchecked(),
                lib_newmetatable: (*api).lib_newmetatable.unwrap_unchecked(),
                setmetatable: (*api).setmetatable.unwrap_unchecked(),
                lua_typename: (*api).lua_typename.unwrap_unchecked(),
                lua_type: (*api).lua_type.unwrap_unchecked(),
                pushboolean: (*api).pushboolean.unwrap_unchecked(),
                pushvalue: (*api).pushvalue.unwrap_unchecked(),
                touserdata: (*api).touserdata.unwrap_unchecked(),
                lib_openlib: (*api).lib_openlib.unwrap_unchecked(),
                rawset: (*api).rawset.unwrap_unchecked(),
                pop: (*api).pop.unwrap_unchecked(),
                lib_ref: (*api).lib_ref.unwrap_unchecked(),
            }
        }
    }

    pub fn lib_ref(&self, L: *mut lua_State, t: i32) -> i32 {
        unsafe { (self.lib_ref)(L, t) }
    }

    pub fn pop(&self, L: *mut lua_State) {
        unsafe { (self.pop)(L) }
    }

    pub fn rawset(&self, L: *mut lua_State, idx: i32, n: i32) {
        unsafe { (self.rawset)(L, idx) }
    }

    pub fn lib_openlib(
        &self,
        L: *mut lua_State,
        libname: impl Into<Vec<u8>>,
        l: *const bindings::luaL_Reg,
        nup: i32,
    ) {
        let libname = CString::new(libname).expect("Invalid CString");
        unsafe { (self.lib_openlib)(L, libname.as_ptr(), l, nup) }
    }

    pub fn touserdata(&self, L: *mut lua_State, idx: i32) -> *mut c_void {
        unsafe { (self.touserdata)(L, idx) }
    }

    pub fn pushvalue(&self, L: *mut lua_State, idx: i32) {
        unsafe { (self.pushvalue)(L, idx) }
    }

    pub fn pushboolean(&self, L: *mut lua_State, b: bool) {
        unsafe { (self.pushboolean)(L, b as i32) }
    }

    pub fn lua_type(&self, L: *mut lua_State, idx: i32) -> LuaType {
        LuaType::from(unsafe { (self.lua_type)(L, idx) })
    }

    pub fn lua_typename(&self, L: *mut lua_State, idx: i32) -> Option<&CStr> {
        let c = unsafe { (self.lua_typename)(L, idx) };

        if c.is_null() {
            None
        } else {
            Some(unsafe { CStr::from_ptr(c) })
        }
    }

    pub fn getscriptenvironmentstate(&self) -> *mut lua_State {
        unsafe { (self.getscriptenvironmentstate)() }
    }

    pub fn add_module_function(
        &self,
        module: impl Into<Vec<u8>>,
        name: impl Into<Vec<u8>>,
        cb: extern "C" fn(*mut lua_State) -> i32,
    ) {
        let module = CString::new(module).expect("Invalid CString");
        let name = CString::new(name).expect("Invalid CString");

        unsafe { (self.add_module_function)(module.as_ptr(), name.as_ptr(), Some(cb)) }
    }

    pub fn tolstring(&self, L: *mut lua_State, idx: i32) -> Option<&CStr> {
        let mut len: usize = 0;

        let c = unsafe { (self.tolstring)(L, idx, &mut len as *mut _) };

        if len == 0 {
            None
        } else {
            // Safety: As long as `len > 0`, Lua guarantees the constraints that `CStr::from_ptr`
            // requires.
            Some(unsafe { CStr::from_ptr(c) })
        }
    }

    pub fn pushstring(&self, L: *mut lua_State, s: impl Into<Vec<u8>>) {
        let s = CString::new(s).expect("Invalid CString");
        unsafe { (self.pushstring)(L, s.as_ptr()) }
    }

    pub fn rawgeti(&self, L: *mut lua_State, idx: i32, n: i32) {
        unsafe { (self.rawgeti)(L, idx, n) }
    }

    pub fn call(&self, L: *mut lua_State, nargs: i32, nresults: i32) {
        unsafe { (self.call)(L, nargs, nresults) }
    }

    pub fn newuserdata(&self, L: *mut lua_State, size: usize) -> *mut c_void {
        unsafe { (self.newuserdata)(L, size) }
    }

    pub fn lib_newmetatable(&self, L: *mut lua_State, name: impl Into<Vec<u8>>) -> i32 {
        let name = CString::new(name).expect("Invalid CString");
        unsafe { (self.lib_newmetatable)(L, name.as_ptr()) }
    }

    pub fn setmetatable(&self, L: *mut lua_State, idx: i32) -> i32 {
        unsafe { (self.setmetatable)(L, idx) }
    }
}
