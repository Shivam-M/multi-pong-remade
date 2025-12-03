#pragma once

#include "renderer.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Client;

class OpenGLRenderer : public Renderer {
	private:
		Client* client;
		GLFWwindow* window;
		GLuint shader, vao, vbo;

		void render_window();
		void render_quad(float x, float y, float width, float height);
		void generate_buffers();
		
		static GLuint compile_shader(GLenum type, const char* source);
		static GLuint create_shader_program(const char* vertex_shader_source, const char* fragment_shader_source);
		static void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

	public:
		OpenGLRenderer(Client* c) : client(c) {}
		~OpenGLRenderer();

		bool setup() override;
		void toggle_fullscreen() override;
		void update_state() override {};
		void render_loop() override;
};
