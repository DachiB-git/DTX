#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

#define IN_OUT_BUFFER_SIZE 512

struct Cursor
{
    float offsetX;
    float offsetY;
    float width;
    unsigned int blinkStart;
    unsigned int blinkState;
    unsigned int blinkIntervalMillis;
};

struct Node
{
    char c;
    struct Node* prev;
    struct Node* next;
};

struct StringBuilder
{
    unsigned int size;
    struct Node* head;
    struct Node* tail;
};

struct Line
{
    float offsetX;
    float offsetY;
    float width;
    float height;
    unsigned int shouldRerender;
    struct StringBuilder *sb;
    struct Line *next;
    struct Line *prev;
};

void stringBuilderCleanUp(struct StringBuilder *sb)
{
    if (sb == NULL) return;
    struct Node *n;
    while (sb->head)
    {
        n = sb->head;
        sb->head = sb->head->next;
        free(n);
    }
    free(sb);
    return;
}

struct StringBuilder* stringBuilderInit()
{
    struct StringBuilder* sb = malloc(sizeof(*sb));
    if (sb == NULL) return sb;
    memset(sb, 0, sizeof(*sb));
    return sb;
}

struct Node* getNode(char c, struct Node* next, struct Node* prev)
{
    struct Node* newNode = malloc(sizeof(*newNode));
    if (newNode == NULL) return newNode;
    newNode->c = c;
    newNode->next = next;
    newNode->prev = prev;
    return newNode;
}

void stringBuilderPop(struct StringBuilder* sb)
{
    if (sb == NULL) return;
    if (sb->size == 0) return;
    sb->size--;
    if (sb->head == sb->tail)
    {
        free(sb->head);
        sb->head = NULL;
        sb->tail = NULL;
        return;
    }
    struct Node* oldTail = sb->tail;
    sb->tail = sb->tail->prev;
    sb->tail->next = NULL;
    free(oldTail);
    return;
}

#define isUTF8ContByte(c) ((c & 0xC0) == 0x80) 

void stringBuilderPopUTF8(struct StringBuilder* sb)
{
    if (sb == NULL) return;
    if (sb->size == 0) return;
    while (sb->size > 0 && isUTF8ContByte(sb->tail->c)) stringBuilderPop(sb);
    stringBuilderPop(sb);
    return;
}

// returns 0 on successful append, 1 on error
int stringBuilderAppend(struct StringBuilder* sb, char c)
{
    struct Node* n;
    sb->size++;
    if (sb->tail == NULL)
    {
        n = getNode(c, NULL, NULL);
        if (n == NULL) return 1;
        sb->head = n;
        sb->tail = n;
    }
    else
    {
        n = getNode(c, NULL, sb->tail);
        if (n == NULL) return 1;
        sb->tail->next = n;
        sb->tail = n;
    }
    return 0;
}

int stringBuilderAppendString(struct StringBuilder* sb, char *s)
{
    while(*s)
    {
        if (stringBuilderAppend(sb, *s) != 0) return 1;
        s++;
    }
    return 0;
}

void stringBuilderToString(struct StringBuilder* sb, char *buffer, unsigned int size)
{
    if (sb == NULL) return;
    struct Node* n = sb->head;
    unsigned int count = 0;
    while (n)
    {
        if (count >= size - 1) break;
        buffer[count++] = n->c;
        n = n->next;
    }
    buffer[count] = '\0';
    return;
}

struct Line* getLine()
{
    struct Line *line = malloc(sizeof(*line));
    memset(line, 0, sizeof(*line));
    line->sb = stringBuilderInit();
    return line;
}

struct TextEngine
{
    char buffer[IN_OUT_BUFFER_SIZE];
    SDL_Window *window;
    SDL_Renderer *renderer;
    float renderScale;
    TTF_Font *font;
    int windowWidth;
    int windowHeight;
    int renderWidth;
    int renderHeight;
    float lineHeight;
    SDL_Color textColor;
    SDL_Color bgColor;
    struct Cursor cursor;
    unsigned int lines;
    unsigned int shouldRerenderLines;
    unsigned int shouldRerenderCursor;
    struct Line* currentLine;
    struct Line* first;
    struct Line* last;
};

void textEngineAppendChar(struct TextEngine *te, char c);

struct TextEngine* textEngineInit(SDL_Window *window, SDL_Renderer *renderer, TTF_Font *font, SDL_Color textColor, SDL_Color bgColor)
{
    struct TextEngine* te = malloc(sizeof(*te));
    memset(te, 0, sizeof(*te));
    te->window = window;
    te->renderer = renderer;
    te->font = font;
    te->textColor = textColor;
    te->bgColor = bgColor;
    SDL_GetWindowSizeInPixels(te->window, &te->windowWidth, &te->windowHeight);
    SDL_GetRendererOutputSize(te->renderer, &te->renderWidth, &te->renderHeight);
    te->renderScale = (float) te->renderWidth / (float) te->windowWidth;
    SDL_RenderSetScale(te->renderer, te->renderScale, te->renderScale);
    te->lineHeight = (float) TTF_FontLineSkip(te->font) / te->renderScale;
    te->cursor.blinkStart = SDL_GetTicks();
    te->cursor.blinkIntervalMillis = 500;
    te->cursor.blinkState = 0xFF;
    te->cursor.offsetX = 0;
    te->cursor.offsetY = 0;
    te->cursor.width = 2.0;
    return te;
}

void textEngineAppendLine(struct TextEngine* te)
{
    if (te == NULL) return;
    struct Line *line = getLine();
    if (te->last == NULL)
    {
        te->first = line;
        te->last = line;
    }
    else
    {
        te->last->next = line;
        line->prev = te->last;
        line->offsetY = te->last->offsetY + te->lineHeight;
        te->last = line;
    }
    te->lines++;
    te->currentLine = line;
    return;
}

void textEnginePopLine(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->lines == 0) return;
    te->lines--;
    if (te->currentLine == te->first)
    {
        stringBuilderCleanUp(te->first->sb);
        free(te->first);
        te->first = NULL;
        te->last = NULL;
    }
    else
    {
        stringBuilderCleanUp(te->last->sb);
        te->last = te->last->prev;
        free(te->last->next);
        te->last->next = NULL;
    }
    te->currentLine = te->last;
    return;
}

void textEngineRenderLine(struct TextEngine *te, struct Line *line)
{
    SDL_FRect oldTextRect = {line->offsetX, line->offsetY, (float) te->renderWidth, (float) te->lineHeight};
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderFillRectF(te->renderer, &oldTextRect);
    stringBuilderToString(line->sb, te->buffer, IN_OUT_BUFFER_SIZE);
    line->shouldRerender = 0;
    if (line->sb->size != 0)
    {
        SDL_Surface *textSurface = TTF_RenderUTF8_Blended_Wrapped(te->font, te->buffer, te->textColor, 0);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(te->renderer, textSurface);
        SDL_FRect textRect = 
        {
            line->offsetX, 
            line->offsetY, 
            (float) textSurface->w / te->renderScale, 
            (float) textSurface->h / te->renderScale
        }; 
        line->width = textRect.w;
        line->height = textRect.h;
        SDL_FreeSurface(textSurface);
        SDL_RenderCopyF(te->renderer, textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
    }
    SDL_RenderPresent(te->renderer);
}

void textEnginePopCharUTF8(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->lines == 0) return;
    if (te->currentLine->sb->size == 0)
    {
        textEngineRenderLine(te, te->currentLine);
        textEnginePopLine(te);
    }
    else
    {
        stringBuilderPopUTF8(te->currentLine->sb);
        te->currentLine->shouldRerender = 1;
    }
    te->shouldRerenderLines = 1;
    return;
}

void textEngineAppendChar(struct TextEngine *te, char c)
{
    if(te == NULL) return;
    if (te->lines == 0)
    {
        textEngineAppendLine(te);
    }
    stringBuilderAppend(te->currentLine->sb, c);
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    return;
}

void textEngineAppendString(struct TextEngine *te, char *s)
{
    if (te == NULL) return;
    if (te->lines == 0)
    {
        textEngineAppendLine(te);
    }
    stringBuilderAppendString(te->currentLine->sb, s);
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    return;
}

void textEnginePollCursorBlinkTimer(struct TextEngine *te)
{
    unsigned int blinkEnd = SDL_GetTicks();
    unsigned int deltaTime = blinkEnd - te->cursor.blinkStart;
    if (deltaTime >= te->cursor.blinkIntervalMillis)
    {
        te->cursor.blinkStart = blinkEnd;
        te->shouldRerenderCursor = 1;
    }
    return;
}

void textEngineRenderCursor(struct TextEngine *te)
{   
    SDL_FRect cursorRect = {0};
    if (te->lines > 0)
    {
        // disgusting hack but will have to do for now
        if (te->currentLine->sb->size == 0) cursorRect.x = 0;
        else cursorRect.x = te->currentLine->width;
        cursorRect.y = te->currentLine->offsetY;
    }
    cursorRect.w = te->cursor.width;
    cursorRect.h = te->lineHeight;
    if (te->shouldRerenderCursor)
    {
        te->cursor.blinkState = te->cursor.blinkState ^ 0xFFFF0000;
        te->shouldRerenderCursor = 0;
    };
    SDL_Color cursorColor = 
    {
        (te->cursor.blinkState >> 24) & 0xFF, 
        (te->cursor.blinkState >> 16) & 0xFF, 
        (te->cursor.blinkState >> 8) & 0xFF,                 
        te->cursor.blinkState & 0xFF
    };
    SDL_Color cachedColor;
    SDL_GetRenderDrawColor(te->renderer, &cachedColor.r, &cachedColor.g, &cachedColor.b, &cachedColor.a);
    // update cords
    te->cursor.offsetX = cursorRect.x;
    te->cursor.offsetY = cursorRect.y;
    // render new cursor
    SDL_SetRenderDrawColor(te->renderer, cursorColor.r, cursorColor.g, cursorColor.b, cursorColor.a);
    SDL_RenderFillRectF(te->renderer, &cursorRect);
    SDL_SetRenderDrawColor(te->renderer, cachedColor.r, cachedColor.b, cachedColor.g, cachedColor.a);
    return;
}

void textEngineRerenderLines(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->lines == 0) return;
    struct Line *line = te->first;
    while (line)
    {
        if (line->shouldRerender)
        {
            textEngineRenderLine(te, line);
        }
        line = line->next;
    }
    te->shouldRerenderLines = 0;
    return;
}

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    char titleBuffer[64];
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        return 1;
    }
    
    if (TTF_Init())
    {
        fprintf(stderr, "Failed to init SDL_TTF: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("DTX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_ALLOW_HIGHDPI);
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
    int rw, rh;
    SDL_GetRendererOutputSize(renderer, &rw, &rh);
    float wScale = (float) rw / 800.0;
    float hScale = (float) rh / 600.0;
    // SDL_RenderSetScale(renderer, wScale, hScale);
    int fontSize = 16;
    const char *fontName = "./AovelSansRounded-rdDL.ttf";
    font = TTF_OpenFont(fontName, (int) (fontSize * wScale));
    if (font == NULL)
    {
        fprintf(stderr, "Failed to open font: %s\n", SDL_GetError());
        return 1;
    }
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT_SUBPIXEL);
    unsigned int frameStart;
    unsigned int frameEnd;
    unsigned int timerStart = SDL_GetTicks();
    int isRunning = 1;

    struct TextEngine *te = textEngineInit(window, renderer, font, (SDL_Color) {255, 255, 255, 255}, (SDL_Color) {0, 0, 0, 255});
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
            case SDL_TEXTINPUT:
                textEngineAppendString(te, event.text.text);
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.mod & KMOD_SHIFT)
                {
                    switch (event.key.keysym.sym)
                    {
                        case SDLK_KP_PLUS:
                            fontSize += 8;
                            TTF_CloseFont(font);
                            font = TTF_OpenFont(fontName, (int) (fontSize * wScale));
                            // TTF_SizeUTF8(font, " ", &cursor.fontWidth, &cursor.fontHeight);
                            // update prevSize to force rerender
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            break;
                        case SDLK_KP_MINUS:
                            if (fontSize > 8)
                            {
                            fontSize -= 8;
                            TTF_CloseFont(font);
                            font = TTF_OpenFont(fontName, (int) (fontSize * wScale));
                            // TTF_SizeUTF8(font, " ", &cursor.fontWidth, &cursor.fontHeight);
                            // update prevSize to force rerender
                            }
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            break;
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
        frameEnd = SDL_GetTicks();
        float frameTime = (frameEnd - frameStart) / 1000.0;
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
    // stringBuilderCleanUp(sb);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}