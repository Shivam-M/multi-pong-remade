package com.sm.render;

import com.sm.protobufs.Pong.*;

public interface Renderer {

    void setup();
    void toggleFullscreen();
    void updateState(State state);
    void renderLoop();
    void cleanup();

}
