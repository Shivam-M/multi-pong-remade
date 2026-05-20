use crate::{client::Client, egui_renderer::EGUIRenderer};

pub enum RendererType {
    EGUI,
}

pub enum Renderer {
    EGUI(EGUIRenderer),
}

impl Renderer {
    pub fn setup(&mut self) {
        match self {
            Renderer::EGUI(r) => r.setup(),
        }
    }

    pub fn update_state(&mut self, state: &crate::protobufs::multi_pong::State) {
        match self {
            Renderer::EGUI(r) => r.update_state(state),
        }
    }
}

// pub trait Renderer {
//     fn setup(&mut self, client: &mut Client) -> bool;
//     fn toggle_fullscreen(&mut self);
//     fn update_state(&mut self);
//     fn render_loop(&mut self);
// }
