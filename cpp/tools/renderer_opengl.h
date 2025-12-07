#pragma once

#include "renderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Client;

class OpenGLRenderer : public Renderer {
	private:
		Client* client;
		GLFWwindow* window;
		GLuint shader = 0, vao = 0, vbo = 0;

		void render_window();
		void render_quad(float x, float y, float width, float height);
		void generate_buffers();
		
		static GLuint compile_shader(GLenum type, const char* source);
		static GLuint create_shader_program(const char* vertex_shader_source, const char* fragment_shader_source);
		static void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

	public:
		OpenGLRenderer() {}
		~OpenGLRenderer();

		bool setup(Client* c) override;
		void toggle_fullscreen() override;
		void update_state() override {};
		void render_loop() override;
};
