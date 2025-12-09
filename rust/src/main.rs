mod protobufs;

use protobufs::multi_pong::{Message, Query};
use crate::protobufs::multi_pong::message;

fn main() {
    let query = Query {};
    let query_message: Message = Message { content: Some(message::Content::Query(query)) };

    println!("Query message: {:?}", query_message);
}
