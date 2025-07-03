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

// 调试模式控制 - 在编译时定义 ENABLE_DEBUG 来启用调试功能
// g++ -DENABLE_DEBUG ...
#ifdef ENABLE_DEBUG
    #define DEBUG_ENABLED 1
#else
    #define DEBUG_ENABLED 1
#endif

class PlayerRender {
public:
    PlayerRender();
    ~PlayerRender();

    bool Initialize();
    bool LoadMedia(const std::string& file);
    void Play();
    void Pause();
    void Stop();
    void Seek(double seconds);
    void Run();
    void CleanUp();

private:
    static constexpr int WIN_W = 1280;
    static constexpr int WIN_H = 720;
    static constexpr int MAX_VQ = 48;
    static constexpr int AUDIO_CACHE_MS = 1000;

    SDL_Window*   win = nullptr;
    SDL_GLContext gl  = nullptr;
    SDL_AudioDeviceID audioDev = 0;
    int bytesPerSec = 0;

    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0, tex = 0;

    AVFormatContext* fmt = nullptr;
    AVCodecContext *vc = nullptr, *ac = nullptr;
    SwsContext* sws = nullptr;
    SwrContext* swr = nullptr;
    AVPacket*  pkt = nullptr;
    AVFrame *vf = nullptr, *af = nullptr;
    int vIdx = -1, aIdx = -1;
    int vw = 0, vh = 0;
    double videoFPS = 25.0;

    std::queue<AVFrame*> vq;
    std::mutex   qMtx;
    std::condition_variable qCv;
    std::thread  decThread;
    std::atomic<bool> playing{false}, paused{false}, stopReq{false};

    std::atomic<double> audioWritePts{0.0};
    std::atomic<bool>   audioReady{false};

    uint8_t* vidBuf = nullptr;
    uint8_t* audBuf = nullptr;

    // 调试统计信息
    #if DEBUG_ENABLED
    struct SyncStats {
        double maxVideoLead = 0.0;    // 视频最大领先时间
        double maxAudioLead = 0.0;    // 音频最大领先时间
        double totalDiff = 0.0;       // 总时间差
        int frameCount = 0;           // 已渲染帧数
        int dropCount = 0;            // 丢弃帧数
        int lateCount = 0;            // 延迟帧数
    } syncStats;

    bool debugOutput = false;         // 实时调试输出开关
    #endif

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

    #if DEBUG_ENABLED
    void logDebug(const std::string& message) const;
    void resetStats();
    void printSyncStats() const;
    #endif
};

#endif
