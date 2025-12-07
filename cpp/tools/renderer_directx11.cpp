#include "renderer_directx11.h"
#include "logger.h"
#include "../client.h"

#include <protobufs/pong.pb.h>

static const char* VERTEX_SHADER = R"(
struct VS_INPUT {
    float2 pos : POSITION;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
};

PS_INPUT vs_main(VS_INPUT input) {
    PS_INPUT output;

    // should use a projection matrix here instead
    float ndcX = input.pos.x * 2.0f - 1.0f;
    float ndcY = 1.0f - input.pos.y * 2.0f;
    output.pos = float4(ndcX, ndcY, 0.0f, 1.0f);

    return output;
}
)";

static const char* PIXEL_SHADER = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
};

float4 ps_main(PS_INPUT input) : SV_TARGET {
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}
)";

LRESULT CALLBACK DirectX11Renderer::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    }

    DirectX11Renderer* renderer = (DirectX11Renderer*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (renderer) {
        return renderer->HandleMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT DirectX11Renderer::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                    client->send_move(multi_pong::Direction::UP);
                    break;
                case VK_DOWN:
                    client->send_move(multi_pong::Direction::DOWN);
                    break;
                }
            return 0;

        case WM_KEYUP:
            switch (wParam) {
                case VK_UP:
                    client->send_move(multi_pong::Direction::STOP);
                    break;
                case VK_DOWN:
                    client->send_move(multi_pong::Direction::STOP);
                    break;
                }
            return 0;

        case WM_SIZE:
            return 0;
        }

    return DefWindowProc(hwnd, message, wParam, lParam);
}


bool DirectX11Renderer::setup(Client* c) {
    Logger::debug("Initialising DirectX11-based renderer...");

    client = c;

    static constexpr LPCWSTR CLASS_NAME = L"MultiPongWindowClass";

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.lpszClassName = CLASS_NAME;

    RegisterClassExW(&window_class);

    window = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Multi Pong Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr, nullptr,
        window_class.hInstance,
        this
    );

    if (!window) {
        Logger::error("Failed to create window: ", GetLastError());
        return false;
    }

    ShowWindow(window, SW_SHOW);

    if (!initialise_device()) return false;
    if (!compile_shaders()) return false;
    generate_buffers();

    return true;
}

bool DirectX11Renderer::initialise_device() {
    Logger::debug("Creating device and swap chain...");

    DXGI_SWAP_CHAIN_DESC swap_chain_description = { 0 };
    swap_chain_description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_description.SampleDesc.Count = 1;
    swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_description.BufferCount = 1;
    swap_chain_description.OutputWindow = window;
    swap_chain_description.Windowed = true;

    D3D_FEATURE_LEVEL feature_level;

    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &swap_chain_description,
        &swap_chain,
        &device,
        &feature_level,
        &device_context);

    if (FAILED(result)) {
        Logger::error("Failed to create the device and swap chain");
        return false;
    }
    
    ID3D11Texture2D* back_buffer = nullptr;
    result = swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);

    if (FAILED(result)) {
        Logger::error("Failed to get back buffer");
        return false;
    }

    result = device->CreateRenderTargetView(back_buffer, 0, &render_target_view);
    back_buffer->Release();

    if (FAILED(result)) {
        Logger::error("Failed to create render target view");
        return false;
    }

    Logger::debug("Created device / swap chain / render target view");
    return true;
}

ID3DBlob* DirectX11Renderer::compile_shader(const char* shader_source, LPCSTR entrypoint, LPCSTR target) {
    ID3DBlob* shader_blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    
    HRESULT result = D3DCompile(
        shader_source,
        strlen(shader_source),
        nullptr,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint,
        target,
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &shader_blob,
        &error_blob
    );

    if (FAILED(result)) {
        if (error_blob) {
            const char* error = static_cast<const char*>(error_blob->GetBufferPointer());
            Logger::error("Shader compilation error: ", error);
            error_blob->Release();
        }
        if (shader_blob) shader_blob->Release();
    }

    return shader_blob;
}

bool DirectX11Renderer::compile_shaders() {
    Logger::debug("Compiling shaders...");

    vertex_shader_blob = compile_shader(VERTEX_SHADER, "vs_main", "vs_5_0");
    pixel_shader_blob = compile_shader(PIXEL_SHADER, "ps_main", "ps_5_0");

    if (!vertex_shader_blob || !pixel_shader_blob) return false;

    device->CreateVertexShader(vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), nullptr, &vertex_shader);
    device->CreatePixelShader(pixel_shader_blob->GetBufferPointer(), pixel_shader_blob->GetBufferSize(), nullptr, &pixel_shader);
    Logger::debug("Compiled shaders succesfully");
    return true;
}

void DirectX11Renderer::generate_buffers() {
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    device->CreateInputLayout(layout, ARRAYSIZE(layout), vertex_shader_blob->GetBufferPointer(), vertex_shader_blob->GetBufferSize(), &input_layout);

    D3D11_BUFFER_DESC description = {};
    description.ByteWidth = sizeof(float) * 8;
    description.Usage = D3D11_USAGE_DYNAMIC;
    description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&description, nullptr, &vbo);

    device_context->OMSetRenderTargets(1, &render_target_view, nullptr);

    RECT rectangle;
    GetClientRect(window, &rectangle);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(rectangle.right - rectangle.left);
    viewport.Height = static_cast<float>(rectangle.bottom - rectangle.top);
    device_context->RSSetViewports(1, &viewport);

    Logger::debug("Created vertex buffers successfully");
}

void DirectX11Renderer::toggle_fullscreen() {
    BOOL is_fullscreen;
    swap_chain->GetFullscreenState(&is_fullscreen, nullptr);
    swap_chain->SetFullscreenState(!is_fullscreen, nullptr);
}

void DirectX11Renderer::render_quad(float x, float y, float w, float h) {
    float half_width = w * 0.5f;
    float half_height = h * 0.5f;

    float vertices[8] = {
        x - half_width, y - half_height,
        x + half_width, y - half_height,
        x - half_width, y + half_height,
        x + half_width, y + half_height
    };

    D3D11_MAPPED_SUBRESOURCE map;
    device_context->Map(vbo, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, vertices, sizeof(vertices));
    device_context->Unmap(vbo, 0);

    UINT stride = sizeof(float) * 2;
    UINT offset = 0;

    device_context->IASetInputLayout(input_layout);
    device_context->IASetVertexBuffers(0, 1, &vbo, &stride, &offset);
    device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    device_context->VSSetShader(vertex_shader, nullptr, 0);
    device_context->PSSetShader(pixel_shader, nullptr, 0);
    device_context->Draw(4, 0);
}

void DirectX11Renderer::render_window() {
    const multi_pong::State& state = client->get_state();

    const float CLEAR_COLOUR[4] = { 0.155f, 0.155f, 0.155f, 1.0f };
    device_context->ClearRenderTargetView(render_target_view, CLEAR_COLOUR);

    render_quad(state.ball().x(), state.ball().y(), MULTI_PONG_BALL_WIDTH, MULTI_PONG_BALL_HEIGHT);

    render_quad(MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.player_1().paddle_location(), MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);

    render_quad(1.0f - MULTI_PONG_PADDLE_HORIZONTAL_PADDING, state.player_2().paddle_location(),MULTI_PONG_PADDLE_WIDTH, MULTI_PONG_PADDLE_HEIGHT);

    swap_chain->Present(1, 0);
}

void DirectX11Renderer::render_loop() {
    MSG message = {};

    while (true) {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) return;

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        render_window();
    }
}

DirectX11Renderer::~DirectX11Renderer() {}
