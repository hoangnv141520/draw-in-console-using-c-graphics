#include <SDL3/SDL.h>

#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "drawvideo 0.1.0\n";
        return 0;
    }

    const bool smokeTest = argc == 2 && std::string_view(argv[1]) == "--smoke-test";

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Unable to initialize SDL: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "drawvideo", 640, 480, smokeTest ? SDL_WINDOW_HIDDEN : 0);
    if (window == nullptr) {
        std::cerr << "Unable to create SDL window: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    bool running = !smokeTest;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
