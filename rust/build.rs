fn main() {
    prost_build::Config::new()
        .out_dir("src/protobufs")
        .compile_protos(&["../_protobufs/pong.proto"], &["../_protobufs"])
        .unwrap();
}