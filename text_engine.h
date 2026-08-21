#ifndef TEXT_ENGINE_H
#define TEXT_ENGINE_H

#include <stdio.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#define IN_OUT_BUFFER_SIZE 2048
#define isUTF8ContByte(c) ((c & 0xC0) == 0x80)
#define SDLColorToInt(c) ((c.r << 24) | (c.g << 16) | (c.b << 8) | c.a)

// enum FILE_STATUS_CODE
// {
//     UNMODIFIED = 0,
//     UNSAVED,
//     SAVING,
// };

struct Cursor
{
    struct Node *currentNode;
    float offsetX;
    float offsetY;
    float width;
    unsigned int scrollIsActive;
    unsigned int blinkStart;
    unsigned int blinkState;
    unsigned int blinkIntervalMillis;
};

struct Node
{
    char c;
    struct Node *prev;
    struct Node *next;
};

struct StringBuilder
{
    unsigned int size;
    struct Node *head;
    struct Node *tail;
};

struct Line
{
    float offsetX;
    float offsetY;
    float width;
    float height;
    unsigned int lineNumber;
    unsigned int indentationDepth;
    struct Node *indentationEnd;
    unsigned int shouldRerender;
    struct StringBuilder *sb;
    struct Line *next;
    struct Line *prev;
};

struct TextEngine
{
    char buffer[IN_OUT_BUFFER_SIZE];
    SDL_Window *window;
    SDL_Renderer *renderer;
    float renderScale;
    TTF_Font *font;
    char *fontFilePath;
    int fontSize;
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
    struct Line *currentLine;
    struct Line *first;
    struct Line *last;
    struct Line *frameFirst;
    struct Line *frameLast;
};

struct Node* cursorSeekNextCodePoint(struct TextEngine *te);
struct Node* cursorSeekPrevCodePoint(struct TextEngine *te);
void cursorMoveUp(struct TextEngine *te);
void cursorMoveDown(struct TextEngine *te);
void cursorMoveLeft(struct TextEngine *te);
void cursorMoveRight(struct TextEngine *te);
float cursorGetOffsetWidth(struct TextEngine *te);
void cursorResetBlinkState(struct TextEngine *te);

struct Node* getNode(char c, struct Node* next, struct Node* prev);

struct Line* getLine(struct TextEngine *te, struct Line *next, struct Line *prev);
void lineCleanUp(struct Line *line);

struct StringBuilder* stringBuilderInit();
int stringBuilderAppend(struct StringBuilder *sb, char c);
int stringBuilderAppendString(struct TextEngine *te, struct StringBuilder *sb, char *s);
void stringBuilderPop(struct StringBuilder *sb);
void stringBuilderPopUTF8(struct TextEngine *te, struct StringBuilder *sb);
void stringBuilderToString(struct StringBuilder *sb, char *buffer, unsigned int size);
unsigned int stringBuilderCalculateSize(struct StringBuilder *sb);
void stringBuilderCleanUp(struct StringBuilder *sb);

struct TextEngine* textEngineInit(char *fileName, int windowWidth, int windowHeight, char *fontFileName, int fontSize, SDL_Color textColor, SDL_Color bgColor);
void textEngineHandleEvents(struct TextEngine *te);
char* textEngineGetResourcePath(char *fileName);
void textEngineReadFile(struct TextEngine *te);
void textEngineWriteFile(struct TextEngine *te);
void textEngineAppendChar(struct TextEngine *te, char c);
void textEngineAppendLine(struct TextEngine *te);
void textEngineAppendString(struct TextEngine *te, char *s);
void textEnginePopCharUTF8(struct TextEngine *te);
void textEnginePopLine(struct TextEngine *te);
void textEnginePollCursorBlinkTimer(struct TextEngine *te);
void textEngineRenderCursor(struct TextEngine *te);
void textEngineClearLine(struct TextEngine *te, struct Line *line);
void textEngineRenderLine(struct TextEngine *te, struct Line *line);
void textEngineRenderStatusBar(struct TextEngine *te);
void textEngineRenderLines(struct TextEngine *te);
void textEngineRecalculateLines(struct TextEngine *te);
void textEngineUpdateFrameState(struct TextEngine *te);
void textEngineCleanUp(struct TextEngine *te);

#endif