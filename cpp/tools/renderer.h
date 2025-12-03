#pragma once

class Renderer {
	public:
		virtual ~Renderer() = default;

		virtual bool setup() = 0;
		virtual void toggle_fullscreen() = 0;
		virtual void update_state() = 0;
		virtual void render_loop() = 0;
};
