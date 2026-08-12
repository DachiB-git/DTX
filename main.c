#include "text_engine.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    // TODO: fix cursor background blink mask with colors other than black and white
    struct TextEngine *te = textEngineInit(800, 600, "AovelSansRounded-rdDL.ttf", 16, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    if (te == NULL) return 1;
    textEngineReadFile(te, argv[1]);
    while (te->isRunning)
    {
        te->frameStart = SDL_GetTicks();
        textEngineHandleEvents(te);
        textEnginePollCursorBlinkTimer(te);
        // only force rerender if the text buffer gets updated or cursor state changes
        if (te->shouldRerenderLines || te->shouldRerenderCursor)
        {
            textEngineRerenderLines(te);
            textEngineRenderCursor(te);
            SDL_RenderPresent(te->renderer);
        }
        float frameTime = (SDL_GetTicks() - te->frameStart) / 1000.0;
        SDL_Delay(SDL_floor(16.66f - frameTime));
    }
    textEngineWriteFile(te, argv[1]);
    textEngineCleanUp(te);
    return 0;
}