#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int, char **)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "Error while initializing SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_CreateWindowAndRenderer("SDL3", 640, 480, 0, &window, &renderer))
    {
        std::cerr << "Error while initializing SDL: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    const int rgb[] = { 203, 203, 203, // Gray
                        254, 254,  31, // Yellow
                          0, 255, 255, // Cyan
                          0, 254,  30, // Green
                        255,  16, 253, // Magenta
                        253,   3,   2, // Red
                         18,  14, 252, // Blue
                          0,   0,   0  // Black
                      };

    SDL_FRect colorBar;
    colorBar.x = 0.0f;
    colorBar.y = 0.0f;
    colorBar.w = 90.0f;
    colorBar.h = 480.0f;

    for (int i = 0; i != sizeof rgb / sizeof *rgb; i += 3, colorBar.x += 90.0f)
    {
        SDL_SetRenderDrawColor(renderer, rgb[i], rgb[i + 1], rgb[i + 2], 255);
        SDL_RenderFillRect(renderer, &colorBar);
        SDL_RenderPresent(renderer);
        SDL_Delay(500);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
