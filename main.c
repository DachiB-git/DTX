#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    char buffer[64];
    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        return 1;
    }
    
    window = SDL_CreateWindow("DTX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    if (window == NULL)
    {
        fprintf(stderr, "Failed to create a window: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL)
    {
        fprintf(stderr, "Failed to create a renderer: %s\n", SDL_GetError());
        return 1;
    }

    unsigned int frameStart;
    unsigned int frameEnd;
    unsigned int timerStart = SDL_GetTicks();
    int isRunning = 1;
    while (isRunning)
    {
        frameStart = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                isRunning = 0;
                break;
            
            default:
                break;
            }
        }
        frameEnd = SDL_GetTicks();
        float frameTime = (frameEnd - frameStart) / 1000.0;
        SDL_Delay(SDL_floor(16.66f - frameTime));
        if (frameEnd - timerStart >= 250)
        {
            
            timerStart = frameEnd;
            snprintf(buffer, sizeof(buffer), "DTX | FPS: %f\n", 1.0 / frameTime);
            SDL_SetWindowTitle(window, buffer);
        }
    }
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}