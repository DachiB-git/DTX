#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>

#define IN_OUT_BUFFER_SIZE 512

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

int main(int argc, char *argv[])
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    char titleBuffer[64];
    char inOutBuffer[IN_OUT_BUFFER_SIZE];

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
    window = SDL_CreateWindow("DTX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
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
    font = TTF_OpenFont("./AovelSansRounded-rdDL.ttf", 24);
    if (font == NULL)
    {
        fprintf(stderr, "Failed to open font: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface *textSurface = NULL;
    SDL_Texture *textTexture = NULL;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    unsigned int frameStart;
    unsigned int frameEnd;
    unsigned int timerStart = SDL_GetTicks();
    int isRunning = 1;
    struct StringBuilder* sb = stringBuilderInit();
    unsigned int prevSize = 0;
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
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                case SDLK_BACKSPACE:
                    stringBuilderPop(sb);
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    stringBuilderAppend(sb, '\n');
                    break;
                default:
                    stringBuilderAppend(sb, event.key.keysym.sym);
                    break;
                }
                break;
            default:
                break;
            }
        }
        // only force rerender if the text buffer gets updated
        if (sb->size != prevSize)
        {
            if (textTexture != NULL)
            {
                SDL_DestroyTexture(textTexture);
                textTexture = NULL;
            }
            prevSize = sb->size;
            SDL_RenderClear(renderer);
            // can't render 0 width textures
            if (sb->size != 0)
            {
                stringBuilderToString(sb, inOutBuffer, IN_OUT_BUFFER_SIZE);
                textSurface = TTF_RenderText_Blended_Wrapped(font, inOutBuffer, textColor, 0);
                textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                SDL_Rect textRect = {100, 100, textSurface->w, textSurface->h};
                SDL_FreeSurface(textSurface);
            
                SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
            }
            SDL_RenderPresent(renderer);
        }
        frameEnd = SDL_GetTicks();
        float frameTime = (frameEnd - frameStart) / 1000.0;
        SDL_Delay(SDL_floor(16.66f - frameTime));
    }
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
    return 0;
}