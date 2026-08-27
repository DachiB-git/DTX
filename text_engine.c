#include "text_engine.h"

unsigned int allocated_bytes_lines = 0;
unsigned int allocated_bytes_sbs = 0;

void cursorSeekPrevCodePoint(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine->sb->gapStart == 0) return;
    te->cursor.columnNumber--;
    // transfer cont bytes
    while (te->currentLine->sb->gapStart > 0 && isUTF8ContByte(te->currentLine->sb->gapBuffer[--te->currentLine->sb->gapStart]))
    {
        if (!te->cursor.transferIsActive) continue;
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapEnd--] = te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart];
    }
    // transfer head byte
    if (te->cursor.transferIsActive)
    {
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapEnd--] = te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart];
    }
    else
    {
        te->currentLine->sb->count--;
    }
    te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart] = '\0';
    return;
}

void cursorSeekNextCodePoint(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine->sb->gapEnd == te->currentLine->sb->capacity - 1) return;
    te->cursor.columnNumber++;
    // we are either on a single byte or multi byte lead character
    // move it over to the left side
    te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart++] = te->currentLine->sb->gapBuffer[++te->currentLine->sb->gapEnd];
    // analyze the moved byte to determine the rest of the operations
    char c = te->currentLine->sb->gapBuffer[te->currentLine->sb->gapEnd];
    int shifts = 0;
    // ascii
    if ((c & 0x80) == 0) return;
    // two byte
    else if ((c & 0xE0) == 0xC0)
    {
        shifts = 1;
    }
    // three bytes
    else if ((c & 0xF0) == 0xE0)
    {
        shifts = 2;
    }
    // four bytes
    else
    {
        shifts = 3;
    }
    for (int i = 0; i < shifts; i++)
    {
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart++] = te->currentLine->sb->gapBuffer[++te->currentLine->sb->gapEnd];
    }
    te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart] = '\0';
    return;
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
    if (te->currentLine == te->first && te->cursor.columnNumber == 0) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine == te->first)
    {
        textEngineShiftGapStart(te);
    }
    else
    {
        te->currentLine = te->currentLine->prev;
        textEngineShiftGapEnd(te);
        te->cursor.columnNumber = te->currentLine->sb->count;
        te->currentLine->shouldRerender = 1;
    }
    if (te->currentLine->offsetY <= SDL_roundf((float) (te->windowHeight - te->lineHeight) * (1.0 - te->autoScrollRatio)) && te->frameFirst != te->first)
    {
        te->frameFirst = te->frameFirst->prev;
        if (te->frameLast->offsetY >= SDL_roundf((float) (te->windowHeight - te->lineHeight * 2)))
        {
            te->frameLast = te->frameLast->prev;
        }
        SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
        SDL_RenderClear(te->renderer);
        textEngineRecalculateLines(te);
        textEngineUpdateFrameState(te);
    }
    cursorResetBlinkState(te);
    return;
}

void cursorMoveDown(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->last && te->cursor.columnNumber == te->currentLine->sb->count) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine == te->last)
    {
        textEngineShiftGapStart(te);
    }
    else
    {
        te->currentLine = te->currentLine->next;
        textEngineShiftGapEnd(te);
        te->cursor.columnNumber = te->currentLine->sb->count;
        te->currentLine->shouldRerender = 1;
    }
    if (te->cursor.scrollIsActive && te->currentLine->offsetY >= SDL_roundf((float) (te->windowHeight - te->lineHeight) * te->autoScrollRatio) && te->frameLast != te->last)
    {
        te->frameFirst = te->frameFirst->next;
        te->frameLast = te->frameLast->next;
        SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
        SDL_RenderClear(te->renderer);
        textEngineRecalculateLines(te);
    }
    cursorResetBlinkState(te);
    return;
}

void cursorMoveLeft(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->first && te->cursor.columnNumber == 0) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine->sb->size == 0 || te->cursor.columnNumber == 0)
    {
        cursorMoveUp(te);
    }
    else
    {
        cursorSeekPrevCodePoint(te);
        cursorResetBlinkState(te);
    }
    return;
}

void cursorMoveRight(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->currentLine == te->last && te->cursor.columnNumber == te->currentLine->sb->count) return;
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    if (te->currentLine->sb->size == 0 || te->cursor.columnNumber == te->currentLine->sb->count)
    {
        cursorMoveDown(te);
        textEngineShiftGapStart(te);
    }
    else
    {
        cursorSeekNextCodePoint(te);
        cursorResetBlinkState(te);
    }
    return;
}

float cursorGetOffsetWidth(struct TextEngine *te)
{
    return ((float) te->charAdvance / te->renderScale) * te->cursor.columnNumber;
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
    allocated_bytes_sbs += sizeof(*sb);
    memset(sb, 0, sizeof(*sb));
    // size + 1 for null terminator
    sb->gapBuffer = calloc(GAP_BUFFER_SIZE + 1, sizeof(char));
    allocated_bytes_sbs += sizeof(char) * GAP_BUFFER_SIZE;
    sb->capacity = GAP_BUFFER_SIZE;
    sb->gapEnd = sb->capacity - 1;
    return sb;
}

struct Node* getNode(char c, struct Node *next, struct Node *prev)
{
    struct Node* newNode = malloc(sizeof(*newNode));
    allocated_bytes_sbs += sizeof(*newNode);
    if (newNode == NULL) return newNode;
    newNode->c = c;
    newNode->next = next;
    newNode->prev = prev;
    return newNode;
}

// void stringBuilderPop(struct StringBuilder *sb)
// {
//     if (sb == NULL) return;
//     if (sb->size == 0) return;
//     sb->size--;
//     if (sb->head == sb->tail)
//     {
//         free(sb->head);
//         sb->head = NULL;
//         sb->tail = NULL;
//         return;
//     }
//     struct Node* oldTail = sb->tail;
//     sb->tail = sb->tail->prev;
//     sb->tail->next = NULL;
//     free(oldTail);
//     return;
// }

void stringBuilderPopUTF8(struct TextEngine *te, struct StringBuilder *sb)
{
    if (sb == NULL) return;
    if (sb->size == 0) return;
    // while (sb->size > 0 && isUTF8ContByte(sb->tail->c)) stringBuilderPop(sb);
    // if (sb->tail->c == ' ' && te->currentLine->indentationEnd == sb->tail)
    // {
    //     te->currentLine->indentationDepth--;
    //     stringBuilderPop(sb);
    //     te->currentLine->indentationEnd = sb->tail;
    // }
    // else
    // {
    //     stringBuilderPop(sb);
    // }
    return;
}

// returns 0 on successful append, 1 on error
int stringBuilderAppend(struct StringBuilder *sb, char c)
{
    sb->size++;
    sb->gapBuffer[sb->gapStart++] = c;
    if (!isUTF8ContByte(c)) sb->count++;
    if (sb->size == sb->capacity)
    {
        unsigned int oldCapacity = sb->capacity;
        sb->capacity <<= 1;
        sb->gapBuffer = realloc(sb->gapBuffer, sizeof(char) * (sb->capacity + 1));
        memset(sb->gapBuffer + oldCapacity, 0, sizeof(char) * (oldCapacity + 1));
        // now there a multiple invariants here to what should happen to the buffer
        // a. the cursor is at the end      a b * _ _
        // A: no need to transfer anything since the right side of the gap is empty
        // b. cursor it at the beginning    * _ _ a b
        // B: after realloc, we need to move the old right side of the gap to the end of the enlarged space
        // c. cursor is in the middle       a * _ _ b
        // C: same idea as in B, since cursor being in the middle can be interpreted as sub portion of the buffer with the cursor at the start
        // so the only time we don't need to transfer is when the cursor is at the end of the line
        if (sb->gapEnd != oldCapacity - 1)
        {
            size_t len = oldCapacity - 1 - sb->gapEnd;
            unsigned char *src = sb->gapBuffer + sb->gapEnd + 1;
            sb->gapEnd = sb->capacity - 1 - len;
            unsigned char *dst = sb->gapBuffer + sb->gapEnd + 1;
            SDL_memmove(dst, src, len);
        }
        else
        {
            sb->gapEnd = sb->capacity - 1;
        }
    }
    sb->gapBuffer[sb->gapStart] = '\0';
    return 0;
}

int stringBuilderAppendString(struct TextEngine *te, struct StringBuilder *sb, char *s)
{
    while(*s)
    {
        if (sb->size >= IN_OUT_BUFFER_SIZE - 1) break;
        if (stringBuilderAppend(sb, *s) != 0) return 1;
        if (sb->gapBuffer[sb->gapStart - 1] == ' ' && te->currentLine->indentationEnd == te->currentLine->sb->gapStart - 1)
        {
            te->currentLine->indentationDepth++;
            te->currentLine->indentationEnd++;
        }
        te->cursor.columnNumber++;
        s++;
    }
    return 0;
}

// Moves all the characters in the current line's gap buffer from the left to the right side.
void textEngineShiftGapStart(struct TextEngine *te)
{
    if (te->currentLine->sb->gapStart != 0)
    {
        char *dst = te->currentLine->sb->gapBuffer + te->currentLine->sb->gapEnd - (te->currentLine->sb->gapStart - 1);
        char *src = te->currentLine->sb->gapBuffer;
        size_t len = te->currentLine->sb->gapStart;
        te->currentLine->sb->gapStart = 0;
        te->currentLine->sb->gapEnd -= len;
        SDL_memmove(dst, src, len);
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart] = 0;
    }
    te->cursor.columnNumber = 0;
    return;
}

// Moves all the characters in the current line's gap buffer from the right to the left side.
void  textEngineShiftGapEnd(struct TextEngine *te)
{
    if (te->currentLine->sb->gapEnd != te->currentLine->sb->capacity - 1)
    {
        char *dst = te->currentLine->sb->gapBuffer + te->currentLine->sb->gapStart;
        char *src = te->currentLine->sb->gapBuffer + te->currentLine->sb->gapEnd + 1;
        size_t len = (te->currentLine->sb->capacity - 1) - te->currentLine->sb->gapEnd;
        te->currentLine->sb->gapEnd = te->currentLine->sb->capacity - 1;
        te->currentLine->sb->gapStart += len;
        SDL_memmove(dst, src, len);
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart] = '\0';
    }
    te->cursor.columnNumber = te->currentLine->sb->count;
    return;
}

// void stringBuilderToString(struct StringBuilder *sb, char *buffer, unsigned int size)
// {
//     if (sb == NULL) return;
//     struct Node* n = sb->head;
//     unsigned int count = 0;
//     while (n)
//     {
//         if (count >= size - 1) break;
//         buffer[count++] = n->c;
//         n = n->next;
//     }
//     buffer[count] = '\0';
//     return;
// }

// unsigned int stringBuilderCalculateSize(struct StringBuilder *sb)
// {
//     struct Node *head = sb->head;
//     unsigned int size = 0;
//     while (head)
//     {
//         size++;
//         head = head->next;
//     }
//     return size;
// }

struct Line* getLine(struct TextEngine *te, struct Line *next, struct Line *prev)
{
    struct Line *line = malloc(sizeof(*line));
    allocated_bytes_lines += sizeof(*line);
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
    // te->renderer = SDL_CreateRenderer(te->window, -1, SDL_RENDERER_ACCELERATED);
    te->renderer = SDL_CreateRenderer(te->window, -1, SDL_RENDERER_SOFTWARE);
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
    // monospace advance same for all chars
    TTF_GlyphMetrics(te->font, '0', NULL, NULL, NULL, NULL, &te->charAdvance);
    // prerender character glyphs
    // printable character range 0x20-0x7E
    for (unsigned int c = PRINTABLE_GLYPH_RANGE_LOW; c <= PRINTABLE_GLYPH_RANGE_HIGH; c++)
    {
        SDL_Surface *charSurface = TTF_RenderGlyph32_Blended(te->font, c, te->textColor);
        te->glyphs[c].texture = SDL_CreateTextureFromSurface(te->renderer, charSurface);
        te->glyphs[c].width = charSurface->w;
        te->glyphs[c].height = charSurface->h;
        SDL_FreeSurface(charSurface);
    }
    te->lineHeight = (int) ((TTF_FontHeight(te->font) / te->renderScale) * 1.2);
    te->halfLeading = (float) (te->lineHeight - (int) (TTF_FontHeight(te->font) / te->renderScale)) / 2.0;
    te->lines = 0;
    te->autoScrollRatio = 0.80;
    te->cursor.scrollIsActive = 0;
    te->cursor.transferIsActive = 1;
    te->cursor.blinkStart = SDL_GetTicks();
    te->cursor.blinkIntervalMillis = 500;
    te->cursor.blinkState = SDLColorToInt(bgColor);
    te->cursor.columnNumber = 0;
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
                            te->lineHeight = (int) ((TTF_FontHeight(te->font) / te->renderScale) * 1.2);
                            te->halfLeading = (float) (te->lineHeight - (int) (TTF_FontHeight(te->font) / te->renderScale)) / 2.0;
                            TTF_GlyphMetrics(te->font, '0', NULL, NULL, NULL, NULL, &te->charAdvance);
                            for (char c = PRINTABLE_GLYPH_RANGE_LOW; c <= PRINTABLE_GLYPH_RANGE_HIGH; c++)
                            {
                                SDL_Surface *charSurface = TTF_RenderGlyph_Blended(te->font, c, te->textColor);
                                te->glyphs[c].texture = SDL_CreateTextureFromSurface(te->renderer, charSurface);
                                te->glyphs[c].width = charSurface->w;
                                te->glyphs[c].height = charSurface->h;
                                SDL_FreeSurface(charSurface);
                            }
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
                            SDL_RenderClear(te->renderer);
                            textEngineRecalculateLines(te);
                            textEngineUpdateFrameState(te);
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
                            te->lineHeight = (int) ((TTF_FontHeight(te->font) / te->renderScale) * 1.2);
                            te->halfLeading = (float) (te->lineHeight - (int) (TTF_FontHeight(te->font) / te->renderScale)) / 2.0;
                            TTF_GlyphMetrics(te->font, '0', NULL, NULL, NULL, NULL, &te->charAdvance);
                            for (char c = PRINTABLE_GLYPH_RANGE_LOW; c <= PRINTABLE_GLYPH_RANGE_HIGH; c++)
                            {
                                SDL_Surface *charSurface = TTF_RenderGlyph_Blended(te->font, c, te->textColor);
                                te->glyphs[c].texture = SDL_CreateTextureFromSurface(te->renderer, charSurface);
                                te->glyphs[c].width = charSurface->w;
                                te->glyphs[c].height = charSurface->h;
                                SDL_FreeSurface(charSurface);
                            }
                            SDL_FlushEvent(SDL_TEXTINPUT);
                            SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
                            SDL_RenderClear(te->renderer);
                            textEngineRecalculateLines(te);
                            textEngineUpdateFrameState(te);
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

void textEngineUpdateFrameState(struct TextEngine *te)
{
    if (te == NULL) return;
    int maxVisibleLineNumber = te->windowHeight / te->lineHeight - 2 + te->frameFirst->lineNumber - 1;
    if (te->last->lineNumber <= maxVisibleLineNumber)
    {
        te->frameLast = te->last;
    }
    else
    {
        te->frameLast = te->frameFirst;
        while (te->frameLast->lineNumber < maxVisibleLineNumber) te->frameLast = te->frameLast->next;
    }
    // cursor out of bounds
    // reset to last visible line
    if (te->cursor.scrollIsActive && te->currentLine->lineNumber > te->frameLast->lineNumber)
    {
        te->currentLine = te->frameLast;
        te->cursor.columnNumber = te->currentLine->sb->count;
    }
    return;
} 

void textEngineAppendLine(struct TextEngine *te)
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
        te->cursor.columnNumber = 0;
        te->currentLine->shouldRerender = 1;
        te->shouldRerenderLines = 1;
        te->frameFirst = te->first;
        te->frameLast = te->last;
        cursorResetBlinkState(te);
        return;
    }
    line = getLine(te, NULL, NULL);
    te->lines++;
    line->lineNumber = te->lines;
    struct Line *cachedCurrentLine = te->currentLine;
    unsigned int cachedCursorPos = te->cursor.columnNumber;
    if (te->currentLine == te->last)
    {
        line->prev = te->last;
        te->last->next = line;
        line->offsetY = te->last->offsetY + te->lineHeight;
        te->last = line;
        textEngineUpdateFrameState(te);
    }
    else
    {
        line->offsetY = te->currentLine->offsetY + te->lineHeight;
        line->prev = te->currentLine;
        struct Line *rest = te->currentLine->next;
        te->currentLine->next = line;
        line->next = rest;
        rest->prev = line;
        textEngineRecalculateLines(te);
        textEngineUpdateFrameState(te);
    }
    // restore cursor pos
    te->cursor.columnNumber = cachedCursorPos;
    // cursor is not at the end
    if (te->cursor.columnNumber != te->currentLine->sb->count)
    {
        te->currentLine = line;
        te->cursor.columnNumber = te->currentLine->sb->count;
        textEngineAppendString(te, cachedCurrentLine->sb->gapBuffer + cachedCurrentLine->sb->gapEnd + 1);
        te->currentLine = cachedCurrentLine;
        te->cursor.columnNumber = cachedCursorPos;
        te->currentLine->sb->count -= textEngineCountChars(te->currentLine->sb->gapBuffer + te->currentLine->sb->gapEnd + 1);
        te->currentLine->sb->size -= te->currentLine->sb->capacity - 1 - te->currentLine->sb->gapEnd;
        te->currentLine->sb->gapBuffer[te->currentLine->sb->gapStart] = '\0';
        te->currentLine->sb->gapEnd = te->currentLine->sb->capacity - 1;
        cursorMoveDown(te);
        textEngineShiftGapStart(te);
    }
    else
    {
        cursorMoveDown(te);
    }
    if (te->currentLine->indentationDepth != te->currentLine->prev->indentationDepth)
    {
        for (int i = 0; i < te->currentLine->prev->indentationDepth; i++)
        {
            textEngineAppendString(te, " ");
        }
    }
    return;
}

void textEngineRemoveLine(struct TextEngine *te, struct Line *line)
{
    if (te == NULL) return;
    if (line == NULL) return;
    if (line == te->first) return;
    te->lines--;
    line->prev->next = line->next;
    textEngineClearLine(te, line);
    if (line->next)
    {
        line->next->prev = line->prev;
        textEngineClear(te);
        textEngineRecalculateLines(te);
    }
    else
    {
        te->last = te->last->prev;
        textEngineUpdateFrameState(te);
    }
    line->next = NULL;
    lineCleanUp(line);
    return;
}

void textEnginePopLine(struct TextEngine *te)
{
    if (te == NULL) return;
    te->lines--;
    te->last = te->last->prev;
    textEngineClearLine(te, te->last->next);
    lineCleanUp(te->last->next);
    te->last->next = NULL;
    return;
}

void textEngineClearLine(struct TextEngine *te, struct Line *line)
{
    SDL_FRect oldTextRect = {0, line->offsetY, (float) te->renderWidth, (float) te->lineHeight};
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderFillRectF(te->renderer, &oldTextRect);
    return;
}

// Returns the number of packed utf8 characters in the buffer.
// This functions assumes that the buffer is always a well formed sequence of utf8 bytes.
unsigned int textEngineCountChars(unsigned char *buffer)
{
    unsigned int count = 0;
    while(*buffer)
    {
        if (!isUTF8ContByte(*buffer)) count++;
        buffer++;
    }
    return count;
}

// Packs utf8 bytes into a single 32 bit integer.
// This functions assumes that the buffer is always a well formed sequence of utf8 bytes and packed is not null.
// Returns number of bytes to advance.
unsigned int textEnginePackUTF8(unsigned char *buffer, unsigned int *packed)
{
    unsigned int advance = 0;
    do
    {
        *packed = 0;
        *packed <<= 8;
        *packed |= *buffer;
        buffer++;
        advance++;
    }
    while(*buffer && isUTF8ContByte(*buffer));
    return advance;
}

void textEngineRenderLine(struct TextEngine *te, struct Line *line)
{
    // clear current line
    SDL_FRect rect = {0, line->offsetY, (float) te->windowWidth, (float) te->lineHeight};
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderFillRectF(te->renderer, &rect);
    // calculate and render current line number
    char lineCounterBuffer[16];
    snprintf(lineCounterBuffer, 16, "%4d", line->lineNumber);
    rect.x = 0;
    rect.y = line->offsetY + te->halfLeading;
    for (char *c = lineCounterBuffer; *c != '\0'; c++)
    {
        struct Glyph glyph = te->glyphs[*c];
        rect.w = (float) glyph.width / te->renderScale;
        rect.h = (float) glyph.height / te->renderScale;
        SDL_SetTextureColorMod(glyph.texture, 255, 255, 0);
        SDL_RenderCopyF(te->renderer, glyph.texture, NULL, &rect);
        SDL_SetTextureColorMod(glyph.texture, 255, 255, 255);
        rect.x += (float) te->charAdvance / te->renderScale;
    }
    // reset flag
    line->shouldRerender = 0;
    // highlight if on current line
    if (line == te->currentLine)
    {
        SDL_FRect highlightRect = {line->offsetX, line->offsetY, (float) te->renderWidth, te->lineHeight};
        SDL_SetRenderDrawColor(te->renderer, 31, 31, 31, 255);
        SDL_RenderFillRectF(te->renderer, &highlightRect);
    }
    // render text if size > 0
    if (line->sb->size != 0)
    {
        struct Glyph charGlyph;
        unsigned int packed = 0;
        rect.x = line->offsetX;
        rect.y = line->offsetY + te->halfLeading;
        unsigned int i = 0;
        // a b c _ _
        // 1 = buffer + 0
        // 2 = buffer + 1
        // 3 = buffer + 2
        while (i < line->sb->gapStart)
        {
            i += textEnginePackUTF8(line->sb->gapBuffer + i, &packed);
            charGlyph = te->glyphs[packed];
            rect.w = (float) charGlyph.width / te->renderScale;
            rect.h = (float) charGlyph.height / te->renderScale;
            SDL_RenderCopyF(te->renderer, charGlyph.texture, NULL, &rect);
            rect.x += (float) (te->charAdvance) / te->renderScale;
        }
        i = line->sb->gapEnd + 1;
        while (i < line->sb->capacity)
        {
            i += textEnginePackUTF8(line->sb->gapBuffer + i, &packed);
            charGlyph = te->glyphs[packed];
            rect.w = (float) charGlyph.width / te->renderScale;
            rect.h = (float) charGlyph.height / te->renderScale;
            SDL_RenderCopyF(te->renderer, charGlyph.texture, NULL, &rect);
            rect.x += (float) (te->charAdvance) / te->renderScale;
        }
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
    snprintf(te->buffer, IN_OUT_BUFFER_SIZE, " Lines: %d Ln: %d Col: %d", te->lines, te->currentLine->lineNumber, te->cursor.columnNumber);
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
    if (te->currentLine == te->first && te->cursor.columnNumber == 0) return;
    // empty line, pop it
    if (te->currentLine->sb->size == 0)
    {
        cursorMoveUp(te);
        textEngineRemoveLine(te, te->currentLine->next);
    }
    // move current line data to the previous one
    else if (te->cursor.columnNumber == 0)
    {
        textEngineShiftGapEnd(te);
        cursorMoveUp(te);
        unsigned int oldCursorPosition = te->currentLine->sb->count;
        textEngineAppendString(te, te->currentLine->next->sb->gapBuffer);
        for (unsigned int i = te->currentLine->sb->count; i > oldCursorPosition; i--)
        {
            cursorMoveLeft(te);
        }
        textEngineRemoveLine(te, te->currentLine->next);
    }
    else
    {
        // delete the character at the cursor
        te->cursor.transferIsActive = 0;
        cursorMoveLeft(te);
        te->cursor.transferIsActive = 1;
        if (te->currentLine->indentationEnd == te->currentLine->sb->gapStart + 1)
        {
            te->currentLine->indentationDepth--;
            te->currentLine->indentationEnd--;
        }
    }
    te->currentLine->shouldRerender = 1;
    te->shouldRerenderLines = 1;
    cursorResetBlinkState(te);
    te->fileIsNotSaved = 1;
    return;
}

// void textEngineAppendChar(struct TextEngine *te, char c)
// {
//     if(te == NULL) return;
//     stringBuilderAppend(te->currentLine->sb, c);
//     te->currentLine->shouldRerender = 1;
//     te->shouldRerenderLines = 1;
//     te->cursor.currentNode = te->currentLine->sb->tail;
//     return;
// }

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
    int bytes_read = 0; 
    while (1)
    {
        bytes_read = fread(te->buffer, sizeof(char), IN_OUT_BUFFER_SIZE - 1, fh);
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
        if (bytes_read != IN_OUT_BUFFER_SIZE)
        {
            if (feof(fh))
            {
                break;
            }
            else if (ferror(fh))
            {
                fprintf(stderr, "Error reading file %s\n", te->fileName);
                break;
            }
        }
    }
    fclose(fh);
    te->fileIsNotSaved = 0;
    te->currentLine = te->first;
    te->frameFirst = te->first;
    te->cursor.scrollIsActive = 1;
    textEngineShiftGapStart(te);
    textEngineUpdateFrameState(te);
    return;
}

void textEngineWriteFile(struct TextEngine *te)
{
    FILE* outputFile = fopen("text.tmp", "w");
    struct Line *line = te->first;
    while (line)
    {
        if (fputs(line->sb->gapBuffer, outputFile) == EOF)
        {
            fprintf(stderr, "Error while writing to the file.");
            break;
        }
        if (fputs(line->sb->gapBuffer + line->sb->gapEnd + 1, outputFile) == EOF)
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
    stringBuilderAppendString(te, te->currentLine->sb, s);
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
    cursorRect.x = te->currentLine->offsetX + cursorGetOffsetWidth(te);
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
        SDL_SetRenderDrawColor(te->renderer, cursorColor.r, cursorColor.g, cursorColor.b, cursorColor.a);
        SDL_RenderFillRectF(te->renderer, &cursorRect);
    }
    return;
}

void textEngineRenderLines(struct TextEngine *te)
{
    if (te == NULL) return;
    if (te->shouldRerenderLines == 0) return;
    struct Line *line = te->frameFirst;
    while (line != te->frameLast->next)
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

// Clears the screen to the background color.
// Doesn't call SDL_RenderPresent.
void textEngineClear(struct TextEngine *te)
{
    SDL_SetRenderDrawColor(te->renderer, te->bgColor.r, te->bgColor.g, te->bgColor.b, te->bgColor.a);
    SDL_RenderClear(te->renderer);
    return;
}

void textEngineRecalculateLines(struct TextEngine *te)
{
    if (te == NULL) return;
    struct Line *line = te->frameFirst;
    line->offsetY = te->lineHeight;
    line->offsetX = te->fontSize * 4;
    line->shouldRerender = 1;
    line = line->next;
    while (line != te->frameLast->next)
    {
        line->shouldRerender = 1;
        line->offsetX = line->prev->offsetX;
        line->offsetY = line->prev->offsetY + te->lineHeight;
        line->lineNumber = line->prev->lineNumber + 1;
        line = line->next;
    }
    te->shouldRerenderLines = 1;
    te->last->lineNumber = te->lines;
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