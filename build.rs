extern crate bindgen;

use std::env;
use std::path::PathBuf;

const HEADER_NAME: &str = "src/plugin_api.h";

fn main() {
    println!("cargo:rerun-if-changed={}", HEADER_NAME);

    let bindings = bindgen::Builder::default()
        .header(HEADER_NAME)
        .rustified_enum("PluginApiID")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .generate()
        .expect("Unable to generate bindings");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");

    if cfg!(debug_assertions) {
        bindings
            .write_to_file(out_path.join("../../../bindings.rs"))
            .expect("Couldn't write bindings to debug path");
    }
}
