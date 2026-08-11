#ifndef TEXT_ENGINE_H
#define TEXT_ENGINE_H

#include <stdio.h>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>

#define IN_OUT_BUFFER_SIZE 512
#define isUTF8ContByte(c) ((c & 0xC0) == 0x80) 

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

struct TextEngine
{
    char buffer[IN_OUT_BUFFER_SIZE];
    SDL_Window *window;
    SDL_Renderer *renderer;
    float renderScale;
    TTF_Font *font;
    int fontSize;
    int windowWidth;
    int windowHeight;
    int renderWidth;
    int renderHeight;
    float lineHeight;
    SDL_Color textColor;
    SDL_Color bgColor;
    struct Cursor cursor;
    unsigned int lines;
    unsigned int isRunning;
    unsigned int frameStart;
    unsigned int shouldRerenderLines;
    unsigned int shouldRerenderCursor;
    struct Line* currentLine;
    struct Line* first;
    struct Line* last;
};

struct Node* getNode(char c, struct Node* next, struct Node* prev);

struct Line* getLine(struct Line* next, struct Line* prev);
void lineCleanUp(struct Line* line);

struct StringBuilder* stringBuilderInit();
int stringBuilderAppend(struct StringBuilder* sb, char c);
int stringBuilderAppendString(struct StringBuilder* sb, char *s);
void stringBuilderPop(struct StringBuilder* sb);
void stringBuilderPopUTF8(struct StringBuilder* sb);
void stringBuilderToString(struct StringBuilder* sb, char *buffer, unsigned int size);
void stringBuilderCleanUp(struct StringBuilder *sb);

struct TextEngine* textEngineInit(int windowWidth, int windowHeight, char *fontFileName, int fontSize, SDL_Color textColor, SDL_Color bgColor);
void textEngineHandleEvents(struct TextEngine *te);
void textEngineReadFile(struct TextEngine *te, char *fileName);
void textEngineWriteFile(struct TextEngine *te, char *fileName);
void textEngineAppendChar(struct TextEngine *te, char c);
void textEngineAppendLine(struct TextEngine* te);
void textEngineAppendString(struct TextEngine *te, char *s);
void textEnginePopCharUTF8(struct TextEngine *te);
void textEnginePopLine(struct TextEngine *te);
void textEnginePollCursorBlinkTimer(struct TextEngine *te);
void textEngineRenderCursor(struct TextEngine *te);
void textEngineRenderLine(struct TextEngine *te, struct Line *line);
void textEngineRerenderLines(struct TextEngine *te);
void textEngineCleanUp(struct TextEngine *te);

#endif