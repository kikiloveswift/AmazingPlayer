// PlayerRender.cpp  — 2025-07-03 final
#include "PlayerRender.h"
#include <iostream>
#include <cstring>

/* ========== GLSL ========== */
static const char* vsrc = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
out vec2 UV;
void main(){ gl_Position=vec4(aPos,1); UV=aUV; })";

static const char* fsrc = R"(#version 330 core
in vec2 UV; out vec4 F;
uniform sampler2D tex0;
void main(){ F=texture(tex0,UV); })";

/* ========== 构析 ========== */
PlayerRender::PlayerRender()  {}
PlayerRender::~PlayerRender() { CleanUp(); }

/* -------- Initialize -------- */
bool PlayerRender::Initialize() {
    if (!initSDL()) return false;
    if (!initGL())  return false;
    return true;
}

/* -------- LoadMedia -------- */
bool PlayerRender::LoadMedia(const std::string& file)
{
    if (avformat_open_input(&fmt, file.c_str(), nullptr, nullptr) < 0) {
        std::cerr<<"open input fail\n"; return false;
    }
    if (avformat_find_stream_info(fmt,nullptr)<0){
        std::cerr<<"find stream info fail\n"; return false;
    }
    for(unsigned i=0;i<fmt->nb_streams;++i){
        auto t=fmt->streams[i]->codecpar->codec_type;
        if(t==AVMEDIA_TYPE_VIDEO && vIdx==-1) vIdx=i;
        if(t==AVMEDIA_TYPE_AUDIO && aIdx==-1) aIdx=i;
    }
    if (vIdx==-1){ std::cerr<<"no video stream\n"; return false; }

    if (!openVideo(fmt->streams[vIdx])) return false;
    if (aIdx!=-1 && !openAudio(fmt->streams[aIdx])) return false;

    /* 公共资源 */
    pkt=av_packet_alloc(); vf=av_frame_alloc(); af=av_frame_alloc();
    return true;
}

/* -------- Play -------- */
void PlayerRender::Play()
{
    if (playing && paused) {            // 恢复
        paused=false;
        return;
    }
    if (playing) return;                // 已经在播

    playing=true; paused=false; stopReq=false;
    decThread = std::thread(&PlayerRender::decodeLoop, this);

    /* 预缓冲：至少 12 帧音频 (~0.25 s) */
    while(aIdx!=-1 && !audioReady.load()) SDL_Delay(10);
}

/* -------- Pause -------- */
void PlayerRender::Pause() { paused=true; }

/* -------- Stop -------- */
void PlayerRender::Stop()
{
    if (!playing) return;
    stopReq=true; qCv.notify_all();
    if(decThread.joinable()) decThread.join();

    /* 清空队列 */
    { std::lock_guard<std::mutex> lk(qMtx);
      while(!vq.empty()){ av_frame_free(&vq.front()); vq.pop(); } }

    SDL_ClearQueuedAudio(audioDev);
    playing=false; paused=false;
}

/* -------- Seek -------- */
void PlayerRender::Seek(double s){ std::cout<<"[TODO] seek to "<<s<<" s\n"; }

/* -------- Run (主循环) -------- */
void PlayerRender::Run()
{
    bool running=true;
    uint64_t last=SDL_GetTicks64();
    uint64_t frameDur = (uint64_t)(1000.0/videoFPS);

    while(running){
        handleEvents(running);

        if(playing && !paused){
            uint64_t now=SDL_GetTicks64();
            if(now-last >= frameDur){
                renderOne(); last=now;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog); glBindVertexArray(vao);
        glBindTexture(GL_TEXTURE_2D,tex);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT,nullptr);
        SDL_GL_SwapWindow(win);
        SDL_Delay(1);
    }
    Stop();
}

/* ==================== 私有实现 ==================== */

/* ---- SDL & GL ---- */
bool PlayerRender::initSDL()
{
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)){ std::cerr<<SDL_GetError(); return false; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    win = SDL_CreateWindow("AmazingPlayer",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                           WIN_W,WIN_H,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
    if(!win){ std::cerr<<SDL_GetError(); return false; }
    gl = SDL_GL_CreateContext(win);
    if(!gl){ std::cerr<<SDL_GetError(); return false; }
    SDL_GL_SetSwapInterval(1);
    return true;
}
bool PlayerRender::initGL()
{
    if(!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)){ std::cerr<<"glad fail\n"; return false; }
    /* VBO/VAO */
    float v[]={
        -1,-1,0, 0,1,
         1,-1,0, 1,1,
         1, 1,0, 1,0,
        -1, 1,0, 0,0};
    unsigned idx[]={0,1,2,2,3,0};
    glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(v),v,GL_STATIC_DRAW);
    glGenBuffers(1,&ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(idx),idx,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,5*sizeof(float),(void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    return initShaders();
}
bool PlayerRender::initShaders()
{
    GLuint vs=compile(GL_VERTEX_SHADER,vsrc);
    GLuint fs=compile(GL_FRAGMENT_SHADER,fsrc);
    link(vs,fs); glDeleteShader(vs); glDeleteShader(fs);
    return true;
}
GLuint PlayerRender::compile(GLenum t,const char* s){
    GLuint id=glCreateShader(t); glShaderSource(id,1,&s,nullptr); glCompileShader(id);
    GLint ok; glGetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(id,512,nullptr,log); std::cerr<<log<<"\n"; }
    return id;
}
void PlayerRender::link(GLuint vs,GLuint fs){
    prog=glCreateProgram(); glAttachShader(prog,vs); glAttachShader(prog,fs);
    glLinkProgram(prog); GLint ok; glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if(!ok){ char log[512]; glGetProgramInfoLog(prog,512,nullptr,log); std::cerr<<log<<"\n"; }
}

/* ---- openVideo ---- */
bool PlayerRender::openVideo(AVStream* s)
{
    const AVCodec* c=avcodec_find_decoder(s->codecpar->codec_id);
    vc=avcodec_alloc_context3(c);
    avcodec_parameters_to_context(vc,s->codecpar);
    if(avcodec_open2(vc,c,nullptr)<0){ std::cerr<<"open video codec fail\n"; return false; }
    vw=vc->width; vh=vc->height;
    AVRational r=s->avg_frame_rate.num? s->avg_frame_rate:s->r_frame_rate;
    videoFPS = r.num&&r.den ? av_q2d(r):25.0;

    sws = sws_getContext(vw,vh,vc->pix_fmt, vw,vh,AV_PIX_FMT_RGB24,
                         SWS_BILINEAR,nullptr,nullptr,nullptr);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    int vbuf = av_image_get_buffer_size(AV_PIX_FMT_RGB24,vw,vh,1);
    vidBuf = (uint8_t*)av_malloc(vbuf);

    glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,vw,vh,0,GL_RGB,GL_UNSIGNED_BYTE,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tex0"), 0);
    return true;
}

/* ---- openAudio ---- */
bool PlayerRender::openAudio(AVStream* s)
{
    const AVCodec* c=avcodec_find_decoder(s->codecpar->codec_id);
    ac = avcodec_alloc_context3(c);
    avcodec_parameters_to_context(ac,s->codecpar);
    if(avcodec_open2(ac,c,nullptr)<0){ std::cerr<<"open audio codec fail\n"; return false; }

    SDL_AudioSpec want{},got{};
    want.freq   = ac->sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = ac->ch_layout.nb_channels;
    want.samples  = 2048;
    /* 不指定 callback → 使用 QueueAudio */
    audioDev = SDL_OpenAudioDevice(nullptr,0,&want,&got,SDL_AUDIO_ALLOW_ANY_CHANGE);
    if(!audioDev){ std::cerr<<"SDL open audio fail\n"; return false; }
    bytesPerSec = got.freq * got.channels * SDL_AUDIO_BITSIZE(got.format)/8;

    /* swr */
    AVChannelLayout outCh; av_channel_layout_default(&outCh, got.channels);
    swr = swr_alloc();
    if(!swr){ std::cerr<<"swr alloc fail\n"; return false; }
    if(swr_alloc_set_opts2(&swr,&outCh,AV_SAMPLE_FMT_S16,got.freq,
                           &ac->ch_layout,ac->sample_fmt,ac->sample_rate,0,nullptr)<0){
        std::cerr<<"swr set opts fail\n"; return false;
    }
    swr_init(swr);

    audBuf = (uint8_t*)av_malloc(got.samples * got.channels * sizeof(int16_t));
    SDL_PauseAudioDevice(audioDev,0);           // 开声（队列为空时静音）
    return true;
}

/* ---- 时钟 ---- */
double PlayerRender::getAudioClock() const
{
    if(aIdx==-1) return 0.0;
    Uint32 queued = SDL_GetQueuedAudioSize(audioDev);
    return audioWritePts.load() - queued / (double)bytesPerSec;
}

/* ---- 解码线程 ---- */
void PlayerRender::decodeLoop()
{
    int warm=0;
    while(!stopReq){
        if(av_read_frame(fmt,pkt)<0) break;

        if(pkt->stream_index==vIdx){
            avcodec_send_packet(vc,pkt);
            while(avcodec_receive_frame(vc,vf)==0){
                std::unique_lock<std::mutex> lk(qMtx);
                qCv.wait(lk,[&]{ return (int)vq.size()<MAX_VQ || stopReq; });
                AVFrame* cp=av_frame_alloc(); av_frame_ref(cp,vf);
                vq.push(cp); qCv.notify_one();
            }

        }else if(aIdx!=-1 && pkt->stream_index==aIdx){
            avcodec_send_packet(ac,pkt);
            while(avcodec_receive_frame(ac,af)==0){
                int outSamples = swr_convert(swr,&audBuf,af->nb_samples,
                                             (const uint8_t**)af->extended_data,af->nb_samples);
                int outBytes = outSamples * bytesPerSec / ac->sample_rate;

                while(SDL_GetQueuedAudioSize(audioDev) > bytesPerSec*AUDIO_CACHE_MS/1000){
                    SDL_Delay(5); if(stopReq) break;
                }
                SDL_QueueAudio(audioDev,audBuf,outBytes);

                double fpts = (af->pts!=AV_NOPTS_VALUE)?
                              af->pts*av_q2d(fmt->streams[aIdx]->time_base):
                              audioWritePts.load();
                audioWritePts.store(fpts + outSamples/(double)ac->sample_rate);
                if(!audioReady.load() && ++warm>=12) audioReady.store(true);
            }
        }
        av_packet_unref(pkt);
    }
}

/* ---- renderOne ---- */
void PlayerRender::renderOne()
{
    std::unique_lock<std::mutex> lk(qMtx);
    if(vq.empty()) return;
    AVFrame* f=vq.front();

    double master = audioReady.load()? getAudioClock(): 0.0;
    double pts = (f->pts!=AV_NOPTS_VALUE)?
                 f->pts*av_q2d(fmt->streams[vIdx]->time_base):master;

    const double EARLY=0.05, LATE=0.25;
    if(pts > master + EARLY) return;                       // 过早，等
    if(pts < master - LATE){                               // 过晚，丢
        vq.pop(); av_frame_free(&f); qCv.notify_one(); return;
    }

    vq.pop(); lk.unlock(); qCv.notify_one();

    uint8_t* dst[4]={vidBuf,nullptr,nullptr,nullptr};
    int ls[4]={vw*3,0,0,0};
    sws_scale(sws,f->data,f->linesize,0,vh,dst,ls);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,vw,vh,GL_RGB,GL_UNSIGNED_BYTE,vidBuf);

    av_frame_free(&f);
}

/* ---- 事件 ---- */
void PlayerRender::handleEvents(bool& run){
    SDL_Event e;
    while(SDL_PollEvent(&e)){
        if(e.type==SDL_QUIT) run=false;
        else if(e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_ESCAPE) run=false;
    }
}

/* ---- 资源释放 ---- */
void PlayerRender::CleanUp()
{
    Stop();
    if(pkt) av_packet_free(&pkt);
    if(vf)  av_frame_free(&vf);
    if(af)  av_frame_free(&af);
    if(vc)  avcodec_free_context(&vc);
    if(ac)  avcodec_free_context(&ac);
    if(sws) sws_freeContext(sws);
    if(swr) swr_free(&swr);
    if(fmt) avformat_close_input(&fmt);
    if(vidBuf) av_free(vidBuf);
    if(audBuf) av_free(audBuf);

    if(tex) glDeleteTextures(1,&tex);
    if(vbo) glDeleteBuffers(1,&vbo);
    if(ebo) glDeleteBuffers(1,&ebo);
    if(vao) glDeleteVertexArrays(1,&vao);
    if(prog)glDeleteProgram(prog);

    if(audioDev) SDL_CloseAudioDevice(audioDev);
    if(gl) SDL_GL_DeleteContext(gl);
    if(win) SDL_DestroyWindow(win);
    SDL_Quit();
}
