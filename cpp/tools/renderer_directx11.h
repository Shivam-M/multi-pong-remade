#pragma once

#include "renderer.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

class Client;

class DirectX11Renderer : public Renderer {
    private:
        Client* client;
        HWND window;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* device_context = nullptr;
        IDXGISwapChain* swap_chain = nullptr;
        ID3D11RenderTargetView* render_target_view = nullptr;
        ID3DBlob* vertex_shader_blob = nullptr;
        ID3DBlob* pixel_shader_blob = nullptr;
        ID3D11VertexShader* vertex_shader;
        ID3D11PixelShader* pixel_shader;
        ID3D11InputLayout* input_layout = nullptr;
        ID3D11Buffer* vbo = nullptr;

        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
        
        bool initialise_device();
        ID3DBlob* compile_shader(const char* shader_source, LPCSTR entrypoint, LPCSTR target);
        bool compile_shaders();
        void generate_buffers();
        void render_quad(float x, float y, float w, float h);
        void render_window();

    public:
        DirectX11Renderer() {}
        ~DirectX11Renderer();

        bool setup(Client* c) override;
        void toggle_fullscreen() override;
        void update_state() override {};
        void render_loop() override;
};
