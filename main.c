#include "text_engine.h"

int main(int argc, char *argv[])
{
    struct TextEngine *te = textEngineInit(800, 600, "./AovelSansRounded-rdDL.ttf", 16, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
    if (te == NULL) return 1;
    while (te->isRunning)
    {
        te->frameStart = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                te->isRunning = 0;
                break;
            case SDL_TEXTINPUT:
                textEngineAppendString(te, event.text.text);
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.mod & KMOD_SHIFT)
                {
                    switch (event.key.keysym.sym)
                    {
                        // case SDLK_KP_PLUS:
                        //     fontSize += 8;
                        //     TTF_CloseFont(font);
                        //     font = TTF_OpenFont(fontName, (int) (fontSize * wScale));
                        //     // TTF_SizeUTF8(font, " ", &cursor.fontWidth, &cursor.fontHeight);
                        //     // update prevSize to force rerender
                        //     SDL_FlushEvent(SDL_TEXTINPUT);
                        //     break;
                        // case SDLK_KP_MINUS:
                        //     if (fontSize > 8)
                        //     {
                        //     fontSize -= 8;
                        //     TTF_CloseFont(font);
                        //     font = TTF_OpenFont(fontName, (int) (fontSize * wScale));
                        //     // TTF_SizeUTF8(font, " ", &cursor.fontWidth, &cursor.fontHeight);
                        //     // update prevSize to force rerender
                        //     }
                        //     SDL_FlushEvent(SDL_TEXTINPUT);
                        //     break;
                        default:
                            break;
                    }
                }
                else
                {
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_BACKSPACE:
                            textEnginePopCharUTF8(te);
                            break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            textEngineRenderLine(te, te->currentLine);
                            textEngineAppendLine(te);
                            break;
                        case SDLK_TAB:
                            textEngineAppendChar(te, ' ');
                            textEngineAppendChar(te, ' ');
                            textEngineAppendChar(te, ' ');
                            textEngineAppendChar(te, ' ');
                        default:
                            break;
                    }
                }
                break;
            default:
                break;
            }
        }
        textEnginePollCursorBlinkTimer(te);
        // only force rerender if the text buffer gets updated
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