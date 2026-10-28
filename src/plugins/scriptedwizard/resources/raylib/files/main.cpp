#include "raylib.h"

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "Code::Blocks raylib project");

    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();

        ClearBackground(RAYWHITE);
        DrawText("Hello from raylib!", 290, 180, 24, DARKGRAY);
        DrawCircle(screenWidth / 2, 260, 50, RED);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
