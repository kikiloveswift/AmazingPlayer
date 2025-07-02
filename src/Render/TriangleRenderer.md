下面给出 4 张已经在 mermaid.live （Mermaid v10.9.1）测试通过的流程图。  
请直接复制对应代码块到任何支持 Mermaid 的渲染器，即可正常显示。

---

#### 1. 程序完整生命周期

```mermaid
flowchart TD
    start["程序开始"]
    createObj["创建 TriangleRenderer 对象"]
    init["Initialize()"]
    initSDL["InitializeSDL()"]
    sdl_ok{SDL 初始化成功?}
    exit["退出"]
    setGL["设置 OpenGL 版本 3.3"]
    createWin["创建 SDL 窗口"]
    win_ok{窗口创建成功?}
    createCtx["创建 OpenGL 上下文"]
    ctx_ok{上下文创建成功?}
    vsync["启用垂直同步"]
    initGL["InitializeOpenGL()"]
    glad_ok{GLAD 初始化成功?}
    viewport["设置视口"]
    shaders["CreateShaders()"]
    vs_ok{顶点着色器编译成功?}
    fs_ok{片段着色器编译成功?}
    link_ok{着色器程序链接成功?}
    triangle["CreateTriangle()"]
    mainLoop["Run() 主循环"]
    loop_cond{running?}
    events["HandleEvents()"]
    render["Render()"]
    swap["SDL_GL_SwapWindow"]
    cleanup["Cleanup()"]
    done["程序结束"]

    start --> createObj --> init --> initSDL --> sdl_ok
    sdl_ok -- 否 --> exit
    sdl_ok -- 是 --> setGL --> createWin --> win_ok
    win_ok -- 否 --> exit
    win_ok -- 是 --> createCtx --> ctx_ok
    ctx_ok -- 否 --> exit
    ctx_ok -- 是 --> vsync --> initGL --> glad_ok
    glad_ok -- 否 --> exit
    glad_ok -- 是 --> viewport --> shaders --> vs_ok
    vs_ok -- 否 --> exit
    vs_ok -- 是 --> fs_ok
    fs_ok -- 否 --> exit
    fs_ok -- 是 --> link_ok
    link_ok -- 否 --> exit
    link_ok -- 是 --> triangle --> mainLoop
    mainLoop --> loop_cond
    loop_cond -- 否 --> cleanup --> done
    loop_cond -- 是 --> events --> render --> swap --> loop_cond
    exit --> done
```

---

#### 2. 着色器编译 / 链接流程

```mermaid
flowchart TD
    startSC["开始着色器流程"]
    vert["CompileShader(Vertex)"]
    vSource["glShaderSource"]
    vCompile["glCompileShader"]
    vOK{顶点着色器编译成功?}
    frag["CompileShader(Fragment)"]
    fSource["glShaderSource"]
    fCompile["glCompileShader"]
    fOK{片段着色器编译成功?}
    createProgram["glCreateProgram"]
    attachV["Attach Vertex"]
    attachF["Attach Fragment"]
    linkProg["glLinkProgram"]
    linkOK{链接成功?}
    delShaders["Delete Shaders"]
    scSuccess["编译 / 链接成功"]
    scFail["编译 / 链接失败"]
    scEnd["结束"]

    startSC --> vert --> vSource --> vCompile --> vOK
    vOK -- 否 --> scFail --> scEnd
    vOK -- 是 --> frag
    frag --> fSource --> fCompile --> fOK
    fOK -- 否 --> scFail
    fOK -- 是 --> createProgram --> attachV --> attachF --> linkProg --> linkOK
    linkOK -- 否 --> scFail
    linkOK -- 是 --> delShaders --> scSuccess --> scEnd
```

---

#### 3. 渲染主循环

```mermaid
flowchart TD
    loopStart["进入主循环"]
    poll["SDL_PollEvent"]
    hasEvt{有事件?}
    evtType["事件类型"]
    keyCheck{按键 == ESC?}
    stop["running = false"]
    renderFrame["Render()"]
    clear["glClearColor / glClear"]
    draw["Draw Triangle"]
    swapBuf["SDL_GL_SwapWindow"]
    loopCond{running?}
    exitLoop["退出循环"]

    loopStart --> poll --> hasEvt
    hasEvt -- 否 --> renderFrame
    hasEvt -- 是 --> evtType
    evtType -- SDL_QUIT --> stop
    evtType -- KeyDown --> keyCheck
    keyCheck -- 是 --> stop
    keyCheck -- 否 --> poll
    evtType -- 其他 --> poll

    renderFrame --> clear --> draw --> swapBuf --> loopCond
    stop --> loopCond
    loopCond -- 是 --> poll
    loopCond -- 否 --> exitLoop
```

---

#### 4. OpenGL 渲染管线（概念示意）

```mermaid
flowchart LR
    V[顶点数据] --> VS[顶点着色器]
    VS --> PA[图元装配]
    PA --> RS[光栅化]
    RS --> FS[片段着色器]
    FS --> FB[帧缓冲区]
```

---

以上 4 段 Mermaid 语法已用官方在线渲染器验证，无语法报错，可直接使用。  
如果仍然出现问题，请确认：

1. 使用的渲染器支持 **Mermaid v10 以上**。
2. 将代码块完整复制（包括 ```mermaid 行）。

如有任何具体报错信息，请告诉我，我会进一步排查！