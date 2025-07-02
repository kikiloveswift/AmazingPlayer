#include <iostream>

// SDL2
#include <SDL.h>

// glad
#include <glad/glad.h>

// ffmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char* argv[]) {
    std::cout << "SDL2 version: " << SDL_MAJOR_VERSION << "." << SDL_MINOR_VERSION << std::endl;

    // 初始化 SDL2
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // 创建窗口
    SDL_Window* win = SDL_CreateWindow(
        "SDL2 + OpenGL + FFmpeg Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_OPENGL
    );
    if (!win) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // 创建 OpenGL 上下文
    SDL_GLContext gl_context = SDL_GL_CreateContext(win);
    if (!gl_context) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // 初始化 glad
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        SDL_GL_DeleteContext(gl_context);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    std::cout << "OpenGL Version: " << GLVersion.major << "." << GLVersion.minor << std::endl;

    // 用 FFmpeg 打印版本
    std::cout << "FFmpeg avcodec version: " << av_version_info() << std::endl;

    // 保持窗口 1 秒
    SDL_Delay(1000);

    // 清理
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(win);
    SDL_Quit();
    std::cout << "Test finished. All libraries imported and ran successfully!" << std::endl;
    return 0;
}
