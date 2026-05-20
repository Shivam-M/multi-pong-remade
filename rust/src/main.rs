mod protobufs;
mod coordinator;
mod client;
mod renderer;
mod egui_renderer;

use coordinator::Coordinator;
use client::Client;
use std::{sync::Arc, vec};

use crate::renderer::RendererType;

fn main() {
    let address = "127.0.0.1";
    let port = 4999;
    // let addresses: Vec<(String, u16)> = Vec::new();

    let addresses = vec![
        ("127.0.0.1".to_string(), 5000),
        ("127.0.0.1".to_string(), 5001),
        ("127.0.0.1".to_string(), 5002),
        ("127.0.0.1".to_string(), 5003),
        ("127.0.0.1".to_string(), 5004)
    ];

    let coordinator = Arc::new(Coordinator::new(port, addresses).unwrap());
    coordinator.init();

    let client = Arc::new(Client::new((address.to_string(), port), RendererType::EGUI));
}
