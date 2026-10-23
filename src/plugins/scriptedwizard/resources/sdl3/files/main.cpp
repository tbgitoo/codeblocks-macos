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

    const int rgb[] = { 203, 203, 203, // Gray
                        254, 254,  31, // Yellow
                          0, 255, 255, // Cyan
                          0, 254,  30, // Green
                        255,  16, 253, // Magenta
                        253,   3,   2, // Red
                         18,  14, 252, // Blue
                          0,   0,   0  // Black
                      };

    bool running = true;
    while (running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
                running = false;
        }

        int width = 640;
        int height = 480;
        SDL_GetWindowSize(window, &width, &height);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_FRect colorBar;
        colorBar.x = 0.0f;
        colorBar.y = 0.0f;
        colorBar.w = static_cast<float>(width) / 8.0f;
        colorBar.h = static_cast<float>(height);

        for (int i = 0; i != sizeof rgb / sizeof *rgb; i += 3, colorBar.x += colorBar.w)
        {
            SDL_SetRenderDrawColor(renderer, rgb[i], rgb[i + 1], rgb[i + 2], 255);
            SDL_RenderFillRect(renderer, &colorBar);
        }
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
