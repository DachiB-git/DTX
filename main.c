#include "text_engine.h"

int main(int argc, char *argv[])
{
    struct TextEngine *te = textEngineInit(800, 600, "./AovelSansRounded-rdDL.ttf", 16, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    if (te == NULL) return 1;
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
    FILE* fh = fopen("dtx.c", "w");
    if (fh == NULL)
    {
        fprintf(stderr, "Unable to open file.");
    }
    struct Line *line = te->first;
    while (line)
    {
        stringBuilderToString(line->sb, te->buffer, IN_OUT_BUFFER_SIZE);
        if (fputs(te->buffer, fh) == EOF)
        {
            fprintf(stderr, "Error while writing to the file.");
            fclose(fh);
        }
        fputc('\n', fh);
        line = line->next;
    }
    fclose(fh);
    textEngineCleanUp(te);
    return 0;
}