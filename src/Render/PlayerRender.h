//
// Created by liqiang on 25-7-3.
//

#ifndef PLAYERRENDER_H
#define PLAYERRENDER_H
#include <SDL2/SDL.h>

class PlayerRender {
public:
    PlayerRender();
    ~PlayerRender();
    void Initialize();
    void Run();
    void CleanUp();
};

#endif //PLAYERRENDER_H
