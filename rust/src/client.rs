

use std::net::{TcpStream, UdpSocket};

use crate::protobufs::multi_pong::{State};
use crate::renderer::{Renderer, RendererType};
use crate::egui_renderer::{EGUIRenderer};

pub struct Client {
    coordinator_socket: Option<TcpStream>,
    coordinator_address: (String, u16),
    server_socket: UdpSocket,
    server_address: Option<(String, u16)>,
    state: State,
    token: String,
    renderer: Renderer,
}

fn connect(address: &(String, u16)) -> TcpStream {
    // println!("Attempting to connect to server at {address.first}:{port}...");

    let stream = TcpStream::connect(address)
        .expect("Failed to connect to the server");

    println!("Successfully connected to the server");
    return stream;
}

impl Client {
    pub fn new(coordinator_address: (String, u16), renderer_type: RendererType) -> Self {
        let renderer = match renderer_type {
            RendererType::EGUI => {
                Renderer::EGUI(EGUIRenderer::new())
            }
        };

        Self {
            coordinator_socket: Some(connect(&coordinator_address)),
            coordinator_address,
            server_address: None,
            server_socket: UdpSocket::bind("0.0.0.0").unwrap(),
            state: State::default(),
            token: String::new(),
            renderer,
        }
    }
}