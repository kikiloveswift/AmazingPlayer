// PlayerRender.h  — 2025-07-03 final (接口保持原样)
#ifndef PLAYERRENDER_H
#define PLAYERRENDER_H

#include <SDL2/SDL.h>
#include <glad/glad.h>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

class PlayerRender {
public:
    PlayerRender();
    ~PlayerRender();

    bool Initialize();                                  // SDL + GL 常规初始化
    bool LoadMedia(const std::string& file);            // 打开媒体
    void Play();                                        // 开始 / 恢复
    void Pause();                                       // 暂停
    void Stop();                                        // 停止并清空
    void Seek(double seconds);                          // （示例仅打印，留给你实现）
    void Run();                                         // 主循环
    void CleanUp();                                     // 释放全部资源

private:
    /* ---------- 常量 ---------- */
    static constexpr int WIN_W = 1280;
    static constexpr int WIN_H = 720;
    static constexpr int MAX_VQ = 48;                   // ≤2 s 视频帧缓存
    static constexpr int AUDIO_CACHE_MS = 1000;         // SDL 队列上限 1 s

    /* ---------- SDL/GL ---------- */
    SDL_Window*   win = nullptr;
    SDL_GLContext gl  = nullptr;
    SDL_AudioDeviceID audioDev = 0;
    int bytesPerSec = 0;                                // 声道×采样率×bytes/sample

    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0, tex = 0;

    /* ---------- FFmpeg ---------- */
    AVFormatContext* fmt = nullptr;
    AVCodecContext *vc = nullptr, *ac = nullptr;
    SwsContext* sws = nullptr;
    SwrContext* swr = nullptr;
    AVPacket*  pkt = nullptr;
    AVFrame *vf = nullptr, *af = nullptr;
    int vIdx = -1, aIdx = -1;
    int vw = 0, vh = 0;
    double videoFPS = 25.0;

    /* ---------- 队列 / 线程 ---------- */
    std::queue<AVFrame*> vq;
    std::mutex   qMtx;
    std::condition_variable qCv;
    std::thread  decThread;
    std::atomic<bool> playing{false}, paused{false}, stopReq{false};

    /* ---------- 音频时钟 ---------- */
    std::atomic<double> audioWritePts{0.0};             // 下一采样绝对 PTS
    std::atomic<bool>   audioReady{false};
    double audioBasePts = NAN;
    double videoBasePts = NAN;
    /* ---------- 缓冲 ---------- */
    uint8_t* vidBuf = nullptr;                          // RGB24
    uint8_t* audBuf = nullptr;                          // 重采样输出

    /* ---------- 私有工具 ---------- */
    bool initSDL();
    bool initGL();
    bool initShaders();
    bool openAudio(AVStream*);
    bool openVideo(AVStream*);
    GLuint compile(GLenum,const char*);
    void   link(GLuint,GLuint);
    void   decodeLoop();
    double getAudioClock() const;
    void   renderOne();
    void   handleEvents(bool& running);
};

#endif
