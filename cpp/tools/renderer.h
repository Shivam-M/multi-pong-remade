#pragma once

class Client;

class Renderer {
	public:
		virtual ~Renderer() = default;

		virtual bool setup(Client* c) = 0;
		virtual void toggle_fullscreen() = 0;
		virtual void update_state() = 0;
		virtual void render_loop() = 0;
};
