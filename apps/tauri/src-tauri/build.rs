use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    tauri_build::build();

    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR"));
    let repo_root = manifest_dir
        .parent()
        .and_then(|p| p.parent())
        .and_then(|p| p.parent())
        .expect("src-tauri must live under apps/tauri");

    let kernel_include = repo_root.join("kernel").join("include");
    let preferred_lib_dir = repo_root
        .join("kernel")
        .join("out")
        .join("build")
        .join("src")
        .join("Release");
    let fallback_lib_dir = repo_root
        .join("kernel")
        .join("out")
        .join("build")
        .join("src")
        .join("Debug");
    let kernel_lib_dir = if preferred_lib_dir.join("chem_kernel.lib").exists() {
        preferred_lib_dir
    } else {
        fallback_lib_dir
    };

    println!("cargo:rerun-if-changed=native/sealed_kernel_bridge.h");
    println!("cargo:rerun-if-changed=native/sealed_kernel_bridge_internal.h");
    println!(
        "cargo:rerun-if-changed={}",
        repo_root.join("kernel").join("include").display()
    );
    println!(
        "cargo:rustc-link-search=native={}",
        kernel_lib_dir.display()
    );
    println!("cargo:rustc-link-lib=static=chem_kernel");

    let mut bridge_sources = fs::read_dir(manifest_dir.join("native"))
        .expect("native bridge directory")
        .map(|entry| entry.expect("native bridge entry").path())
        .filter(|path| path.extension().is_some_and(|extension| extension == "cpp"))
        .collect::<Vec<_>>();
    bridge_sources.sort();

    let mut bridge_build = cc::Build::new();
    bridge_build.cpp(true).std("c++20").include(kernel_include);
    for source in bridge_sources {
        println!("cargo:rerun-if-changed={}", source.display());
        bridge_build.file(source);
    }
    bridge_build.compile("sealed_kernel_bridge");
}
