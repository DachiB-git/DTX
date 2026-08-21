#include "text_engine.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    // TODO: fix cursor background blink mask with colors other than black and white
    struct TextEngine *te = textEngineInit(argv[1], 800, 600, "BasicallyAMono-Regular.ttf", 16, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    if (te == NULL) return 1;
    textEngineReadFile(te);
    while (te->isRunning)
    {
        te->frameStart = SDL_GetTicks();
        textEngineHandleEvents(te);
        textEnginePollCursorBlinkTimer(te);
        // only force rerender if the text buffer gets updated or cursor state changes
        if (te->shouldRerenderLines || te->shouldRerenderCursor)
        {
            textEngineRenderLines(te);
            textEngineRenderCursor(te);
            textEngineRenderStatusBar(te);
            SDL_RenderPresent(te->renderer);
        }
        unsigned int frameTime = (SDL_GetTicks() - te->frameStart);
        if ((float) frameTime < 16.66f)
        {
            SDL_Delay(16.66f - (float) frameTime);
        }
    }
    textEngineCleanUp(te);
    return 0;
}