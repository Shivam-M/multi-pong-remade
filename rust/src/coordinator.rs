use std::net::{TcpListener, TcpStream, SocketAddr, UdpSocket};
use std::collections::{HashMap, VecDeque};
use std::{thread, time};
use std::sync::Arc;

use prost::Message as ProstMessage;

use crate::protobufs::multi_pong::status::Phase;
use crate::protobufs::multi_pong::{Message, Query};
use crate::protobufs::multi_pong::message;

const MULTI_PONG_SERVER_CHECK_INTERVAL: u64 = 5;

pub struct Coordinator {
    port: u16,
    coordinator_socket: TcpListener,
    secret: String,
    clients: Vec<TcpStream>,
    searching_clients: VecDeque<TcpStream>,
    server_list: HashMap<(String, i16), Phase>,
}

impl Coordinator {
    pub fn new(port: u16, addresses: Vec<(String, i16)>) -> std::io::Result<Self> {
        let coordinator_socket = TcpListener::bind(("0.0.0.0", port))?;

        let mut server_list: HashMap<(String, i16), Phase> = HashMap::new();
        for address in addresses {
            server_list.insert(address, Phase::Unknown);
        }

        Ok(Self {
            port,
            coordinator_socket,
            secret: String::from(""),
            clients: Vec::new(),
            searching_clients: VecDeque::new(),
            server_list,
        })
    }

    pub fn init(self: &Arc<Self>) {
        let coordinator_clone = Arc::clone(self);
        let check_servers = thread::spawn(move || {
            coordinator_clone.check_servers();
        });

        let _ = check_servers.join();
    }

    fn check_servers(&self) {
        let query = Query {};
        let query_message: Message = Message { content: Some(message::Content::Query(query)) };

        loop {
            thread::sleep(time::Duration::from_secs(MULTI_PONG_SERVER_CHECK_INTERVAL));

            // println!("begin search");

            for address in self.server_list.keys() {
                self.send_message_to_server(address.clone(), &query_message);
            }
        }
    }

    fn send_message_to_server(&self, (host, port): (String, i16), message: &Message) -> Option<Message> {
        let server_socket = UdpSocket::bind("0.0.0.0:0").unwrap();

        let mut message_buffer = Vec::new();
        message.encode(&mut message_buffer).expect("Failed to encode message");
        server_socket.send_to(&message_buffer, format!("{host}:{port}")).unwrap();

        let mut buffer = [0; 1024];
        let (length, address) = match server_socket.recv_from(&mut buffer) {
            // validate address and maybe add the coordinator-server secret to these messages
            Ok(result) => result,
            Err(_) => {
                println!("Server {host}:{port} is unavailable");
                return None;
            }
        };

        // println!("Received {} bytes from {}", length, address);

        let received_message = match Message::decode(&buffer[..length]) {
            Ok(m) => m,
            Err(err) => {
                println!("Warning: Failed to decode protobuf message from server {host}:{port} - {err}");
                return None;
            }
        };

        match &received_message.content {
            Some(message::Content::Status(status)) => {
                if status.phase() == Phase::Waiting {
                    println!("Server {host}:{port} is available");
                } else {
                    println!("Server {host}:{port} is busy");
                }
                // self.server_list[(host, port)] = status.phase();
            }
            Some(message::Content::Tokens(tokens)) => {
                println!("Got tokens {:?}", tokens);
            }
            Some(other) => {
                println!("Warning: Received invalid message type {:?} from server {host}:{port}", other);
            }
            None => {
                println!("No content");
            }
        }

        return Some(received_message);
    }
}
