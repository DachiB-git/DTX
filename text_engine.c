#include "text_engine.h"

struct Node* cursorSeekPrevCodePoint(struct TextEngine *te)
{
    if (te == NULL) return NULL;
    struct Node *node = te->cursor.currentNode;
    while (node && isUTF8ContByte(node->c)) node = node->prev;
    if (node == NULL) return NULL;
    return node->prev;
}

struct Node* cursorSeekNextCodePoint(struct TextEngine *te)
{
    if (te == NULL) return NULL;
    struct Node *node = te->cursor.currentNode;
    // empty lines can't show up here, so cursor is at the start
    if (node == NULL)
    {
        // set node to the first character of the line
        node = te->currentLine->sb->head;
    }
    else
    // cursor is inside the line, either at the last byte of a multibyte utf char
    // or a single byte code point
    // so we shift over to reach the next code point
    {
        node = node->next;
    }
    // 0xxxx
    // 110xx
    // 1110x
    // 11110
    if ((node->c & 0x80) == 0) return node;
    if ((node->c & 0xE0) == 0xC0) return node->next;
    if ((node->c & 0xF0) == 0xE0)
    {
        node = node->next;
        return node->next;
    }
    node = node->next;
    node = node->next;
    return node->next;
}

void cursorResetBlinkState(struct TextEngine *te)
{
    if (te == NULL) return;
    te->cursor.blinkState = 0xFF;
    te->cursor.blinkStart = SDL_GetTicks();
    te->shouldRerenderCursor = 1;
    return;
}

void cursorMoveUp(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->first && te->cursor.currentNode == NULL) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine == te->first)
    {
        te->cursor.currentNode = NULL;
    }
    else
    {
        te->currentLine = te->currentLine->prev;
        te->cursor.currentNode = te->currentLine->sb->tail;
    }
    te->currentLine->shouldRerender = 1;
    cursorResetBlinkState(te);
    return;
}

void cursorMoveDown(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->last && te->cursor.currentNode == te->currentLine->sb->tail) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine != te->last)
        te->currentLine = te->currentLine->next;
    te->cursor.currentNode = te->currentLine->sb->tail;
    te->currentLine->shouldRerender = 1;
    cursorResetBlinkState(te);
    return;
}

void cursorMoveLeft(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->first && te->cursor.currentNode == NULL) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine->sb->size == 0 || te->cursor.currentNode == NULL)
    {
        if (te->currentLine->prev == NULL) return;
        te->currentLine = te->currentLine->prev;
        te->cursor.currentNode = te->currentLine->sb->tail;
    }
    else
    {
        te->cursor.currentNode = cursorSeekPrevCodePoint(te);
    }
    te->currentLine->shouldRerender = 1;
    cursorResetBlinkState(te);
    return;
}

void cursorMoveRight(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->last && te->cursor.currentNode == te->currentLine->sb->tail) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine->sb->size == 0 || te->cursor.currentNode == te->currentLine->sb->tail)
    {
        if (te->currentLine->next == NULL) return;
        te->currentLine = te->currentLine->next;
        te->cursor.currentNode = NULL;
    }
    else
    {
        te->cursor.currentNode = cursorSeekNextCodePoint(te);
    }
    te->currentLine->shouldRerender = 1;
    cursorResetBlinkState(te);
    return;
}

float cursorGetOffsetWidth(struct TextEngine *te)
{
    struct Node *rest;
    rest = te->cursor.currentNode->next;
    te->cursor.currentNode->next = NULL;
    stringBuilderToString(te->currentLine->sb, te->buffer, IN_OUT_BUFFER_SIZE);
    te->cursor.currentNode->next = rest;
    int w, h;
    TTF_SizeUTF8(te->font, te->buffer, &w, &h);
    return (float) w / te->renderScale;
}

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

void lineCleanUp(struct Line *line)
{
    struct Line* curr;
    while (line)
    {
        curr = line;
        stringBuilderCleanUp(curr->sb);
        line = line->next;
        free(curr);
    }
    return;
}

struct StringBuilder* stringBuilderInit()
{
    struct StringBuilder* sb = malloc(sizeof(*sb));
    if (sb == NULL) return sb;
    memset(sb, 0, sizeof(*sb));
    return sb;
}

struct Node* getNode(char c, struct Node *next, struct Node *prev)
{
    struct Node* newNode = malloc(sizeof(*newNode));
    if (newNode == NULL) return newNode;
    newNode->c = c;
    newNode->next = next;
    newNode->prev = prev;
    return newNode;
}

void stringBuilderPop(struct StringBuilder *sb)
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

void stringBuilderPopUTF8(struct TextEngine *te, struct StringBuilder *sb)
{
    if (sb == NULL) return;
    if (sb->size == 0) return;
    while (sb->size > 0 && isUTF8ContByte(sb->tail->c)) stringBuilderPop(sb);
    if (sb->tail->c == ' ' && te->currentLine->indentationEnd == sb->tail)
    {
        te->currentLine->indentationDepth--;
        stringBuilderPop(sb);
        te->currentLine->indentationEnd = sb->tail;
    }
    else
    {
        stringBuilderPop(sb);
    }
    return;
}

// returns 0 on successful append, 1 on error
int stringBuilderAppend(struct StringBuilder *sb, char c)
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

int stringBuilderAppendString(struct TextEngine *te, struct StringBuilder *sb, char *s)
{
    while(*s)
    {
        if (sb->size >= IN_OUT_BUFFER_SIZE - 1) break;
        if (stringBuilderAppend(sb, *s) != 0) return 1;
        if (*s == ' ' && sb->tail->prev == te->currentLine->indentationEnd)
        {
            te->currentLine->indentationEnd = sb->tail;
            te->currentLine->indentationDepth++;
        }
        s++;
    }
    return 0;
}

void stringBuilderToString(struct StringBuilder *sb, char *buffer, unsigned int size)
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

unsigned int stringBuilderCalculateSize(struct StringBuilder *sb)
{
    struct Node *head = sb->head;
    unsigned int size = 0;
    while (head)
    {
        size++;
        head = head->next;
    }
    return size;
}

struct Line* getLine(struct TextEngine *te, struct Line *next, struct Line *prev)
{
    struct Line *line = malloc(sizeof(*line));
    if (line == NULL) return line;
    memset(line, 0, sizeof(*line));
    line->sb = stringBuilderInit();
    line->next = next;
    line->prev = prev;
    line->offsetY = te->lineHeight;
    line->offsetX = te->fontSize * 4;
    return line;
}

struct TextEngine* textEngineInit(char *fileName, int windowWidth, int windowHeight, char *fontFileName, int fontSize, SDL_Color textColor, SDL_Color bgColor)
{
    struct TextEngine* te = malloc(sizeof(*te));
    if (te == NULL)
    {
        fprintf(stderr, "Failed to malloc TextEngine. Terminating.");
        return NULL;
    }
    memset(te, 0, sizeof(*te));
    te->windowWidth = windowWidth;
    te->windowHeight = windowHeight;
    te->fontSize = fontSize;
    te->fileName = fileName;
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
    if (SDL_Init(SDL_INIT_EVERYTHING))
    {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        textEngineCleanUp(te);
        return NULL;
    }
    
    if (TTF_Init())
    {
        fprintf(stderr, "Failed to init SDL_TTF: %s\n", SDL_GetError());
        textEngineCleanUp(te);
        return NULL;
    }
    te->window = SDL_CreateWindow("DTX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, te->windowWidth, te->windowHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (te->window == NULL)
    {
        fprintf(stderr, "Failed to create a window: %s\n", SDL_GetError());
        textEngineCleanUp(te);
        return NULL;
    }
    char *iconPath = textEngineGetResourcePath("dtx.bmp");
    SDL_Surface* iconSurface = SDL_LoadBMP(iconPath);
    if (iconSurface != NULL)
    {
        SDL_SetWindowIcon(te->window, iconSurface);
        SDL_FreeSurface(iconSurface);
        free(iconPath);
    }
    te->renderer = SDL_CreateRenderer(te->window, -1, SDL_RENDERER_ACCELERATED);
    if (te->renderer == NULL)
    {
        fprintf(stderr, "Failed to create a renderer: %s\n", SDL_GetError());
        textEngineCleanUp(te);
        return NULL;
    }
    te->textColor = textColor;
    te->bgColor = bgColor;
    SDL_GetRendererOutputSize(te->renderer, &te->renderWidth, &te->renderHeight);
    te->renderScale = (float) te->renderWidth / (float) te->windowWidth;
    SDL_RenderSetScale(te->renderer, te->renderScale, te->renderScale);
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderClear(te->renderer);
    te->fontFilePath = textEngineGetResourcePath(fontFileName);
    te->font = TTF_OpenFont(te->fontFilePath, (int) (fontSize * te->renderScale));
    if (te->font == NULL)
    {
        fprintf(stderr, "Failed to open font: %s\n", SDL_GetError());
        textEngineCleanUp(te);
        return NULL;
    }
    TTF_SetFontHinting(te->font, TTF_HINTING_LIGHT_SUBPIXEL);
    te->lineHeight = (int) ((TTF_FontHeight(te->font) / te->renderScale) * 1.2);
    te->halfLeading = (float) (te->lineHeight - (int) (TTF_FontHeight(te->font) / te->renderScale)) / 2.0;
    te->lines = 0;
    te->cursor.blinkStart = SDL_GetTicks();
    te->cursor.blinkIntervalMillis = 500;
    te->cursor.blinkState = SDLColorToInt(bgColor);
    te->cursor.currentNode = NULL;
    te->cursor.offsetX = 0;
    te->cursor.offsetY = 0;
    te->cursor.width = 2.0;
    te->isRunning = 1;
    te->fileIsNotSaved = 0;
    SDL_ShowWindow(te->window);
    return te;
}

char* textEngineGetResourcePath(char *fileName)
{
    char *basePath = SDL_GetBasePath();
    size_t pathLen = SDL_strlen(basePath) + SDL_strlen(fileName) + 1;
    char *fullPath = malloc(sizeof(char) * pathLen);
    snprintf(fullPath, pathLen, "%s%s", basePath, fileName);
    SDL_free(basePath);
    return fullPath;
}

void textEngineHandleEvents(struct TextEngine *te)
{
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
            switch (event.key.keysym.sym)
            {
                case SDLK_ESCAPE:
                    SDL_PushEvent(&(SDL_Event){.type = SDL_QUIT});
                    break;
                case SDLK_BACKSPACE:
                    textEnginePopCharUTF8(te);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    textEngineAppendLine(te);
                    break;
                case SDLK_TAB:
                    textEngineAppendString(te, " ");
                    textEngineAppendString(te, " ");
                    textEngineAppendString(te, " ");
                    textEngineAppendString(te, " ");
                    cursorResetBlinkState(te);
                    break;
                case SDLK_DOWN:
                    cursorMoveDown(te);
                    break;
                case SDLK_UP:
                    cursorMoveUp(te);
                    break;
                case SDLK_LEFT:
                    cursorMoveLeft(te);
                    break;
                case SDLK_RIGHT:
                    cursorMoveRight(te);
                    break;
                case SDLK_KP_PLUS:
                    if (event.key.keysym.mod & KMOD_CTRL)
                    {
                        te->fontSize += 4;
                        TTF_Font *font = TTF_OpenFont(te->fontFilePath, (int) (te->fontSize * te->renderScale));
                        if (font == NULL)
                        {
                            fprintf(stderr, "Unable to reopen the font. %s. Falling back to font size: %d.\n", SDL_GetError(), te->fontSize - 4);
                            te->fontSize -= 4;
                        }
                        else
                        {
                            TTF_CloseFont(te->font);
                            te->font = font;
                            te->font = TTF_OpenFont(te->fontFilePath, (int) (te->fontSize * te->renderScale));
                            te->lineHeight = (float) TTF_FontLineSkip(te->font) / te->renderScale;
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
                            SDL_RenderClear(te->renderer);
                            textEngineRecalculateLines(te);
                        }
                    }
                    break;
                case SDLK_KP_MINUS:
                    if (event.key.keysym.mod & KMOD_CTRL && te->fontSize > 8)
                    {
                        te->fontSize -= 4;
                        TTF_Font *font = TTF_OpenFont(te->fontFilePath, (int) (te->fontSize * te->renderScale));
                        if (font == NULL)
                        {
                            fprintf(stderr, "Unable to reopen the font. %s. Falling back to font size: %d.\n", SDL_GetError(), te->fontSize + 4);
                            te->fontSize += 4;
                        }
                        else
                        {
                            TTF_CloseFont(te->font);
                            te->font = font;
                            te->font = TTF_OpenFont(te->fontFilePath, (int) (te->fontSize * te->renderScale));
                            te->lineHeight = (float) TTF_FontLineSkip(te->font) / te->renderScale;
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
                            SDL_RenderClear(te->renderer);
                            textEngineRecalculateLines(te);
                        }
                    }
                    break;
                case SDLK_s:
                    if (event.key.keysym.mod & KMOD_CTRL && te->fileIsNotSaved)
                    {
                        te->fileIsNotSaved = 0;
                        textEngineWriteFile(te);
                    }
                default:
                    break;
            }
            break;
        default:
            break;
        }
    }
}

void textEngineAppendLine(struct TextEngine* te)
{
    if (te == NULL) return;
    struct Line *line;
    te->fileIsNotSaved = 1;
    if (te->lines == 0)
    {
        line = getLine(te, NULL, NULL);
        te->lines++;
        line->lineNumber = te->lines;
        te->first = line;
        te->last = line;
        te->currentLine = line;
        te->cursor.currentNode = NULL;
        te->currentLine->shouldRerender = 1;
        te->shouldRerenderLines = 1;
        cursorResetBlinkState(te);
        return;
    }
    line = getLine(te, NULL, NULL);
    te->lines++;
    line->lineNumber = te->lines;
    // cursor is not at the end
    if (te->cursor.currentNode != te->currentLine->sb->tail)
    {
        // cursor is at the beginning
        line->sb->tail = te->currentLine->sb->tail;
        // move the whole line
        if (te->cursor.currentNode == NULL)
        {
            line->sb->head = te->currentLine->sb->head;
            line->sb->size = te->currentLine->sb->size;
            memset(te->currentLine->sb, 0, sizeof(*(te->currentLine->sb)));
        }
        else
        // move portion of the line
        {
            line->sb->head = te->cursor.currentNode->next;
            line->sb->head->prev = NULL;
            line->sb->size = stringBuilderCalculateSize(line->sb);
            te->currentLine->sb->tail = te->cursor.currentNode;
            te->currentLine->sb->tail->next = NULL;
            te->currentLine->sb->size -= line->sb->size;
        }
        line->shouldRerender = 1;
        te->shouldRerenderLines = 1;
    }
    if (te->currentLine == te->last)
    {
        line->prev = te->last;
        te->last->next = line;
        line->offsetY = te->last->offsetY + te->lineHeight;
        te->last = line;
        te->currentLine->shouldRerender = 1;
        te->currentLine = te->last;
        te->currentLine->shouldRerender = 1;
        te->cursor.currentNode = NULL;
        cursorResetBlinkState(te);
        te->shouldRerenderLines = 1;
    }
    else
    {
        line->offsetY = te->currentLine->offsetY + te->lineHeight;
        line->prev = te->currentLine;
        struct Line *rest = te->currentLine->next;
        te->currentLine->next = line;
        line->next = rest;
        rest->prev = line;
        te->currentLine = line;
        textEngineRecalculateLines(te);
        cursorResetBlinkState(te);
        te->cursor.currentNode = NULL;
    }
    if (te->currentLine->prev->indentationDepth > 0)
    {
        for (int i = 0; i < te->currentLine->prev->indentationDepth; i++)
        {
            textEngineAppendString(te, " ");
        }
    }
    return;
}

void textEnginePopLine(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->first) return;
    te->lines--;
    te->last = te->last->prev;
    lineCleanUp(te->last->next);
    te->last->next = NULL;
    te->currentLine = te->last;
    return;
}

void textEngineClearLine(struct TextEngine *te, struct Line *line)
{
    SDL_FRect oldTextRect = {0, line->offsetY, (float) te->renderWidth, (float) te->lineHeight};
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderFillRectF(te->renderer, &oldTextRect);
    return;
}

void textEngineRenderLine(struct TextEngine *te, struct Line *line)
{
    SDL_FRect oldTextRect = {0, line->offsetY, (float) te->renderWidth, (float) te->lineHeight};
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderFillRectF(te->renderer, &oldTextRect);
    char lineCounterBuffer[16];
    snprintf(lineCounterBuffer, 16, "%5d", line->lineNumber);
    SDL_Surface *lineCounterSurface = TTF_RenderUTF8_Blended(te->font, lineCounterBuffer, (SDL_Color) {255, 255, 0, 255});
    SDL_Texture *lineCounterTexture = SDL_CreateTextureFromSurface(te->renderer, lineCounterSurface);
    SDL_FRect lineCounterRect =
    {
        0,
        line->offsetY + te->halfLeading,
        (float) lineCounterSurface->w / te->renderScale,
        (float) lineCounterSurface->h / te->renderScale
    };
    SDL_FreeSurface(lineCounterSurface);
    SDL_RenderCopyF(te->renderer, lineCounterTexture, NULL, &lineCounterRect);
    SDL_DestroyTexture(lineCounterTexture);
    stringBuilderToString(line->sb, te->buffer, IN_OUT_BUFFER_SIZE);
    line->shouldRerender = 0;
    if (line == te->currentLine)
        {
            SDL_FRect highlightRect = {line->offsetX, line->offsetY, (float) te->renderWidth, te->lineHeight};
            SDL_SetRenderDrawColor(te->renderer, 31, 31, 31, 255);
            SDL_RenderFillRectF(te->renderer, &highlightRect);
        }
    if (line->sb->size != 0)
    {
        SDL_Surface *textSurface = TTF_RenderUTF8_Blended(te->font, te->buffer, te->textColor);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(te->renderer, textSurface);
        SDL_FRect textRect = 
        {
            line->offsetX, 
            line->offsetY + te->halfLeading, 
            (float) textSurface->w / te->renderScale, 
            (float) textSurface->h / te->renderScale
        };
        line->width = textRect.w;
        line->height = textRect.h;
        SDL_FreeSurface(textSurface);
        SDL_RenderCopyF(te->renderer, textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
    }
    else
    {
        line->width = 0;
    }
    return;
}

void textEngineRenderStatusBar(struct TextEngine *te)
{
    // find the bottom of the screen
    // reserve 1 lineHeight of space
    // render a UI box there
    // shows number of lines, characters, cursor location
    float statusBarOffsetY = (float) te->renderHeight / te->renderScale - te->lineHeight;
    SDL_FRect statusBarRect = 
    {
        0,
        statusBarOffsetY,
        (float) te->renderWidth,
        te->lineHeight
    };
    SDL_SetRenderDrawColor(te->renderer, 255, 255, 0, 255);
    SDL_RenderFillRectF(te->renderer, &statusBarRect);
    snprintf(te->buffer, IN_OUT_BUFFER_SIZE, " Lines: %d ", te->lines);
    SDL_Surface *statusBarSurface = TTF_RenderUTF8_Blended(te->font, te->buffer, (SDL_Color) {0, 0, 0, 255});
    SDL_Texture *statusBarTexture = SDL_CreateTextureFromSurface(te->renderer, statusBarSurface);
    SDL_FRect statusBarTextRect =
    {
        0,
        statusBarOffsetY + te->halfLeading,
        (float) statusBarSurface->w / te->renderScale,
        (float) statusBarSurface->h / te->renderScale
    };
    SDL_FRect statusBarFileStatusRect =
    {
        statusBarTextRect.w,
        statusBarOffsetY,
        (float) te->renderWidth,
        te->lineHeight
    };
    if (te->fileIsNotSaved)
    {
        SDL_SetRenderDrawColor(te->renderer, 255, 0, 0, 255);
    }
    else
    {
        SDL_SetRenderDrawColor(te->renderer, 255, 255, 0, 255);
    }
    SDL_RenderFillRectF(te->renderer, &statusBarFileStatusRect);
    SDL_RenderCopyF(te->renderer, statusBarTexture, NULL, &statusBarTextRect);
    SDL_DestroyTexture(statusBarTexture);
    SDL_FreeSurface(statusBarSurface);
    return;
}

void textEnginePopCharUTF8(struct TextEngine *te)
{
    if (te == NULL) return;
    te->fileIsNotSaved = 1;
    // cursor is anywhere but at the end of the line
    if (te->cursor.currentNode != te->currentLine->sb->tail)
    {
        struct Node *rest;
        struct Node *oldTail;
        // cursor is at the beginning of the line
        if (te->cursor.currentNode == NULL)
        {
            if (te->currentLine == te->first) return;
            // if the previous line is empty, just move the current line data
            if (te->currentLine->prev->sb->size == 0)
            {
                te->currentLine->prev->sb->size = te->currentLine->sb->size;
                te->currentLine->prev->sb->head = te->currentLine->sb->head;
                te->currentLine->prev->sb->tail = te->currentLine->sb->tail;
                oldTail = NULL;
            }
            else
            // else chain the two lines together
            {
                te->currentLine->prev->sb->tail->next = te->currentLine->sb->head;
                te->currentLine->sb->head->prev = te->currentLine->prev->sb->tail;
                oldTail = te->currentLine->prev->sb->tail;
                te->currentLine->prev->sb->tail = te->currentLine->sb->tail;
                te->currentLine->prev->sb->size += te->currentLine->sb->size;
            }                
            textEngineClearLine(te, te->currentLine);
            // manual free to preserve nodes
            free(te->currentLine->sb);
            te->currentLine->sb = NULL;
            struct Line *restLines = te->currentLine->next;
            struct Line *lastLine = te->last;
            te->last = te->currentLine;
            te->last->next = NULL;
            textEnginePopLine(te);
            if (restLines)
            {
                te->last->next = restLines;
                restLines->prev = te->last;
                te->last = lastLine;
                textEngineClearLine(te, te->last);
                textEngineRecalculateLines(te);
            }
            te->cursor.currentNode = oldTail;
        }
        else
        {
            // cursor is inside the line
            rest = te->cursor.currentNode->next;
            oldTail = te->currentLine->sb->tail;
            te->cursor.currentNode->next = NULL;
            te->currentLine->sb->tail = te->cursor.currentNode;
            stringBuilderPopUTF8(te, te->currentLine->sb);
            te->cursor.currentNode = te->currentLine->sb->tail;
            if (te->cursor.currentNode == NULL)
            {
                te->currentLine->sb->head = rest;
                rest->prev = NULL;
            }
            else
            {
                te->currentLine->sb->tail->next = rest;
                rest->prev = te->currentLine->sb->tail;
            }
            te->currentLine->sb->tail = oldTail;
        }
    }
    // cursor is at the end so just pop from tail
    else
    {
        if (te->currentLine->sb->size == 0)
        {
            if (te->currentLine == te->first) return;
            if (te->currentLine != te->last)
            {
                textEngineClearLine(te, te->last);
                struct Line *rest = te->currentLine->next;
                struct Line *oldLast = te->last;
                te->currentLine->next = NULL;
                te->last = te->currentLine;
                textEnginePopLine(te);
                te->currentLine->next = rest;
                rest->prev = te->currentLine;
                te->last = oldLast;
                textEngineRecalculateLines(te);
            }
            else
            {
                textEngineClearLine(te, te->last);
                textEnginePopLine(te);
            }
        }
        else
        {
            stringBuilderPopUTF8(te, te->currentLine->sb);
        }
        te->cursor.currentNode = te->currentLine->sb->tail;
    }
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    cursorResetBlinkState(te);
    return;
}

void textEngineAppendChar(struct TextEngine *te, char c)
{
    if(te == NULL) return;
    stringBuilderAppend(te->currentLine->sb, c);
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    te->cursor.currentNode = te->currentLine->sb->tail;
    return;
}

void textEngineReadFile(struct TextEngine *te)
{
    FILE *fh = fopen(te->fileName, "r");
    if (fh == NULL)
    {
        // file not found, try creating it
        fh = fopen(te->fileName, "w");
    }
    snprintf(te->buffer, IN_OUT_BUFFER_SIZE, "DTX | %s", te->fileName);
    SDL_SetWindowTitle(te->window, te->buffer);
    // make sure to always have at least a single line ready
    textEngineAppendLine(te);
    int bytes_read = fread(te->buffer, sizeof(char), IN_OUT_BUFFER_SIZE - 1, fh);
    fclose(fh);
    te->buffer[bytes_read] = '\0';
    int i = 0;
    int acc = 0;
    int wordSize = 0;
    char *b = te->buffer;
    while (acc < bytes_read)
    {
        // we search for the closes newline
        // we change the newline to null terminator
        // we append the string to the engine
        // we insert a newline
        // we shift the buffer
        if (b[i] == '\0')
        {
            textEngineAppendString(te, b);
            break;
        }
        if (b[i] == '\n')
        {
            b[i++] = '\0';
            textEngineAppendString(te, b);
            unsigned int indentationCache = te->currentLine->indentationDepth;
            te->currentLine->indentationDepth = 0;
            textEngineAppendLine(te);
            te->currentLine->prev->indentationDepth = indentationCache;
            b += i;
            acc += i;
            i = 0;
            continue;
        }
        i++;
    }
    te->fileIsNotSaved = 0;
    return;
}

void textEngineWriteFile(struct TextEngine *te)
{
    FILE* outputFile = fopen("text.tmp", "w");
    struct Line *line = te->first;
    while (line)
    {
        stringBuilderToString(line->sb, te->buffer, IN_OUT_BUFFER_SIZE);
        if (fputs(te->buffer, outputFile) == EOF)
        {
            fprintf(stderr, "Error while writing to the file.");
            break;
        }
        line = line->next;
        if (line)
            fputc('\n', outputFile);
    }
    fflush(outputFile);
    fclose(outputFile);
#ifdef _WIN32
    remove(te->fileName);
#endif
    rename("text.tmp", te->fileName);
    te->fileIsNotSaved = 0;
}

void textEngineAppendString(struct TextEngine *te, char *s)
{
    if (te == NULL) return;
    te->fileIsNotSaved = 1;
    if (te->cursor.currentNode != te->currentLine->sb->tail)
    {
        struct Node *rest;
        struct Node *oldTail;
        oldTail = te->currentLine->sb->tail;
        if (te->cursor.currentNode != NULL)
        {
            rest = te->cursor.currentNode->next;
            te->cursor.currentNode->next = NULL;
            te->currentLine->sb->tail = te->cursor.currentNode;
        }
        else
        {
            rest = te->currentLine->sb->head;
            te->currentLine->sb->head = NULL;
            te->currentLine->sb->tail = NULL;
        }
        stringBuilderAppendString(te, te->currentLine->sb, s);
        te->currentLine->sb->tail->next = rest;
        rest->prev = te->currentLine->sb->tail;
        te->cursor.currentNode = te->currentLine->sb->tail;
        te->currentLine->sb->tail = oldTail;
    }
    else
    {
        stringBuilderAppendString(te, te->currentLine->sb, s);
        te->cursor.currentNode = te->currentLine->sb->tail;
    }
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    cursorResetBlinkState(te);
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
    cursorRect.y = te->currentLine->offsetY;
    cursorRect.x = te->currentLine->offsetX;
    if (te->cursor.currentNode != NULL)
    {
        if (te->cursor.currentNode == te->currentLine->sb->tail)
        {
            cursorRect.x += te->currentLine->width;
        }
        else
        {
            cursorRect.x += cursorGetOffsetWidth(te);
        }
    }
    cursorRect.w = te->cursor.width;
    cursorRect.h = te->lineHeight;
    if (te->shouldRerenderCursor)
    {
        te->cursor.blinkState = te->cursor.blinkState ^ 0xFFFF0000;
        te->shouldRerenderCursor = 0;
    };
    if (te->cursor.blinkState == SDLColorToInt(te->bgColor))
    {
        te->currentLine->shouldRerender = 1;
        te->shouldRerenderLines = 1;
    }
    else
    {
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
    }
    return;
}

void textEngineRenderLines(struct TextEngine *te)
{
    if (te == NULL) return;
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

void textEngineRecalculateLines(struct TextEngine *te)
{
    if (te == NULL) return;
    struct Line *line = te->first;
    line->offsetY = te->lineHeight;
    line->offsetX = te->fontSize * 4;
    line->shouldRerender = 1;
    line->lineNumber = 1;
    line = line->next;
    while (line)
    {
        line->shouldRerender = 1;
        line->offsetX = line->prev->offsetX;
        line->offsetY = line->prev->offsetY + te->lineHeight;
        line->lineNumber = line->prev->lineNumber + 1;
        line = line->next;
    }
    te->shouldRerenderLines = 1;
    return;
}

void textEngineCleanUp(struct TextEngine *te)
{
    if (te == NULL) return;
    SDL_DestroyWindow(te->window);
    SDL_DestroyRenderer(te->renderer);
    TTF_CloseFont(te->font);
    lineCleanUp(te->first);
    free(te->fontFilePath);
    TTF_Quit();
    SDL_Quit();
    free(te);
    return;
}