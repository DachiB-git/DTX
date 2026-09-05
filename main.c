#include "text_engine.h"

extern unsigned int allocated_bytes_lines;

char allocationFormatBuffer[11];

char* formatAllocation(unsigned int bytes)
{
    char *suffixes[] = {"Bs", "KBs", "MBs", "GBs"};
    int offset = 0;
    float _bytes = (float) bytes;
    while (_bytes >= 1024)
    {
        offset++;
        _bytes /= 1024;
    }
    snprintf(allocationFormatBuffer, sizeof(allocationFormatBuffer), "%.2f%s", _bytes, suffixes[offset]);
    return allocationFormatBuffer;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    // TODO: fix cursor background blink mask with colors other than black and white
    struct TextEngine *te = textEngineInit(argv[1], 800, 600, "JuliaMono-Regular.ttf", 16, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    if (te == NULL) return 1;
    textEngineReadFile(te);
    struct ProfilingTimers
    {
        unsigned int generalTimer;
        unsigned int eventHandlerTimer;
        unsigned int renderTimer;
        unsigned int cursorTimer;
        unsigned int swapTimer;
    };
    struct ProfilingTimers profilingTimers;
    profilingTimers.generalTimer = SDL_GetTicks();
    while (te->isRunning)
    {
        te->frameStart = SDL_GetTicks();
        profilingTimers.eventHandlerTimer = SDL_GetTicks();
        textEngineHandleEvents(te);
        profilingTimers.eventHandlerTimer = SDL_GetTicks() - profilingTimers.eventHandlerTimer;
        textEnginePollCursorBlinkTimer(te);
        // only force rerender if the text buffer gets updated or cursor state changes
        if (te->shouldRerenderLines || te->shouldRerenderCursor)
        {
            profilingTimers.renderTimer = SDL_GetTicks();
            textEngineRenderLines(te);
            profilingTimers.renderTimer = SDL_GetTicks() - profilingTimers.renderTimer;
            textEngineRenderCursor(te);
            textEngineRenderStatusBar(te);
            profilingTimers.swapTimer = SDL_GetTicks();
            SDL_RenderPresent(te->renderer);
            profilingTimers.swapTimer = SDL_GetTicks() - profilingTimers.swapTimer;
        }
        unsigned int frameTime = (SDL_GetTicks() - te->frameStart);
        unsigned int profilingTimeEnd = SDL_GetTicks();
        // if (profilingTimeEnd - profilingTimers.generalTimer >= 1000)
        // {
        //     profilingTimers.generalTimer = profilingTimeEnd;
        //     printf("\n\n\n\n");
        //     printf("events_time(ms): %d\n", profilingTimers.eventHandlerTimer);
        //     printf("render_time(ms): %d\n", profilingTimers.renderTimer);
        //     printf("swap_time(ms): %d\n", profilingTimers.swapTimer);
        //     printf("frame_time(ms): %d\n", frameTime);
        // }
        if (frameTime < TARGET_MILLIS_PER_FRAME)
        {
            SDL_Delay(TARGET_MILLIS_PER_FRAME - frameTime);
        }
    }
    // TODO: add a prompt that asks if the user wants to save the modified file or not on exit
    // if (te->fileIsNotSaved)
    // {
    //     textEngineWriteFile(te);
    // }
    printf("allocated_bytes(lines): %s\n", formatAllocation(allocated_bytes_lines));
    textEngineCleanUp(te);
    return 0;
}