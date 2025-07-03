#include <iostream>
#include "Render/PlayerRender.h"
// // ffmpeg
// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// }

int main(int argc, char* argv[]) {
    std::cout << "AmazingPlayer - Starting up..." << std::endl;
    
    // 创建播放器实例
    PlayerRender player;

    // 初始化播放器
    if (!player.Initialize()) {
        std::cerr << "Failed to initialize player" << std::endl;
        return 1;
    }

    // 加载本地视频文件
    std::string videoFile = "../src/wwdc-243.mp4";
    std::cout << "Loading video file: " << videoFile << std::endl;

    if (!player.LoadMedia(videoFile)) {
        std::cerr << "Failed to load video file: " << videoFile << std::endl;
        std::cerr << "Please make sure wwdc-243.mp4 exists in the current directory" << std::endl;
        return 1;
    }
    
    // 显示控制说明
    std::cout << "\n=== Controls ===" << std::endl;
    std::cout << "Space     - Play/Pause" << std::endl;
    std::cout << "Left      - Seek backward 10 seconds" << std::endl;
    std::cout << "Right     - Seek forward 10 seconds" << std::endl;
    std::cout << "ESC       - Exit" << std::endl;
    std::cout << "================" << std::endl;
    
    // 开始播放
    std::cout << "Starting playback..." << std::endl;
    player.Play();

    // 运行主循环
    player.Run();
    
    std::cout << "Player closed successfully!" << std::endl;
    return 0;
}
