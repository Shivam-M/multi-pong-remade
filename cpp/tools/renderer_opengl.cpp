#include "renderer_opengl.h"
#include "logger.h"
#include "../client.h"

#include <protobufs/pong.pb.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

static const char* VERTEX_SHADER = R"(
#version 330 core

layout (location = 0) in vec2 position;

uniform mat4 projection;

void main() {
    gl_Position = projection * vec4(position, 0.0, 1.0);
}
)";

static const char* FRAGMENT_SHADER = R"(
#version 330 core

out vec4 FragColour;

void main() {
    FragColour = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

bool OpenGLRenderer::setup() {
    Logger::debug("Initialising OpenGL-based renderer...");

    if (!glfwInit()) {
        Logger::error("Failed to initialise GLFW");
        return false;
    }

    window = glfwCreateWindow(1280, 720, "Pong", NULL, NULL);

    if (!window) {
        glfwTerminate();
        Logger::error("Failed to create GLFW window");
        return false;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::error("Failed to initialise GLAD");
        return false;
    }

    glClearColor(0.155f, 0.155f, 0.155f, 0.f);
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, keyboard_callback);

    shader = create_shader_program(VERTEX_SHADER, FRAGMENT_SHADER);
    generate_buffers();

    return true;
}

GLuint OpenGLRenderer::compile_shader(GLenum type, const char* source) {
    Logger::debug("Compiling shader type ", type);

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        Logger::error("Shader compilation error: ", log);
        return 0;
    }

    return shader;
}

GLuint OpenGLRenderer::create_shader_program(const char* vertex_shader_source, const char* fragment_shader_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    if (!vertex_shader || !fragment_shader) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        Logger::error("Shader linking error: ", log);
        return 0;
    }

    Logger::debug("Created shader program successfully");
    return program;
}

void OpenGLRenderer::generate_buffers() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void OpenGLRenderer::keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    OpenGLRenderer* renderer = static_cast<OpenGLRenderer*>(glfwGetWindowUserPointer(window));
    if (!renderer) return;

    Client* client = renderer->client;
    if (!client) return;

    if (action == GLFW_RELEASE) {
        if (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) {
            client->send_move(multi_pong::STOP);
        }
        return;
    }

    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_UP:
            client->send_move(multi_pong::UP);
            break;
        case GLFW_KEY_DOWN:
            client->send_move(multi_pong::DOWN);
            break;
        case GLFW_KEY_F:
            renderer->toggle_fullscreen();
            break;
        }
}

void OpenGLRenderer::toggle_fullscreen() {
    if (glfwGetWindowMonitor(window) == NULL) {
        const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        glfwSetWindowMonitor(window, NULL, 100, 100, 1280, 720, 0);
    }
}

// batching these draw calls would only provide a negligible performance boost
void OpenGLRenderer::render_quad(float x, float y, float width, float height) {
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;

    float vertices[8] = {
        x - half_width, y - half_height,
        x + half_width, y - half_height,
        x + half_width, y + half_height,
        x - half_width, y + half_height
    };

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void OpenGLRenderer::render_window() {
    const multi_pong::State& state = client->get_state();

    render_quad(state.ball().x(), state.ball().y(), MULTI_PONG_BALL_WIDTH, MULTI_PONG_BALL_HEIGHT);

    render_quad(MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.player_1().paddle_location(), MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);

    render_quad(1.0f - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.player_2().paddle_location(), MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);
}

void OpenGLRenderer::render_loop() {
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glBindVertexArray(vao);

    // not waiting for state updates
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        render_window();
        glfwSwapBuffers(window);
    }

    glfwTerminate();
}

OpenGLRenderer::~OpenGLRenderer() {
    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}