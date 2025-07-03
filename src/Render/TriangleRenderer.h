//
// Created by liqiang on 25-7-2.
//

#ifndef TRIANGLERENDERER_H
#define TRIANGLERENDERER_H

#include <SDL.h>

class TriangleRenderer {
public:
    TriangleRenderer();
    ~TriangleRenderer();

    // 初始化渲染器
    bool Initialize();

    // 运行主循环
    void Run();

    // 清理资源
    void Cleanup();

private:
    // SDL 相关
    SDL_Window* window;
    SDL_GLContext glContext;

    // OpenGL 相关
    unsigned int shaderProgram;
    unsigned int VAO, VBO;

    // 窗口参数
    static const int WINDOW_WIDTH = 800;
    static const int WINDOW_HEIGHT = 600;

    // 私有方法
    bool InitializeSDL();
    bool InitializeOpenGL();
    bool CreateShaders();
    bool CreateTriangle();
    void HandleEvents(bool& running);
    void Render();

    // 着色器相关
    unsigned int CompileShader(unsigned int type, const char* source);
    void CheckShaderCompileError(unsigned int shader, const char* type);

    // OpenGL 运行时检查
    void CheckOpenGLError(const char* operation);
};

#endif // TRIANGLERENDERER_H