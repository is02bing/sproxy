fn main() {
    println!("cargo:rerun-if-changed=c/");

    cc::Build::new()
        .file("c/proxy.c")
        .file("c/tunnel.c")
        .file("c/trace.c")
        .file("c/tcp.c")
        .file("c/dns.c")
        .file("c/doh.c")
        .file("c/dot.c")
        .file("c/hashmap.c")
        .file("c/base.c")
        .file("c/utils.c")
        .file("c/filemacro.c")
        .file("c/json.c")
        .flag("-std=c99")
        .flag("-D_XOPEN_SOURCE=600")
        .flag("-D_DEFAULT_SOURCE")
        .flag("-D_GNU_SOURCE")
        .flag("-Wall")
        .flag("-O2")
        .compile("cproxy");

    // 链接 C 依赖库
    println!("cargo:rustc-link-lib=event_core");
    println!("cargo:rustc-link-lib=event_extra");
    println!("cargo:rustc-link-lib=config");
    println!("cargo:rustc-link-lib=ssl");
    println!("cargo:rustc-link-lib=crypto");
    println!("cargo:rustc-link-lib=event_openssl");
    println!("cargo:rustc-link-lib=ssh2");
    println!("cargo:rustc-link-lib=nftables");
}
