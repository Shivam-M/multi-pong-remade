pub struct EGUIRenderer {}

impl EGUIRenderer {
    pub fn new() -> Self {
        Self {}
    }

    pub fn setup(&mut self) {}

    pub fn update_state(&mut self, state: &crate::protobufs::multi_pong::State) {}
}
