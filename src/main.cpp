#include <iostream>
#include "Render/TriangleRenderer.h"
using namespace std;
// ffmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

int main(int argc, char* argv[]) {
    TriangleRenderer renderer;
    if (!renderer.Initialize()) {
        cerr << "Failed to initalize triangle renderer" << endl;
        return 1;
    }
    renderer.Run();
    cout << "Program finished successfully" << endl;
    return 0;
}
