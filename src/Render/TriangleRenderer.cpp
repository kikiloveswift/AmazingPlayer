//
// Created by liqiang on 25-7-2.
//

#include "TriangleRenderer.h"
#include <iostream>
#include <glad/glad.h>
#include <cstring>

// 顶点着色器源码
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

// 片段着色器源码
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
)";

TriangleRenderer::TriangleRenderer()
    : window(nullptr), glContext(nullptr), shaderProgram(0), VAO(0), VBO(0) {
}

TriangleRenderer::~TriangleRenderer() {
    Cleanup();
}

bool TriangleRenderer::Initialize() {
    if (!InitializeSDL()) {
        return false;
    }

    if (!InitializeOpenGL()) {
        return false;
    }

    if (!CreateShaders()) {
        return false;
    }

    if (!CreateTriangle()) {
        return false;
    }

    std::cout << "Triangle renderer initialized successfully!" << std::endl;
    std::cout << "Press ESC to exit." << std::endl;
    return true;
}

void TriangleRenderer::Run() {
    bool running = true;

    while (running) {
        HandleEvents(running);
        Render();
        SDL_GL_SwapWindow(window);
    }
}

void TriangleRenderer::Cleanup() {
    // 清理 OpenGL 资源
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    // 清理 SDL 资源
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

bool TriangleRenderer::InitializeSDL() {
    std::cout << "SDL2 version: " << SDL_MAJOR_VERSION << "." << SDL_MINOR_VERSION << std::endl;

    // 初始化 SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // 设置 OpenGL 版本
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // 创建窗口
    window = SDL_CreateWindow(
        "OpenGL Triangle - AmazingPlayer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // 创建 OpenGL 上下文
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // 启用垂直同步
    SDL_GL_SetSwapInterval(1);

    return true;
}

bool TriangleRenderer::InitializeOpenGL() {
    // 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    std::cout << "OpenGL Version: " << GLVersion.major << "." << GLVersion.minor << std::endl;

    // 设置视口
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    return true;
}

bool TriangleRenderer::CreateShaders() {
    // 编译顶点着色器
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (vertexShader == 0) return false;

    // 编译片段着色器
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    // 创建着色器程序
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    CheckShaderCompileError(shaderProgram, "PROGRAM");

    // 删除着色器，它们已经链接到程序中了
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return true;
}

bool TriangleRenderer::CreateTriangle() {
    // 定义三角形顶点
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // 左下
         0.5f, -0.5f, 0.0f, // 右下
         0.0f,  0.5f, 0.0f  // 顶部
    };

    // 创建并绑定 VAO 和 VBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return true;
}

void TriangleRenderer::HandleEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        } else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }
    }
}

void TriangleRenderer::Render() {
    // 清屏
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 绘制三角形
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

unsigned int TriangleRenderer::CompileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
    CheckShaderCompileError(shader, typeName);

    return shader;
}

void TriangleRenderer::CheckShaderCompileError(unsigned int shader, const char* type) {
    int success;
    char infoLog[512];

    if (strcmp(type, "PROGRAM") != 0) {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n" << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 512, NULL, infoLog);
            std::cerr << "ERROR::SHADER::" << type << "::LINKING_FAILED\n" << infoLog << std::endl;
        }
    }
}

void TriangleRenderer::CheckOpenGLError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after " << operation << ": ";
        switch (error) {
            case GL_INVALID_ENUM:
                std::cerr << "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                std::cerr << "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                std::cerr << "GL_INVALID_OPERATION";
                break;
            case GL_OUT_OF_MEMORY:
                std::cerr << "GL_OUT_OF_MEMORY";
                break;
            default:
                std::cerr << "Unknown error " << error;
        }
        std::cerr << std::endl;
    }
}