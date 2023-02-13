#include <iostream>
#include <unistd.h> // for usleep
#include <ncurses.h>
#include <random>
#include <ctime>
#include <cstdlib>

// build command: g++ snake-linux.cpp -lncurses

int width = 20;
int height = 20;
int x, y, fruitX, fruitY, score;
int tailX[100], tailY[100];
int nTail;
enum eDirection { STOP = 0, LEFT, RIGHT, UP, DOWN};
eDirection dir;
bool gameOver;

void Setup()
{
    initscr();
    clear();
    noecho();
    cbreak();
    curs_set(0);

    // seed the random number generator
    std::srand(std::time(nullptr));

    gameOver = false;
    dir = STOP;
    x = width / 2;
    y = height / 2;
    fruitX = rand() % width;
    fruitY = rand() % height;
    score = 0;
}

void Draw()
{
    clear();
    for (int i = 0; i < width + 2; i++)
        mvaddch(0, i, '#');
    for (int i = 0; i < height + 2; i++)
        mvaddch(i, 0, '#');
    for (int i = 0; i < width + 2; i++)
        mvaddch(height + 1, i, '#');
    for (int i = 0; i < height + 2; i++)
        mvaddch(i, width + 1, '#');
    mvaddch(y, x, 'O');
    mvaddch(fruitY, fruitX, '@');
    for (int i = 0; i < nTail; i++)
        mvaddch(tailY[i], tailX[i], 'o');
    mvprintw(height + 3, 0, "Score: %d", score);
    refresh();
}

void Input()
{
    switch (getch())
    {
    case 'a':
        dir = LEFT;
        break;
    case 'd':
        dir = RIGHT;
        break;
    case 'w':
        dir = UP;
        break;
    case 's':
        dir = DOWN;
        break;
    case 'q':
        gameOver = true;
        break;
    }
}

void Logic()
{
    int prevX = tailX[0];
    int prevY = tailY[0];
    int prev2X, prev2Y;
    tailX[0] = x;
    tailY[0] = y;
    for (int i = 1; i < nTail; i++)
    {
        prev2X = tailX[i];
        prev2Y = tailY[i];
        tailX[i] = prevX;
        tailY[i] = prevY;
        prevX = prev2X;
        prevY = prev2Y;
    }
    switch (dir)
    {
    case LEFT:
        x--;
        break;
    case RIGHT:
        x++;
        break;
    case UP:
        y--;
        break;
    case DOWN:
        y++;
        break;
    default:
        break;
    }

    if (x > width || x < 0 || y > height || y < 0)
        gameOver = true;

    for (int i = 0; i < nTail; i++)
        if (tailX[i] == x && tailY[i] == y)
            gameOver = true;

    if (x == fruitX && y == fruitY)
    {
        score += 10;
        fruitX = rand() % width;
        fruitY = rand() % height;
        nTail++;
    }
}

int main()
{
    Setup();
    while (!gameOver)
    {
		timeout(200);
        Draw();
        Input();
        Logic();
    }
    std::cout << "\nGAME OVER" << std::endl;
    return 0;
}
