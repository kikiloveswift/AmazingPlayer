#include "PlayerRender.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

#if DEBUG_ENABLED
#include <fstream> // 用于调试日志记录
#endif

/* ========== GLSL ========== */
static const char* vsrc = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
out vec2 UV;
void main(){
    gl_Position = vec4(aPos, 1.0);
    UV = aUV;
})";

static const char* fsrc = R"(#version 330 core
in vec2 UV;
out vec4 FragColor;
uniform sampler2D tex0;
void main(){
    FragColor = texture(tex0, UV);
})";

/* ========== 构析 ========== */
PlayerRender::PlayerRender()  {}
PlayerRender::~PlayerRender() { CleanUp(); }

/* -------- Initialize -------- */
bool PlayerRender::Initialize() {
    if (!initSDL()) return false;
    if (!initGL())  return false;

    #if DEBUG_ENABLED
    resetStats();
    #endif

    return true;
}

/* -------- LoadMedia -------- */
bool PlayerRender::LoadMedia(const std::string& file)
{
    // 重置索引
    vIdx = -1;
    aIdx = -1;

    fmt = avformat_alloc_context();
    if (!fmt) {
        std::cerr << "Failed to allocate format context\n";
        return false;
    }

    // 打开媒体文件
    if (avformat_open_input(&fmt, file.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input file: " << file << "\n";
        return false;
    }

    // 获取流信息
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        return false;
    }

    // 查找视频和音频流
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        auto codec_type = fmt->streams[i]->codecpar->codec_type;
        if (codec_type == AVMEDIA_TYPE_VIDEO && vIdx == -1) {
            vIdx = i;
        }
        if (codec_type == AVMEDIA_TYPE_AUDIO && aIdx == -1) {
            aIdx = i;
        }
    }

    if (vIdx == -1) {
        std::cerr << "No video stream found\n";
        return false;
    }

    // 打开视频流
    if (!openVideo(fmt->streams[vIdx])) {
        std::cerr << "Failed to open video stream\n";
        return false;
    }

    // 打开音频流（如果有）
    if (aIdx != -1) {
        if (!openAudio(fmt->streams[aIdx])) {
            std::cerr << "Failed to open audio stream\n";
            // 即使音频失败也继续
        }
    } else {
        std::cout << "No audio stream found, continuing without audio\n";
    }

    // 分配资源
    pkt = av_packet_alloc();
    vf = av_frame_alloc();
    af = av_frame_alloc();

    if (!pkt || !vf || !af) {
        std::cerr << "Failed to allocate FFmpeg resources\n";
        return false;
    }

    std::cout << "Media loaded successfully\n";
    std::cout << "Video: " << vw << "x" << vh << " @ " << videoFPS << " fps\n";
    if (aIdx != -1) {
        std::cout << "Audio: " << ac->sample_rate << " Hz, "
                  << ac->ch_layout.nb_channels << " channels\n";
    }

    return true;
}

/* -------- Play -------- */
void PlayerRender::Play()
{
    if (playing && paused) {
        paused = false;
        if (audioDev) SDL_PauseAudioDevice(audioDev, 0); // 恢复音频
        return;
    }
    if (playing) return;

    playing = true;
    paused = false;
    stopReq = false;
    audioReady = false;

    // 启动解码线程
    decThread = std::thread(&PlayerRender::decodeLoop, this);

    // 等待音频缓冲
    if (aIdx != -1) {
        std::cout << "Buffering audio...\n";
        while (!stopReq && !audioReady.load()) {
            SDL_Delay(10);
        }
        std::cout << "Audio ready\n";
    }
}

/* -------- Pause -------- */
void PlayerRender::Pause() {
    if (playing && !paused) {
        paused = true;
        if (audioDev) SDL_PauseAudioDevice(audioDev, 1); // 暂停音频
    }
}

/* -------- Stop -------- */
void PlayerRender::Stop()
{
    if (!playing) return;

    stopReq = true;
    qCv.notify_all();

    if (decThread.joinable()) {
        decThread.join();
    }

    // 清空视频队列
    {
        std::lock_guard<std::mutex> lock(qMtx);
        while (!vq.empty()) {
            av_frame_free(&vq.front());
            vq.pop();
        }
    }

    // 清空音频队列
    if (audioDev) {
        SDL_ClearQueuedAudio(audioDev);
    }

    playing = false;
    paused = false;

    #if DEBUG_ENABLED
    printSyncStats(); // 停止时自动打印统计信息
    #endif
}

/* -------- Seek -------- */
void PlayerRender::Seek(double s) {
    std::cout << "[Seek] to " << s << " seconds\n";
}

/* -------- Run (主循环) -------- */
void PlayerRender::Run()
{
    if (!win) {
        std::cerr << "Window not initialized\n";
        return;
    }

    bool running = true;
    auto lastFrameTime = std::chrono::steady_clock::now();
    const auto frameDuration = std::chrono::milliseconds(static_cast<int>(1000.0 / videoFPS));

    // 初始化视频队列统计
    int framesRendered = 0;
    auto startTime = std::chrono::steady_clock::now();

    #if DEBUG_ENABLED
    bool showStats = false;
    #endif

    while (running) {
        handleEvents(running);

        // 计算时间差
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - lastFrameTime;

        if (playing && !paused) {
            // 根据帧率控制渲染
            if (elapsed >= frameDuration) {
                renderOne();
                lastFrameTime = now;
                framesRendered++;
            }
        } else {
            // 暂停时更新时钟
            lastFrameTime = now;
        }

        // 渲染
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        SDL_GL_SwapWindow(win);

        #if DEBUG_ENABLED
        // 显示帧率统计
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedSec >= 2) {
            double fps = framesRendered / static_cast<double>(elapsedSec);
            std::string title = "Media Player | FPS: " + std::to_string(static_cast<int>(fps));
            SDL_SetWindowTitle(win, title.c_str());
            startTime = currentTime;
            framesRendered = 0;
        }

        // 显示统计信息
        if (showStats) {
            printSyncStats();
            showStats = false;
        }
        #endif

        // 避免CPU占用过高
        if (vq.size() < 5) {
            // 队列不足时稍微等待
            SDL_Delay(static_cast<Uint32>(frameDuration.count() / 2));
        } else {
            SDL_Delay(1);
        }
    }

    Stop();
}

/* ==================== 私有实现 ==================== */

/* ---- SDL & GL ---- */
bool PlayerRender::initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return false;
    }

    // 设置OpenGL属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // 创建窗口
    win = SDL_CreateWindow("Media Player",
                          SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED,
                          WIN_W, WIN_H,
                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!win) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
        SDL_Quit();
        return false;
    }

    // 创建OpenGL上下文
    gl = SDL_GL_CreateContext(win);
    if (!gl) {
        std::cerr << "SDL_GL_CreateContext Error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(win);
        SDL_Quit();
        return false;
    }

    // 设置垂直同步
    SDL_GL_SetSwapInterval(1);

    return true;
}

bool PlayerRender::initGL()
{
    // 加载GLAD
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    // 设置视口
    glViewport(0, 0, WIN_W, WIN_H);

    // 顶点数据 (位置 + UV)
    float vertices[] = {
        // 位置          // 纹理坐标
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,  // 左下
         1.0f, -1.0f, 0.0f,  1.0f, 1.0f,  // 右下
         1.0f,  1.0f, 0.0f,  1.0f, 0.0f,  // 右上
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f   // 左上
    };

    unsigned int indices[] = {
        0, 1, 2,  // 第一个三角形
        2, 3, 0   // 第二个三角形
    };

    // 创建VAO, VBO, EBO
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    // 绑定并设置VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 绑定并设置EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // 解绑
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 编译着色器
    return initShaders();
}

bool PlayerRender::initShaders()
{
    GLuint vertexShader = compile(GL_VERTEX_SHADER, vsrc);
    if (!vertexShader) return false;

    GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fsrc);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return false;
    }

    // 链接着色器程序
    prog = glCreateProgram();
    glAttachShader(prog, vertexShader);
    glAttachShader(prog, fragmentShader);
    glLinkProgram(prog);

    // 检查链接错误
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << "\n";
        glDeleteProgram(prog);
        prog = 0;
    }

    // 删除着色器
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return prog != 0;
}

GLuint PlayerRender::compile(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // 检查编译错误
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
                  << " shader compilation failed:\n" << infoLog << "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void PlayerRender::link(GLuint vs, GLuint fs)
{
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Shader link error: " << log << "\n";
    }
}

/* ---- openVideo ---- */
bool PlayerRender::openVideo(AVStream* stream)
{
    // 查找解码器
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Unsupported video codec\n";
        return false;
    }

    // 分配编解码器上下文
    vc = avcodec_alloc_context3(decoder);
    if (!vc) {
        std::cerr << "Failed to allocate video codec context\n";
        return false;
    }

    // 复制流参数到编解码器上下文
    if (avcodec_parameters_to_context(vc, stream->codecpar) < 0) {
        std::cerr << "Failed to copy video codec parameters\n";
        return false;
    }

    // 打开解码器
    if (avcodec_open2(vc, decoder, nullptr) < 0) {
        std::cerr << "Failed to open video codec\n";
        return false;
    }

    // 获取视频尺寸
    vw = vc->width;
    vh = vc->height;

    // 计算帧率
    AVRational frameRate = stream->avg_frame_rate;
    if (frameRate.num && frameRate.den) {
        videoFPS = av_q2d(frameRate);
    } else {
        // 尝试从时间基计算帧率
        if (stream->time_base.num && stream->time_base.den) {
            videoFPS = 1.0 / av_q2d(stream->time_base);
        } else {
            videoFPS = 30.0; // 默认值
            std::cerr << "Warning: Using default frame rate 30fps\n";
        }
    }

    // 创建SWSContext用于像素格式转换
    sws = sws_getContext(vw, vh, vc->pix_fmt,
                        vw, vh, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        std::cerr << "Failed to create SwsContext\n";
        return false;
    }

    // 分配视频缓冲区
    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, vw, vh, 1);
    vidBuf = (uint8_t*)av_malloc(bufferSize);
    if (!vidBuf) {
        std::cerr << "Failed to allocate video buffer\n";
        return false;
    }

    // 创建OpenGL纹理
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 分配纹理内存
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, vw, vh, 0,
                GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // 设置纹理单元
    glUseProgram(prog);
    GLint texLoc = glGetUniformLocation(prog, "tex0");
    if (texLoc != -1) {
        glUniform1i(texLoc, 0);
    } else {
        std::cerr << "Warning: Failed to find texture uniform\n";
    }

    std::cout << "Video initialized: " << vw << "x" << vh
              << " (" << av_get_pix_fmt_name(vc->pix_fmt) << ") @ "
              << videoFPS << " fps\n";

    return true;
}

/* ---- openAudio ---- */
bool PlayerRender::openAudio(AVStream* stream)
{
    // 查找解码器
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Unsupported audio codec\n";
        return false;
    }

    // 分配编解码器上下文
    ac = avcodec_alloc_context3(decoder);
    if (!ac) {
        std::cerr << "Failed to allocate audio codec context\n";
        return false;
    }

    // 复制流参数到编解码器上下文
    if (avcodec_parameters_to_context(ac, stream->codecpar) < 0) {
        std::cerr << "Failed to copy audio codec parameters\n";
        return false;
    }

    // 打开解码器
    if (avcodec_open2(ac, decoder, nullptr) < 0) {
        std::cerr << "Failed to open audio codec\n";
        return false;
    }

    // 设置SDL音频参数
    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = ac->sample_rate;
    desired.format = AUDIO_S16SYS;
    desired.channels = ac->ch_layout.nb_channels;
    desired.samples = 2048;

    // 打开音频设备
    audioDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (!audioDev) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        return false;
    }

    // 计算每秒字节数
    bytesPerSec = obtained.freq * obtained.channels * (SDL_AUDIO_BITSIZE(obtained.format) / 8);

    // 创建重采样上下文
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, obtained.channels);

    if (swr_alloc_set_opts2(&swr,
                           &outLayout, AV_SAMPLE_FMT_S16, obtained.freq,
                           &ac->ch_layout, ac->sample_fmt, ac->sample_rate,
                           0, nullptr) < 0) {
        std::cerr << "Failed to create SwrContext\n";
        return false;
    }

    if (swr_init(swr) < 0) {
        std::cerr << "Failed to initialize SwrContext\n";
        return false;
    }

    // 分配音频缓冲区
    int bufferSize = obtained.samples * obtained.channels * sizeof(int16_t);
    audBuf = (uint8_t*)av_malloc(bufferSize);
    if (!audBuf) {
        std::cerr << "Failed to allocate audio buffer\n";
        return false;
    }

    // 启动音频设备
    SDL_PauseAudioDevice(audioDev, 0);

    std::cout << "Audio initialized: " << obtained.freq << " Hz, "
              << obtained.channels << " channels\n";

    return true;
}

/* ---- 时钟 ---- */
double PlayerRender::getAudioClock() const
{
    if (aIdx == -1 || !audioDev) return 0.0;

    Uint32 queuedBytes = SDL_GetQueuedAudioSize(audioDev);
    double queuedSeconds = queuedBytes / static_cast<double>(bytesPerSec);

    return audioWritePts.load() - queuedSeconds;
}

/* ---- 解码线程 ---- */
void PlayerRender::decodeLoop()
{
    int audioFrames = 0;
    int videoFrames = 0;

    while (!stopReq) {
        // 读取数据包
        int ret = av_read_frame(fmt, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                std::cout << "End of file reached\n";

                // 刷新视频解码器
                avcodec_send_packet(vc, nullptr);
                while (true) {
                    AVFrame* frame = av_frame_alloc();
                    ret = avcodec_receive_frame(vc, frame);
                    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                        av_frame_free(&frame);
                        break;
                    } else if (ret < 0) {
                        av_frame_free(&frame);
                        break;
                    }

                    // 将帧加入队列
                    {
                        std::unique_lock<std::mutex> lock(qMtx);
                        vq.push(frame);
                    }
                    videoFrames++;
                }

                // 刷新音频解码器
                if (aIdx != -1) {
                    avcodec_send_packet(ac, nullptr);
                    while (true) {
                        ret = avcodec_receive_frame(ac, af);
                        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                            break;
                        } else if (ret < 0) {
                            break;
                        }

                        // 重采样并发送到音频设备
                        int outSamples = swr_convert(swr, &audBuf, af->nb_samples,
                                                    (const uint8_t**)af->extended_data, af->nb_samples);
                        if (outSamples > 0) {
                            int outBytes = outSamples * ac->ch_layout.nb_channels * sizeof(int16_t);
                            SDL_QueueAudio(audioDev, audBuf, outBytes);
                        }

                        av_frame_unref(af);
                    }
                }

                // 等待队列处理完毕
                while (!stopReq && !vq.empty()) {
                    SDL_Delay(10);
                }

                break; // 退出解码循环
            } else {
                char errbuf[256];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "av_read_frame error: " << errbuf << "\n";
                break;
            }
        }

        // 处理视频数据包
        if (pkt->stream_index == vIdx) {
            // 发送数据包到解码器
            ret = avcodec_send_packet(vc, pkt);
            if (ret < 0) {
                std::cerr << "Failed to send video packet to decoder\n";
                av_packet_unref(pkt);
                continue;
            }

            // 接收解码后的帧
            while (true) {
                ret = avcodec_receive_frame(vc, vf);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Video decoding error\n";
                    break;
                }

                // 将帧加入队列
                {
                    std::unique_lock<std::mutex> lock(qMtx);

                    // 等待队列有空间
                    while (vq.size() >= MAX_VQ && !stopReq) {
                        lock.unlock();
                        SDL_Delay(5); // 避免忙等待
                        lock.lock();
                    }

                    if (stopReq) break;

                    // 复制帧
                    AVFrame* frame = av_frame_clone(vf);
                    if (frame) {
                        vq.push(frame);
                        videoFrames++;
                    }
                }

                av_frame_unref(vf);
            }
        }
        // 处理音频数据包
        else if (aIdx != -1 && pkt->stream_index == aIdx) {
            // 发送数据包到解码器
            ret = avcodec_send_packet(ac, pkt);
            if (ret < 0) {
                std::cerr << "Failed to send audio packet to decoder\n";
                av_packet_unref(pkt);
                continue;
            }

            // 接收解码后的帧
            while (true) {
                ret = avcodec_receive_frame(ac, af);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    std::cerr << "Audio decoding error\n";
                    break;
                }

                // 重采样
                int outSamples = swr_convert(swr, &audBuf, af->nb_samples,
                                            (const uint8_t**)af->extended_data, af->nb_samples);
                if (outSamples < 0) {
                    std::cerr << "Audio resampling error\n";
                    av_frame_unref(af);
                    continue;
                }

                // 计算输出大小
                int outBytes = outSamples * ac->ch_layout.nb_channels * sizeof(int16_t);

                // 控制队列大小
                Uint32 queuedSize = SDL_GetQueuedAudioSize(audioDev);
                while (!stopReq && queuedSize > bytesPerSec * AUDIO_CACHE_MS / 1000) {
                    SDL_Delay(1);
                    queuedSize = SDL_GetQueuedAudioSize(audioDev);
                }

                if (stopReq) break;

                // 将音频数据加入队列
                if (SDL_QueueAudio(audioDev, audBuf, outBytes) < 0) {
                    std::cerr << "SDL_QueueAudio error: " << SDL_GetError() << "\n";
                }

                // 更新音频时钟
                double pts = (af->pts != AV_NOPTS_VALUE) ?
                            af->pts * av_q2d(fmt->streams[aIdx]->time_base) :
                            audioWritePts.load();

                audioWritePts.store(pts + (outSamples / static_cast<double>(ac->sample_rate)));

                // 标记音频准备好
                if (++audioFrames > 10) {
                    audioReady.store(true);
                }

                av_frame_unref(af);
            }
        }

        av_packet_unref(pkt);
    }

    std::cout << "Decoding thread exited\n";
}

/* ---- renderOne ---- */
void PlayerRender::renderOne()
{
    AVFrame* frame = nullptr;
    bool hasFrame = false;

    {
        std::unique_lock<std::mutex> lock(qMtx);

        if (!vq.empty()) {
            frame = vq.front();
            vq.pop();
            hasFrame = true;
        }
    }

    if (!hasFrame) {
        return;
    }

    // 确保有有效的帧
    if (!frame || !frame->data[0]) {
        if (frame) av_frame_free(&frame);
        return;
    }

    #if DEBUG_ENABLED
    // 调试模式下收集音画同步数据
    double master = 0.0;
    double pts = 0.0;
    double diff = 0.0;

    if (audioReady.load() && aIdx != -1) {
        master = getAudioClock();

        if (frame->pts != AV_NOPTS_VALUE) {
            pts = frame->pts * av_q2d(fmt->streams[vIdx]->time_base);
        } else if (frame->pkt_dts != AV_NOPTS_VALUE) {
            pts = frame->pkt_dts * av_q2d(fmt->streams[vIdx]->time_base);
        } else {
            pts = master;
        }

        diff = pts - master;

        // 收集统计信息
        syncStats.frameCount++;
        syncStats.totalDiff += std::abs(diff);

        if (diff > 0) {
            if (diff > syncStats.maxVideoLead) syncStats.maxVideoLead = diff;
        } else {
            if (-diff > syncStats.maxAudioLead) syncStats.maxAudioLead = -diff;
        }

        if (diff < -0.01) {
            syncStats.lateCount++;
        }

        // 调试输出
        if (debugOutput) {
            std::ostringstream oss;
            oss << "Frame PTS: " << std::fixed << std::setprecision(3) << pts
                << " | Audio: " << master
                << " | Diff: " << diff * 1000 << " ms";
            logDebug(oss.str());
        }
    }
    #endif

    // 转换像素格式
    uint8_t* dst[4] = {vidBuf, nullptr, nullptr, nullptr};
    int dstStride[4] = {vw * 3, 0, 0, 0};

    sws_scale(sws, frame->data, frame->linesize,
             0, vh, dst, dstStride);

    // 上传纹理
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vw, vh,
                   GL_RGB, GL_UNSIGNED_BYTE, vidBuf);

    // 释放帧
    av_frame_free(&frame);
}

/* ---- 事件处理 ---- */
void PlayerRender::handleEvents(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    if (playing) {
                        paused ? Play() : Pause();
                    }
                }
                #if DEBUG_ENABLED
                else if (event.key.keysym.sym == SDLK_d) {
                    debugOutput = !debugOutput;
                    std::cout << "Debug output "
                              << (debugOutput ? "ENABLED" : "DISABLED") << "\n";
                }
                else if (event.key.keysym.sym == SDLK_s) {
                    printSyncStats();
                }
                else if (event.key.keysym.sym == SDLK_r) {
                    resetStats();
                    std::cout << "Sync statistics reset\n";
                }
                #endif
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    glViewport(0, 0, event.window.data1, event.window.data2);
                }
                break;
        }
    }
}

/* ---- 资源释放 ---- */
void PlayerRender::CleanUp()
{
    Stop();

    // 释放FFmpeg资源
    if (pkt) av_packet_free(&pkt);
    if (vf) av_frame_free(&vf);
    if (af) av_frame_free(&af);
    if (vc) avcodec_free_context(&vc);
    if (ac) avcodec_free_context(&ac);
    if (fmt) avformat_close_input(&fmt);
    if (sws) sws_freeContext(sws);
    if (swr) swr_free(&swr);
    if (vidBuf) av_free(vidBuf);
    if (audBuf) av_free(audBuf);

    // 重置指针
    pkt = nullptr;
    vf = nullptr;
    af = nullptr;
    vc = nullptr;
    ac = nullptr;
    fmt = nullptr;
    sws = nullptr;
    swr = nullptr;
    vidBuf = nullptr;
    audBuf = nullptr;

    // 释放OpenGL资源
    if (tex) glDeleteTextures(1, &tex);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);

    // 重置OpenGL句柄
    tex = 0;
    vbo = 0;
    ebo = 0;
    vao = 0;
    prog = 0;

    // 释放SDL资源
    if (audioDev) SDL_CloseAudioDevice(audioDev);
    if (gl) SDL_GL_DeleteContext(gl);
    if (win) SDL_DestroyWindow(win);

    // 重置SDL句柄
    audioDev = 0;
    gl = nullptr;
    win = nullptr;

    SDL_Quit();
}

#if DEBUG_ENABLED
/* ==================== 调试功能实现 ==================== */

void PlayerRender::logDebug(const std::string& message) const {
    std::cout << "[DEBUG] " << message << "\n";

    // 可选：记录到文件
    static std::ofstream logFile("av_sync.log", std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        logFile << now_ms << " | " << message << "\n";
    }
}

void PlayerRender::resetStats() {
    syncStats = {};
}

void PlayerRender::printSyncStats() const {
    if (syncStats.frameCount == 0) {
        std::cout << "No frames rendered yet.\n";
        return;
    }

    double avgDiff = syncStats.totalDiff / syncStats.frameCount;

    std::cout << "\n===== 音画同步统计 =====\n";
    std::cout << "已渲染帧数: " << syncStats.frameCount << "\n";
    std::cout << "丢弃帧数: " << syncStats.dropCount << "\n";
    std::cout << "延迟帧数: " << syncStats.lateCount << "\n";
    std::cout << "平均时间差: " << avgDiff * 1000 << " ms\n";
    std::cout << "视频最大领先: " << syncStats.maxVideoLead * 1000 << " ms\n";
    std::cout << "音频最大领先: " << syncStats.maxAudioLead * 1000 << " ms\n";
    std::cout << "========================\n";
}
#endif
