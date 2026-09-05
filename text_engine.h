#ifndef TEXT_ENGINE_H
#define TEXT_ENGINE_H

#include <stdio.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#define IN_OUT_BUFFER_SIZE 2048
#define GAP_BUFFER_SIZE 32
#define TARGET_FPS 60
#define TARGET_MILLIS_PER_FRAME (1000 / TARGET_FPS)
#define PRINTABLE_GLYPH_RANGE_LOW  0x20
#define PRINTABLE_GLYPH_RANGE_HIGH 0x7E
#define isUTF8ContByte(c) ((c & 0xC0) == 0x80)
#define SDLColorToInt(c) ((c.r << 24) | (c.g << 16) | (c.b << 8) | c.a)

// enum FILE_STATUS_CODE
// {
//     UNMODIFIED = 0,
//     UNSAVED,
//     SAVING,
// };

struct Glyph
{
    SDL_Texture *texture;
    int width;
    int height;
};

struct Cursor
{
    float width;
    unsigned int columnNumber;
    unsigned int scrollIsActive;
    unsigned int transferIsActive;
    unsigned int blinkStart;
    unsigned int blinkState;
    unsigned int blinkIntervalMillis;
};

struct Line
{
    float offsetX;
    float offsetY;
    unsigned int lineNumber;
    unsigned int indentationDepth;
    unsigned int indentationEnd;
    unsigned int shouldRerender;
    char *gapBuffer;
    unsigned int gapStart;
    unsigned int gapEnd;
    unsigned int count;
    unsigned int size;
    unsigned int capacity;
    struct Line *next;
    struct Line *prev;
};

struct TextEngine
{
    char buffer[IN_OUT_BUFFER_SIZE];
    struct Glyph glyphs[128];
    SDL_Window *window;
    SDL_Renderer *renderer;
    float renderScale;
    TTF_Font *font;
    char *fontFilePath;
    int fontSize;
    int charAdvance;
    int windowWidth;
    int windowHeight;
    int renderWidth;
    int renderHeight;
    int lineHeight;
    float halfLeading;
    SDL_Color textColor;
    SDL_Color bgColor;
    struct Cursor cursor;
    char *fileName;
    unsigned int fileIsNotSaved;
    unsigned int lines;
    unsigned int isRunning;
    unsigned int frameStart;
    unsigned int shouldRerenderLines;
    unsigned int shouldRerenderCursor;
    unsigned int shouldRerenderStatusBar;
    float autoScrollRatio;
    unsigned int tabsToSpacesCount;
    struct Line *currentLine;
    struct Line *first;
    struct Line *last;
    struct Line *frameFirst;
    struct Line *frameLast;
};

void cursorSeekNextCodePoint(struct TextEngine *te);
void cursorSeekPrevCodePoint(struct TextEngine *te);
void cursorMoveUp(struct TextEngine *te);
void cursorMoveDown(struct TextEngine *te);
void cursorMoveLeft(struct TextEngine *te);
void cursorMoveRight(struct TextEngine *te);
float cursorGetOffsetWidth(struct TextEngine *te);
void cursorResetBlinkState(struct TextEngine *te);

struct Line* getLine(struct TextEngine *te, struct Line *next, struct Line *prev);
void lineWriteString(struct TextEngine *te, struct Line *line, char *s);
void lineCleanUp(struct Line *line);

struct TextEngine* textEngineInit(char *fileName, int windowWidth, int windowHeight, char *fontFileName, int fontSize, SDL_Color textColor, SDL_Color bgColor);
void textEngineHandleEvents(struct TextEngine *te);
char* textEngineGetResourcePath(char *fileName);
void textEngineReadFile(struct TextEngine *te);
void textEngineWriteFile(struct TextEngine *te);
void textEngineAppendLine(struct TextEngine *te);
void textEngineAppendString(struct TextEngine *te, char *s);
void textEnginePopCharUTF8(struct TextEngine *te);
unsigned int textEnginePackUTF8(unsigned char *buffer, unsigned int *packed);
unsigned int textEngineCountChars(unsigned char *buffer);
void textEngineShiftGapStart(struct TextEngine *te);
void textEngineShiftGapEnd(struct TextEngine *te);
void textEnginePopLine(struct TextEngine *te);
void textEngineRemoveLine(struct TextEngine *te, struct Line *line);
void textEnginePollCursorBlinkTimer(struct TextEngine *te);
void textEngineRenderCursor(struct TextEngine *te);
void textEngineClearLine(struct TextEngine *te, struct Line *line);
void textEngineRenderLine(struct TextEngine *te, struct Line *line);
void textEngineRenderStatusBar(struct TextEngine *te);
void textEngineRenderLines(struct TextEngine *te);
void textEngineClear(struct TextEngine *te);
void textEngineRecalculateLines(struct TextEngine *te);
void textEngineUpdateFrameState(struct TextEngine *te);
void textEngineCleanUp(struct TextEngine *te);

#endif