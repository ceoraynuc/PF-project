/*
 * SnakeForge - Snakes and Ladders with a board editor (raylib version)
 * Programming Fundamentals Project
 *
 * Graphics, mouse input and sound come from raylib. Everything else is
 * plain C: arrays, loops, functions, if/else, switch and file handling.
 *
 * HOW THE BOARD IS STORED
 *   jump[square] = where that square sends you (0 = normal square)
 *   jump[sq] > sq -> LADDER (it sends you UP)
 *   jump[sq] < sq -> SNAKE  (it sends you DOWN)
 *   Example: jump[2] = 38 means "landing on square 2 climbs a ladder to 38".
 *
 * HOW THE SCREEN WORKS
 *   The whole screen is laid out on a fixed 1090 x 760 canvas. A Camera2D
 *   scales that canvas to fit the window, so the window can be resized,
 *   maximised or made full screen (F11). All our drawing and mouse code
 *   can therefore pretend the window is always 1090 x 760.
 *
 * HOW THE PROGRAM RUNS (see main() at the bottom)
 *   Every frame (60 times per second) we:
 *     1. update  - read the mouse/keyboard and move the game forward a tiny bit
 *     2. draw    - paint the whole screen again from scratch
 *   This is called a "game loop".
 *
 * Build (see README.md):
 *   Windows: gcc main.c -o snakeforge.exe -mwindows -I raylib/include -L raylib/lib -lraylibdll
 *   Linux  : gcc main.c -o snakeforge -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
 */

/* ---------- libraries we use ---------- */
#include "raylib.h"     /* graphics, sound, mouse, keyboard */
#include <stdio.h>      /* files (fopen, fprintf...) and text formatting (sprintf) */
#include <stdlib.h>     /* malloc / free (memory) */
#include <string.h>     /* text helpers (strcpy, strlen...) */
#include <math.h>       /* sinf, cosf, sqrtf, atan2f */
#include <time.h>       /* time(), used to seed the random numbers */

/* ---------- constants ---------- */
/* A #define is a name for a fixed number. Using names instead of raw
   numbers makes code readable: "BOARD_SIZE" is clearer than "100". */
#define BOARD_SIZE 100
#define MAX_PLAYERS 4
#define CELL 70            /* size of one board square in pixels */
#define BX 30              /* x pixel where the board starts (left edge) */
#define BY 30              /* y pixel where the board starts (top edge) */
#define PANEL_X 760        /* x pixel where the right-hand control panel starts */
#define PANEL_W 300        /* width of that panel */
#define SCREEN_W 1090      /* width of our fixed canvas */
#define SCREEN_H 760       /* height of our fixed canvas */
#define FONT_BASE 72       /* fonts are loaded big, then scaled down smoothly */

/* Which screen is the player looking at? (stored in the variable "mode") */
#define MODE_MENU 0
#define MODE_EDITOR 1
#define MODE_GAME 2
#define MODE_RECORDS 3

/* A turn is split into steps called "phases" (stored in "phase").
   The game moves from one phase to the next like a small story. */
#define PH_WAIT 0      /* waiting for a roll */
#define PH_DICE 1      /* dice is rolling */
#define PH_WALK 2      /* token walks square by square */
#define PH_SLIDE 3     /* token slides along a snake or ladder */
#define PH_BOUNCE 4    /* roll was too big, token shakes */
#define PH_WON 5       /* somebody reached 100 */

/* Button looks */
#define STYLE_NORMAL 0
#define STYLE_SELECTED 1
#define STYLE_PRIMARY 2

/* Text alignment */
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

/* ---------- colours ---------- */
/* A Color is {red, green, blue, alpha}. Each part is 0-255.
   Alpha 255 = fully solid, 0 = invisible. */
Color C_BG      = {14, 74, 79, 255};     /* main teal background */
Color C_DEEP    = {9, 55, 59, 255};      /* darker teal (shadows) */
Color C_SAFFRON = {244, 178, 35, 255};   /* yellow board frame */
Color C_MADDER  = {200, 67, 58, 255};    /* red accent */
Color C_PANEL   = {251, 252, 250, 255};  /* panel background */
Color C_INK     = {20, 32, 43, 255};     /* main dark text */
Color C_SOFT    = {74, 90, 102, 255};    /* softer text */
Color C_MUTED   = {112, 128, 138, 255};  /* faint text */
Color C_LINE    = {213, 223, 218, 255};  /* thin border lines */
Color C_HOVER   = {238, 246, 242, 255};  /* button colour when mouse is over it */
Color C_CELLA   = {234, 242, 236, 255};  /* light board square */
Color C_CELLB   = {211, 230, 220, 255};  /* slightly darker board square (checkerboard) */
Color C_GOOD    = {30, 123, 75, 255};    /* green = success */
Color C_BAD     = {179, 38, 30, 255};    /* red = error */

/* One colour per player (player 0 = red, 1 = blue, 2 = yellow, 3 = purple) */
Color playerColor[MAX_PLAYERS] = {
    {200, 16, 46, 255}, {29, 78, 216, 255}, {244, 178, 35, 255}, {109, 40, 217, 255}
};
/* Snakes take their colours from this list, one after another */
Color snakeColor[5] = {
    {47, 107, 58, 255}, {181, 71, 43, 255}, {109, 58, 140, 255},
    {138, 109, 0, 255}, {31, 95, 122, 255}
};
/* Each player's token is shifted a little {x, y} inside the square,
   so 4 tokens on the same square do not sit on top of each other. */
float tokenOffset[MAX_PLAYERS][2] = { {-14, -4}, {14, -4}, {-14, 16}, {14, 16} };

/* ---------- board and rules ---------- */
int jump[BOARD_SIZE + 1];   /* +1 because we use squares 1..100 and ignore index 0 */
int exactFinish = 1;        /* 1 = you must roll the exact number to land on 100 */
int extraTurnOnSix = 1;     /* 1 = rolling a six gives you another turn */

/* ---------- setup (chosen on the menu before the game starts) ---------- */
int setupCount = 2;                         /* how many players */
int setupComputer[MAX_PLAYERS] = {0, 1, 1, 1};   /* 1 = that player is the computer */

/* ---------- game state (changes while playing) ---------- */
int numPlayers = 2;
int position[MAX_PLAYERS];      /* which square each player is standing on */
int throwsCount[MAX_PLAYERS];   /* how many times each player has rolled */
int isComputer[MAX_PLAYERS];    /* 1 = computer controlled */
char names[MAX_PLAYERS][20];    /* "Player 1", "Computer 2" ... */
int current = 0;                /* whose turn it is */
int mover = 0;                  /* who is currently moving (same as current during a turn) */
int phase = PH_WAIT;            /* which step of the turn we are in */
float phaseTimer = 0;           /* seconds spent in the current phase */
float stepTimer = 0;            /* seconds since the token last took a step */
int rollValue = 0;              /* the real dice result (1-6) */
int shownDie = 0;               /* the number drawn on the die (flickers while rolling) */
float dieTick = 0;              /* timer for changing the flickering die number */
/* walkSq   = square the token is stepping from right now
   landedSq = square the token lands on after walking
   finalSq  = where it ends up after any snake/ladder
   jumpKind = 0 none, 1 ladder, 2 snake */
int walkSq = 1, landedSq = 1, finalSq = 1, jumpKind = 0;
int winnerIndex = -1;           /* -1 = nobody has won yet */
char hintText[80] = "";         /* text shown after pressing "Hint" */

/* ---------- screens and messages ---------- */
int mode = MODE_MENU;           /* which screen is shown */
int removeTool = 0;             /* editor: 0 = place tool, 1 = remove tool */
int editFrom = 0;               /* editor: first square clicked (0 = nothing picked yet) */
char message[160] = "Welcome! Pick your players and press Play.";   /* status text at the bottom */
int messageGood = 1;            /* 1 = green message, 0 = red message */
int uiBlocked = 0;              /* 1 = buttons ignore the mouse (used under the winner popup) */
float speed = 1.0f;             /* animation speed multiplier */

/* ---------- analysis (editor) ---------- */
int anaLadders = 0, anaSnakes = 0, anaFastest = 0;
double anaAverage = 0;
char anaLevel[10] = "";         /* "EASY", "MEDIUM" or "HARD" */

/* ---------- records ---------- */
char recordLines[10][100];      /* the last 10 win lines read from records.txt */
int recordCount = 0;

/* ---------- sound ---------- */
int soundOn = 1;                /* player's mute switch */
int soundReady = 0;             /* 1 if the computer has a working audio device */
Sound sndStep, sndRoll, sndLadder, sndSnake, sndWin, sndError;

/* ---------- fonts, window scaling and mouse cursor ---------- */
Font fontBody, fontSemi, fontBold;
Camera2D view;                      /* scales the 1090 x 760 canvas to the window */
int handCursor = 0;                 /* set while the mouse is over something clickable */
int shownCursor = MOUSE_CURSOR_DEFAULT;

/* ====================================================== */
/*                       SOUND                            */
/* ====================================================== */

/* Builds a little tune from a list of note frequencies.
     freqs   - the notes (in Hz; higher number = higher pitch)
     n       - how many notes are in the list
     noteDur - how long each note lasts, in seconds
     square  - 1 = harsh "buzzy" sound, 0 = smooth sound
     vol     - loudness (0.0 to 1.0)
   We make the sound ourselves, so no sound files are needed. */
Sound makeMelody(const float freqs[], int n, float noteDur, int square, float vol)
{
    int rate = 22050;                    /* samples per second (sound quality) */
    int per = (int)(noteDur * rate);     /* samples in ONE note */
    int total = per * n;                 /* samples in the whole tune */
    short *data = (short *)malloc(total * sizeof(short));   /* reserve memory for the samples */
    int i, k;
    float phase = 0, t, env, s;
    Wave w;
    Sound snd;

    for (i = 0; i < n; i++)              /* go through each note */
    {
        for (k = 0; k < per; k++)        /* go through each sample of that note */
        {
            t = (float)k / per;          /* how far through the note (0 to 1) */
            env = 1.0f - t;              /* fades the note out so it does not "click" */
            phase += 2 * PI * freqs[i] / rate;   /* move along the sound wave */
            s = sinf(phase);                     /* smooth wave between -1 and 1 */
            if (square) s = (s > 0) ? 1.0f : -1.0f;   /* turn it into a buzzy square wave */
            data[i * per + k] = (short)(s * env * vol * 32000);   /* store as a 16-bit number */
        }
    }
    /* Pack the samples into a raylib "Wave", then make a playable Sound */
    w.frameCount = total;
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    snd = LoadSoundFromWave(w);
    free(data);                          /* raylib made its own copy, so release ours */
    return snd;
}

/* Creates all the game's sound effects once, at the start */
void loadSounds(void)
{
    /* Each list is the notes of one sound effect */
    float step[]   = {520};
    float roll[]   = {300, 380, 340, 420, 360};
    float ladder[] = {392, 494, 587, 784};
    float snake[]  = {440, 370, 311, 247, 196};
    float win[]    = {523, 659, 784, 1047, 784, 1047};
    float error[]  = {160};

    InitAudioDevice();
    soundReady = IsAudioDeviceReady();
    if (!soundReady) return;             /* no speakers/audio device: skip sounds */
    sndStep   = makeMelody(step, 1, 0.07f, 0, 0.3f);
    sndRoll   = makeMelody(roll, 5, 0.07f, 1, 0.12f);
    sndLadder = makeMelody(ladder, 4, 0.12f, 0, 0.3f);
    sndSnake  = makeMelody(snake, 5, 0.12f, 1, 0.15f);
    sndWin    = makeMelody(win, 6, 0.15f, 0, 0.35f);
    sndError  = makeMelody(error, 1, 0.15f, 1, 0.12f);
}

/* Plays a sound only if audio works AND the player has not muted it */
void play(Sound s)
{
    if (soundReady && soundOn) PlaySound(s);
}

/* ====================================================== */
/*                  BOARD LOGIC (same as C engine)        */
/* ====================================================== */

/* Removes every snake and ladder */
void clearBoard(void)
{
    int i;
    for (i = 0; i <= BOARD_SIZE; i++) jump[i] = 0;
}

/* Sets up the classic board. ladderFrom[i] pairs with ladderTo[i]
   (for example the ladder on square 2 goes up to 38). */
void loadDefaultBoard(void)
{
    int ladderFrom[] = {2, 7, 8, 15, 21, 28, 36, 51, 71, 78, 87};
    int ladderTo[]   = {38, 14, 31, 26, 42, 84, 44, 67, 91, 98, 94};
    int snakeFrom[]  = {16, 46, 49, 62, 64, 74, 89, 92, 95, 99};
    int snakeTo[]    = {6, 25, 11, 19, 60, 53, 68, 88, 75, 80};
    int i;

    clearBoard();
    for (i = 0; i < 11; i++) jump[ladderFrom[i]] = ladderTo[i];   /* 11 ladders */
    for (i = 0; i < 10; i++) jump[snakeFrom[i]] = snakeTo[i];     /* 10 snakes */
}

/* Converts a grid position into a square number (1-100).
   Row 0 is the bottom row. Rows go left-to-right, then right-to-left
   (a zig-zag), exactly like a real Snakes and Ladders board. */
int squareNumber(int row, int col)
{
    if (row % 2 == 0) return row * 10 + col + 1;        /* even row: left to right */
    return row * 10 + (9 - col) + 1;                    /* odd row: right to left */
}

/* Finds the centre of a square in pixels.
   x and y are pointers: we write the answer INTO the caller's variables
   (a function can only "return" one value, so this is how we give back two). */
void squareCenter(int sq, float *x, float *y)
{
    int idx = sq - 1;                                   /* squares start at 1, math is easier from 0 */
    int row = idx / 10;                                 /* which row (0 = bottom) */
    int col = (row % 2 == 0) ? idx % 10 : 9 - (idx % 10);   /* column, flipped on odd rows */
    *x = BX + col * CELL + CELL / 2.0f;
    *y = BY + (9 - row) * CELL + CELL / 2.0f;           /* (9 - row) because screen y grows DOWNWARD */
}

/* Which square is under the mouse? Returns 0 if the mouse is not on the board */
int squareAtMouse(void)
{
    Vector2 m = GetMousePosition();
    int col, rowFromTop;

    /* Outside the board rectangle? Then no square. */
    if (m.x < BX || m.y < BY || m.x >= BX + 10 * CELL || m.y >= BY + 10 * CELL) return 0;
    col = (int)((m.x - BX) / CELL);
    rowFromTop = (int)((m.y - BY) / CELL);
    return squareNumber(9 - rowFromTop, col);           /* our rows count from the bottom */
}

/* BFS (breadth-first search): finds the fewest throws needed to get from
   'start' to every square. The answers go into dist[]. (-1 = unreachable)
   Idea: begin at 'start', then look at every square reachable in 1 throw,
   then every square reachable in 2 throws, and so on. A "queue" (a waiting
   line) keeps the squares in the order they should be checked. */
void bfs(int start, int dist[])
{
    int queue[BOARD_SIZE + 1];
    int head = 0, tail = 0;      /* head = next to check, tail = where to add new ones */
    int i, cur, roll, next;

    for (i = 0; i <= BOARD_SIZE; i++) dist[i] = -1;   /* nothing visited yet */
    dist[start] = 0;                                  /* standing here costs 0 throws */
    queue[tail++] = start;

    while (head < tail)                               /* while the queue is not empty */
    {
        cur = queue[head++];                          /* take the next square from the queue */
        for (roll = 1; roll <= 6; roll++)             /* try every possible dice result */
        {
            next = cur + roll;
            if (next > BOARD_SIZE)                    /* rolled past 100 */
            {
                if (exactFinish) continue;            /* exact rule: this roll is not allowed */
                next = BOARD_SIZE;                    /* otherwise just stop on 100 */
            }
            if (jump[next] != 0) next = jump[next];   /* snake or ladder: follow it */
            if (dist[next] == -1)                     /* first time we reach this square */
            {
                dist[next] = dist[cur] + 1;           /* one more throw than where we came from */
                queue[tail++] = next;
            }
        }
    }
}

/* Fewest throws needed to reach square 100 from 'start' (-1 = impossible) */
int minThrowsFrom(int start)
{
    int dist[BOARD_SIZE + 1];
    bfs(start, dist);
    return dist[BOARD_SIZE];
}

/* Playable = from every square a player can reach, 100 is still reachable.
   This stops the editor from making a board where players get stuck forever. */
int isPlayable(void)
{
    int fromStart[BOARD_SIZE + 1];
    int s;

    bfs(1, fromStart);
    for (s = 1; s <= BOARD_SIZE; s++)
    {
        /* reachable from the start, but can't get from there to 100 -> trapped! */
        if (fromStart[s] != -1 && minThrowsFrom(s) == -1) return 0;
    }
    return fromStart[BOARD_SIZE] != -1;
}

/* Plays 500 pretend games with random dice and returns the average number
   of throws. This gives the "Average game" number and difficulty rating. */
double averageThrows(void)
{
    int games, pos, throws, next;
    long total = 0;

    for (games = 0; games < 500; games++)
    {
        pos = 1;
        throws = 0;
        /* the 5000 limit is a safety net so the loop can never run forever */
        while (pos != BOARD_SIZE && throws < 5000)
        {
            next = pos + GetRandomValue(1, 6);        /* roll the dice */
            throws++;
            if (next > BOARD_SIZE)
            {
                if (exactFinish) continue;            /* too big: lose this throw, stay put */
                next = BOARD_SIZE;
            }
            if (jump[next] != 0) next = jump[next];   /* follow snake/ladder */
            pos = next;
        }
        total += throws;
    }
    return (double)total / 500;       /* (double) so the division keeps decimals */
}

/* Fills in the editor's statistics box */
void computeAnalysis(void)
{
    int i;

    anaLadders = 0;
    anaSnakes = 0;
    for (i = 1; i <= BOARD_SIZE; i++)
    {
        if (jump[i] > i) anaLadders++;             /* goes up = ladder */
        else if (jump[i] != 0) anaSnakes++;        /* goes down = snake */
    }
    anaFastest = minThrowsFrom(1);
    anaAverage = averageThrows();
    if (anaAverage < 30) strcpy(anaLevel, "EASY");
    else if (anaAverage < 45) strcpy(anaLevel, "MEDIUM");
    else strcpy(anaLevel, "HARD");
}

/* Sets the status text at the bottom of the panel.
   good = 1 shows it green, good = 0 shows it red. */
void setMessage(const char text[], int good)
{
    strncpy(message, text, sizeof(message) - 1);   /* copy, but never more than fits */
    message[sizeof(message) - 1] = '\0';           /* make sure the text is properly ended */
    messageGood = good;
}

/* Tries to add a ladder (to > from) or a snake (to < from).
   Returns 1 if it was added, 0 if it was refused (with a message saying why). */
int addJump(int from, int to)
{
    int i;

    /* Rule checks: refuse bad input before changing anything */
    if (from < 2 || from > 99 || to < 1 || to > 99)
    {
        setMessage("Squares must be 2 to 99 (a snake tail may be square 1).", 0);
        return 0;
    }
    if (from == to)
    {
        setMessage("Start and end must be different squares.", 0);
        return 0;
    }
    if (jump[from] != 0)
    {
        setMessage("That square already has a snake or a ladder.", 0);
        return 0;
    }
    if (jump[to] != 0)
    {
        setMessage("The end square is the start of another snake or ladder.", 0);
        return 0;
    }
    for (i = 1; i <= BOARD_SIZE; i++)
    {
        if (jump[i] == from)    /* another jump already ends where this one would start */
        {
            setMessage("The start square is the end of another snake or ladder.", 0);
            return 0;
        }
    }
    /* Try it, then check the board can still be finished. Undo if not. */
    jump[from] = to;
    if (!isPlayable())
    {
        jump[from] = 0;
        setMessage("This would make the board impossible to finish.", 0);
        return 0;
    }
    setMessage(to > from ? "Ladder added." : "Snake added.", 1);
    return 1;
}

/* Removes the snake/ladder that starts OR ends on square 'sq'.
   Returns 1 if removed, 0 if not. */
int removeJumpAt(int sq)
{
    int i, start = 0, old;

    /* Look for a jump that starts on sq, or one that ends on sq.
       'start' becomes the square where that jump begins. */
    for (i = 1; i <= BOARD_SIZE; i++)
    {
        if (i == sq && jump[i] != 0) start = i;
        else if (jump[i] == sq) start = i;
    }
    if (start == 0)
    {
        setMessage("That square has no snake or ladder.", 0);
        return 0;
    }
    old = jump[start];          /* remember it so we can undo */
    jump[start] = 0;
    if (!isPlayable())          /* removing it made the board unfinishable */
    {
        jump[start] = old;      /* put it back */
        setMessage("Cannot remove it: the board would become impossible to finish.", 0);
        return 0;
    }
    setMessage("Removed.", 1);
    return 1;
}

/* ====================================================== */
/*                     FILE HANDLING                      */
/* ====================================================== */

/* Saves the board to board1.txt / board2.txt / board3.txt.
   File format: first line = the two rules, then one "from to" pair per line. */
void saveBoardSlot(int slot)
{
    char name[30];
    FILE *file;
    int i;

    sprintf(name, "board%d.txt", slot);      /* build the file name, e.g. "board2.txt" */
    file = fopen(name, "w");                 /* "w" = write (creates/overwrites the file) */
    if (file == NULL)                        /* NULL means the file could not be opened */
    {
        setMessage("Could not save the file.", 0);
        return;
    }
    fprintf(file, "%d %d\n", exactFinish, extraTurnOnSix);
    for (i = 1; i <= BOARD_SIZE; i++)
    {
        if (jump[i] != 0) fprintf(file, "%d %d\n", i, jump[i]);
    }
    fclose(file);                            /* always close a file when done */
    setMessage(TextFormat("Board saved to slot %d.", slot), 1);
}

/* Loads a board from a slot file. If the file is bad, or the board would
   be unfinishable, the old board is restored (that is what 'backup' is for). */
void loadBoardSlot(int slot)
{
    char name[30];
    FILE *file;
    int backup[BOARD_SIZE + 1];
    int oldExact = exactFinish, oldExtra = extraTurnOnSix;
    int from, to, i, e, x;

    sprintf(name, "board%d.txt", slot);
    file = fopen(name, "r");                 /* "r" = read */
    if (file == NULL)
    {
        setMessage(TextFormat("Slot %d is empty. Save a board there first.", slot), 0);
        return;
    }
    /* fscanf returns how many values it read. We expect 2 (the two rules). */
    if (fscanf(file, "%d %d", &e, &x) != 2)
    {
        fclose(file);
        setMessage("That is not a valid board file.", 0);
        return;
    }
    for (i = 0; i <= BOARD_SIZE; i++) backup[i] = jump[i];   /* save the current board */
    exactFinish = e ? 1 : 0;
    extraTurnOnSix = x ? 1 : 0;
    clearBoard();
    /* keep reading "from to" pairs until the file runs out */
    while (fscanf(file, "%d %d", &from, &to) == 2)
    {
        /* ignore any pair with impossible values */
        if (from >= 2 && from <= 99 && to >= 1 && to <= 99 && from != to) jump[from] = to;
    }
    fclose(file);

    if (!isPlayable())
    {
        /* undo everything: put back the old board and rules */
        for (i = 0; i <= BOARD_SIZE; i++) jump[i] = backup[i];
        exactFinish = oldExact;
        extraTurnOnSix = oldExtra;
        setMessage("That board cannot be finished, so it was not loaded.", 0);
        return;
    }
    setMessage(TextFormat("Board loaded from slot %d.", slot), 1);
}

/* Adds one line to records.txt when somebody wins */
void saveRecord(const char winner[], int throws)
{
    FILE *file = fopen("records.txt", "a");      /* "a" = append (add to the end) */
    if (file == NULL) return;
    fprintf(file, "%s won in %d throws\n", winner, throws);
    fclose(file);
}

/* Reads the last 10 lines of records.txt, newest first */
void loadRecords(void)
{
    FILE *file = fopen("records.txt", "r");
    char all[10][100];       /* holds at most the 10 newest lines */
    char line[100];
    int count = 0, i;

    recordCount = 0;
    if (file == NULL) return;                    /* no file yet = no records */
    while (fgets(line, sizeof(line), file) != NULL)   /* read one line at a time */
    {
        line[strcspn(line, "\r\n")] = '\0';      /* cut off the end-of-line characters */
        if (count == 10)                         /* already full: drop the oldest line */
        {
            for (i = 1; i < 10; i++) strcpy(all[i - 1], all[i]);   /* shift everything up one */
            count--;
        }
        strcpy(all[count], line);
        count++;
    }
    fclose(file);
    /* copy to recordLines in reverse order so the newest comes first */
    for (i = count - 1; i >= 0; i--) strcpy(recordLines[recordCount++], all[i]);
}

/* ====================================================== */
/*                       THE GAME                         */
/* ====================================================== */

/* Resets everything and starts a fresh game using the menu choices */
void startNewGame(void)
{
    int i;

    numPlayers = setupCount;
    for (i = 0; i < numPlayers; i++)
    {
        isComputer[i] = setupComputer[i];
        if (isComputer[i]) sprintf(names[i], "Computer %d", i + 1);
        else sprintf(names[i], "Player %d", i + 1);
        position[i] = 1;                 /* everybody starts on square 1 */
        throwsCount[i] = 0;
    }
    current = 0;
    mover = 0;
    phase = PH_WAIT;
    phaseTimer = 0;
    shownDie = 0;
    winnerIndex = -1;
    hintText[0] = '\0';                  /* an empty string = no hint text */
    mode = MODE_GAME;
    setMessage(TextFormat("%s starts on square 1.", names[0]), 1);
}

/* Called when a roll begins: picks the dice number and starts the dice animation */
void startRoll(void)
{
    mover = current;
    rollValue = GetRandomValue(1, 6);    /* the real result is decided right now */
    throwsCount[mover]++;
    phase = PH_DICE;
    phaseTimer = 0;
    dieTick = 0;
    hintText[0] = '\0';
    play(sndRoll);
}

/* The roll is known: decide how the token will move */
void planMove(void)
{
    int from = position[mover];
    int next = from + rollValue;

    jumpKind = 0;
    if (next > BOARD_SIZE)               /* the roll goes past square 100 */
    {
        if (exactFinish)
        {
            /* not allowed: shake the token and stay where it is */
            phase = PH_BOUNCE;
            phaseTimer = 0;
            play(sndError);
            setMessage(TextFormat("%s rolled %d but needs an exact roll to land on 100.", names[mover], rollValue), 0);
            return;
        }
        next = BOARD_SIZE;               /* otherwise stop on 100 */
    }
    landedSq = next;
    finalSq = next;
    if (jump[next] != 0)                 /* landed on a snake head or ladder bottom */
    {
        jumpKind = jump[next] > next ? 1 : 2;    /* 1 = ladder, 2 = snake */
        finalSq = jump[next];
    }
    walkSq = from;
    stepTimer = 0;
    phase = PH_WALK;
    setMessage(TextFormat("%s rolled a %d.", names[mover], rollValue), 1);
}

/* The animation is finished: the token officially arrives at 'where'.
   Then we check for a win and decide whose turn is next. */
void finishMove(int where)
{
    position[mover] = where;
    if (where == BOARD_SIZE)             /* reached 100: this player wins */
    {
        winnerIndex = mover;
        phase = PH_WON;
        saveRecord(names[mover], throwsCount[mover]);
        play(sndWin);
        setMessage(TextFormat("%s wins in %d throws!", names[mover], throwsCount[mover]), 1);
        return;
    }
    if (rollValue == 6 && extraTurnOnSix)
    {
        /* a six: same player goes again (current is not changed) */
        setMessage(TextFormat("A six! %s rolls again.", names[mover]), 1);
    }
    else
    {
        current = (current + 1) % numPlayers;   /* next player; % wraps back to 0 after the last */
    }
    phase = PH_WAIT;
    phaseTimer = 0;
}

/* Runs once per frame. dt = seconds since the last frame.
   Adding dt to a timer makes animations take the same real time on any computer.
   The switch runs the code for whichever phase we are in. */
void updateGame(float dt)
{
    float t;

    switch (phase)
    {
        case PH_WAIT:
            if (isComputer[current])
            {
                phaseTimer += dt;
                if (phaseTimer >= 0.9f) startRoll();    /* computer "thinks" for 0.9 s, then rolls */
            }
            else if (IsKeyPressed(KEY_SPACE))
            {
                startRoll();                            /* human presses Space to roll */
            }
            break;

        case PH_DICE:
            phaseTimer += dt;
            dieTick += dt;
            if (dieTick >= 0.07f)               /* every 0.07 s show a random face (flicker) */
            {
                dieTick = 0;
                shownDie = GetRandomValue(1, 6);
            }
            if (phaseTimer >= 0.7f)             /* after 0.7 s, show the real result */
            {
                shownDie = rollValue;
                planMove();
            }
            break;

        case PH_WALK:
            stepTimer += dt;
            if (stepTimer >= 0.16f)             /* one step every 0.16 seconds */
            {
                stepTimer = 0;
                walkSq++;                       /* move one square forward */
                play(sndStep);
                if (walkSq == landedSq)         /* walking is done */
                {
                    if (jumpKind != 0)          /* there is a snake or ladder here */
                    {
                        phase = PH_SLIDE;
                        phaseTimer = 0;
                        play(jumpKind == 1 ? sndLadder : sndSnake);
                        if (jumpKind == 1)
                            setMessage(TextFormat("Ladder! %s climbs from %d to %d.", names[mover], landedSq, finalSq), 1);
                        else
                            setMessage(TextFormat("Snake! %s slides from %d to %d.", names[mover], landedSq, finalSq), 0);
                    }
                    else
                    {
                        finishMove(landedSq);
                    }
                }
            }
            break;

        case PH_SLIDE:
            phaseTimer += dt;
            t = phaseTimer / 0.9f;              /* t goes from 0 to 1 over 0.9 seconds */
            if (t >= 1.0f) finishMove(finalSq);
            break;

        case PH_BOUNCE:
            phaseTimer += dt;
            if (phaseTimer >= 0.5f) finishMove(position[mover]);   /* shake for 0.5 s, stay in place */
            break;

        default:                                /* PH_WON: nothing to update */
            break;
    }
}

/* ====================================================== */
/*               FONTS AND WINDOW SCALING                 */
/* ====================================================== */

/* Is this font raylib's built-in fallback font? (it needs slightly different spacing) */
int isDefaultFont(Font f)
{
    return f.texture.id == GetFontDefault().texture.id;
}

/* Uses the first font file in the list that exists.
   If none is found, raylib's built-in font is used instead. */
Font loadFontFrom(const char *files[], int count)
{
    Font f;
    int i;

    for (i = 0; i < count; i++)
    {
        if (!FileExists(files[i])) continue;                /* not on this computer, try the next */
        f = LoadFontEx(files[i], FONT_BASE, NULL, 0);
        if (f.texture.id == 0) continue;                    /* failed to load, try the next */
        GenTextureMipmaps(&f.texture);                      /* smooth when drawn small */
        SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
        return f;
    }
    return GetFontDefault();
}

/* Loads the three font styles. We list a Windows, Linux and macOS font file
   for each, so the program looks good on any of them. */
void loadFonts(void)
{
    /* Windows (Segoe UI), then Linux, then macOS */
    const char *body[] = {"C:/Windows/Fonts/segoeui.ttf",
                          "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                          "/System/Library/Fonts/Supplemental/Arial.ttf"};
    const char *semi[] = {"C:/Windows/Fonts/seguisb.ttf",
                          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                          "/System/Library/Fonts/Supplemental/Arial Bold.ttf"};
    const char *bold[] = {"C:/Windows/Fonts/segoeuib.ttf",
                          "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                          "/System/Library/Fonts/Supplemental/Arial Bold.ttf"};

    fontBody = loadFontFrom(body, 3);
    fontSemi = loadFontFrom(semi, 3);
    fontBold = loadFontFrom(bold, 3);
}

/* Frees the fonts when the program ends (the built-in font must not be freed) */
void unloadFonts(void)
{
    if (!isDefaultFont(fontBody)) UnloadFont(fontBody);
    if (!isDefaultFont(fontSemi)) UnloadFont(fontSemi);
    if (!isDefaultFont(fontBold)) UnloadFont(fontBold);
}

/* Fits the 1090 x 760 canvas into the window and keeps its shape.
   The mouse is scaled the same way, so all the click code still
   works in canvas coordinates. */
void updateView(void)
{
    float sw = (float)GetScreenWidth();      /* current window width */
    float sh = (float)GetScreenHeight();     /* current window height */
    float s;                                 /* zoom factor */

    if (sw < 1 || sh < 1) return;          /* window is minimised */
    /* Pick the smaller of the two possible zooms so the whole canvas fits */
    s = sw / SCREEN_W;
    if (sh / SCREEN_H < s) s = sh / SCREEN_H;

    view.zoom = s;
    view.rotation = 0;
    view.target = (Vector2){0, 0};
    /* offset centres the canvas, leaving equal empty bars on the sides/top */
    view.offset = (Vector2){(float)(int)((sw - SCREEN_W * s) / 2), (float)(int)((sh - SCREEN_H * s) / 2)};
    /* tell raylib to convert real mouse position into canvas position */
    SetMouseOffset(-(int)view.offset.x, -(int)view.offset.y);
    SetMouseScale(1 / s, 1 / s);
}

/* Makes the first window a comfortable size for this monitor */
void fitWindowToMonitor(void)
{
    int mon = GetCurrentMonitor();
    float mw = (float)GetMonitorWidth(mon);
    float mh = (float)GetMonitorHeight(mon);
    Vector2 pos = GetMonitorPosition(mon);
    float s;
    int w, h;

    if (mw <= 0 || mh <= 0) return;
    /* use at most 85% of the monitor width and 80% of its height */
    s = mw * 0.85f / SCREEN_W;
    if (mh * 0.80f / SCREEN_H < s) s = mh * 0.80f / SCREEN_H;
    w = (int)(SCREEN_W * s);
    h = (int)(SCREEN_H * s);
    SetWindowSize(w, h);
    SetWindowPosition((int)pos.x + ((int)mw - w) / 2, (int)pos.y + ((int)mh - h) / 2);   /* centre it */
}

/* Shows a hand cursor over buttons (only changes it when needed) */
void updateCursor(void)
{
    int want = handCursor ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT;

    if (want != shownCursor)
    {
        SetMouseCursor(want);
        shownCursor = want;
    }
}

/* ====================================================== */
/*                   DRAWING HELPERS                      */
/* ====================================================== */

/* raylib's built-in font needs some space between letters, real fonts do not */
float textSpacing(Font f, float size)
{
    return isDefaultFont(f) ? size / 10 : 0;
}

/* Text with its top-left corner at (x, y) */
void drawTextF(Font f, const char text[], float x, float y, float size, Color c)
{
    DrawTextEx(f, text, (Vector2){x, y}, size, textSpacing(f, size), c);
}

/* Width of a piece of text in pixels (useful for centring and wrapping) */
float textWidth(Font f, const char text[], float size)
{
    return MeasureTextEx(f, text, size, textSpacing(f, size)).x;
}

/* One line of text, vertically centred on cy.
   x is the left edge, the middle or the right edge, depending on align. */
void drawLabel(Font f, const char text[], float x, float cy, float size, int align, Color c)
{
    float w = textWidth(f, text, size);
    float y = cy - size * (isDefaultFont(f) ? 0.5f : 0.57f);   /* move up so the middle of the text sits on cy */

    if (align == ALIGN_CENTER) x -= w / 2;    /* shift left by half the width */
    if (align == ALIGN_RIGHT) x -= w;         /* shift left by the whole width */
    drawTextF(f, text, x, y, size, c);
}

/* Small grey capital-letter title above a group of controls */
void drawHeading(const char text[], float x, float cy)
{
    float size = 15;
    float y = cy - size * (isDefaultFont(fontBold) ? 0.5f : 0.57f);
    DrawTextEx(fontBold, text, (Vector2){x, y}, size, isDefaultFont(fontBold) ? 1.5f : 1.2f, C_MUTED);
}

/* Breaks text into lines that fit in maxW and draws them (if draw is 1).
   Returns the y just below the last line, so it can also measure text.
   Method: collect letters into a "word"; at each space, check whether adding
   that word still fits the line. If not, draw the line and start a new one. */
int wrapText(Font f, const char text[], float x, float y, float maxW, float size, Color c, int draw)
{
    char line[200] = "";     /* the line being built */
    char test[300];          /* the line plus the new word, to test its width */
    char word[80];           /* the word being collected */
    int i = 0, w = 0, n = (int)strlen(text);

    for (i = 0; i <= n; i++)        /* <= n so we also handle the final '\0' (end of text) */
    {
        if (text[i] == ' ' || text[i] == '\0')    /* a word just ended */
        {
            word[w] = '\0';
            w = 0;
            if (line[0] == '\0') snprintf(test, sizeof(test), "%s", word);
            else snprintf(test, sizeof(test), "%s %s", line, word);
            if (textWidth(f, test, size) > maxW && line[0] != '\0')
            {
                /* too wide: finish the current line and begin a new one with this word */
                if (draw) drawTextF(f, line, x, y, size, c);
                y += size + 3;
                snprintf(line, sizeof(line), "%s", word);
            }
            else if (strlen(test) < sizeof(line))
            {
                strcpy(line, test);               /* it fits: keep it on this line */
            }
        }
        else if (w < 78)
        {
            word[w++] = text[i];                  /* add this letter to the current word */
        }
    }
    if (draw) drawTextF(f, line, x, y, size, c);  /* draw the last line */
    return (int)(y + size + 3);
}

/* Rounded rectangle with the corner radius given in pixels */
void roundRect(float x, float y, float w, float h, float radius, Color c)
{
    float shortSide = (w < h) ? w : h;
    float roundness = 0;

    /* raylib wants "roundness" as a 0-1 fraction, not pixels, so convert */
    if (shortSide > 0) roundness = 2 * radius / shortSide;
    if (roundness > 1) roundness = 1;
    DrawRectangleRounded((Rectangle){x, y, w, h}, roundness, 12, c);
}

/* Rounded rectangle with a border of the given thickness
   (a big border-coloured box with a slightly smaller filled box on top) */
void roundBox(float x, float y, float w, float h, float radius, Color fill, Color border, float thick)
{
    roundRect(x, y, w, h, radius, border);
    roundRect(x + thick, y + thick, w - 2 * thick, h - 2 * thick, radius - thick, fill);
}

/* A player's round token with their number on it.
   x, y = centre, i = player number (0-3), r = radius */
void drawToken(float x, float y, int i, float r)
{
    DrawCircleV((Vector2){x, y + 2}, r, (Color){0, 0, 0, 60});      /* soft shadow */
    DrawCircleV((Vector2){x, y}, r, WHITE);                         /* white rim */
    DrawCircleV((Vector2){x, y}, r - 2.5f, playerColor[i]);         /* coloured centre */
    /* player 3 (index 2) is yellow, so its number is dark to stay readable */
    drawLabel(fontBold, TextFormat("%d", i + 1), x, y, r * 1.35f, ALIGN_CENTER, i == 2 ? C_INK : WHITE);
}

/* Immediate-mode button: draws it and returns 1 if it was clicked.
   "Immediate mode" means there is no button object; each frame we draw the
   button AND check the mouse in the same function call.
     style   - STYLE_NORMAL / STYLE_SELECTED / STYLE_PRIMARY (the look)
     enabled - 0 = greyed out and not clickable */
int uiButton(float x, float y, float w, float h, const char label[], int style, int enabled)
{
    Rectangle r = {x, y, w, h};
    /* hover = the mouse is over the button (and the button is usable) */
    int hover = enabled && !uiBlocked && CheckCollisionPointRec(GetMousePosition(), r);
    float dy = (hover && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) ? 2 : 0;   /* pressed look */
    float size = (h >= 46) ? 22 : (h >= 36 ? 19 : 17);       /* taller buttons get bigger text */
    Color fill = WHITE, text = C_INK, border = C_LINE, edge = {196, 209, 203, 255};

    /* pick colours for the chosen style */
    if (style == STYLE_SELECTED) { fill = C_BG; text = WHITE; border = C_BG; edge = C_DEEP; }
    if (style == STYLE_PRIMARY)  { fill = C_MADDER; text = WHITE; border = C_MADDER; edge = (Color){140, 44, 37, 255}; }
    if (hover)
    {
        handCursor = 1;                  /* ask main() to show the hand cursor */
        if (style == STYLE_NORMAL) { fill = C_HOVER; border = C_BG; }
        if (style == STYLE_PRIMARY) { fill = (Color){216, 84, 74, 255}; border = fill; }
    }
    if (!enabled)                        /* greyed out */
    {
        fill = (Color){236, 241, 239, 255};
        text = (Color){150, 162, 162, 255};
        border = fill;
        edge = (Color){0, 0, 0, 0};
    }
    roundRect(x, y + 3, w, h, 10, edge);                 /* darker bottom edge */
    roundBox(x, y + dy, w, h, 10, fill, border, 1.5f);
    drawLabel(fontSemi, label, x + w / 2, y + dy + h / 2, size, ALIGN_CENTER, text);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);   /* 1 only on the frame of the click */
}

/* A row of options where exactly one is selected.
   Returns the option that was clicked, or -1 if none was. */
int uiChoice(float x, float y, float w, float h, const char *labels[], int count, int selected)
{
    float part = w / count;              /* width of one option */
    int i, hover, clicked = -1;
    Rectangle r;

    roundRect(x, y, w, h, 10, (Color){229, 237, 234, 255});   /* background strip */
    for (i = 0; i < count; i++)
    {
        r = (Rectangle){x + i * part + 3, y + 3, part - 6, h - 6};
        /* you can only hover/click options that are not already selected */
        hover = !uiBlocked && i != selected && CheckCollisionPointRec(GetMousePosition(), r);
        if (i == selected)
        {
            roundRect(r.x, r.y + 2, r.width, r.height, 8, C_DEEP);   /* shadow */
            roundRect(r.x, r.y, r.width, r.height, 8, C_BG);         /* selected look */
        }
        else if (hover)
        {
            roundRect(r.x, r.y, r.width, r.height, 8, (Color){247, 251, 249, 255});
            handCursor = 1;
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) clicked = i;
        }
        drawLabel(fontSemi, labels[i], r.x + r.width / 2, r.y + r.height / 2, 18, ALIGN_CENTER,
                  i == selected ? WHITE : C_SOFT);
    }
    return clicked;
}

/* A row with a label on the left and an on/off switch on the right.
   Returns 1 when it is clicked. */
int uiToggle(float x, float y, float w, float h, const char label[], int on)
{
    Rectangle r = {x, y, w, h};
    int hover = !uiBlocked && CheckCollisionPointRec(GetMousePosition(), r);
    float tx = x + w - 58, ty = y + h / 2 - 12;       /* top-left of the little switch */

    if (hover) handCursor = 1;
    roundBox(x, y, w, h, 10, hover ? C_HOVER : WHITE, hover ? C_BG : C_LINE, 1.5f);
    drawLabel(fontBody, label, x + 14, y + h / 2, 19, ALIGN_LEFT, C_INK);
    roundRect(tx, ty, 44, 24, 12, on ? C_GOOD : (Color){190, 201, 198, 255});   /* green when on */
    DrawCircleV((Vector2){on ? tx + 32 : tx + 12, ty + 12}, 9, WHITE);          /* knob: right = on, left = off */
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

/* Square number with a white outline so it stays readable over snakes.
   The outline is made by drawing the white text 8 times, nudged by 1 pixel in
   every direction, then the dark text once on top. */
void drawNumber(int sq, float x, float y)
{
    const char *txt = (sq == 1) ? "Start" : TextFormat("%d", sq);
    int dx, dy;

    for (dx = -1; dx <= 1; dx++)
    {
        for (dy = -1; dy <= 1; dy++)
        {
            if (dx != 0 || dy != 0) drawTextF(fontSemi, txt, x + dx, y + dy, 17, WHITE);
        }
    }
    drawTextF(fontSemi, txt, x, y, 17, (Color){44, 74, 77, 255});
}

/* Draws a ladder as two rails (long lines) with rungs (short lines) between.
   A little vector maths is used: "u" is the direction along the ladder and
   "n" points sideways (at 90 degrees), so we can offset the rails left/right. */
void drawLadder(int from, int to)
{
    float ax, ay, bx, by, dx, dy, len, ux, uy, nx, ny, d, cx, cy;
    int s;

    squareCenter(from, &ax, &ay);        /* bottom of the ladder */
    squareCenter(to, &bx, &by);          /* top of the ladder */
    dx = bx - ax; dy = by - ay;          /* how far it goes in x and y */
    len = sqrtf(dx * dx + dy * dy);      /* Pythagoras: the length of the ladder */
    ux = dx / len; uy = dy / len;        /* direction, with length 1 */
    nx = -uy; ny = ux;                   /* sideways direction (rotated 90 degrees) */

    /* two rails: s = -1 is the left rail, s = +1 is the right rail */
    for (s = -1; s <= 1; s += 2)
    {
        Vector2 p1 = {ax + nx * 9 * s, ay + ny * 9 * s};
        Vector2 p2 = {bx + nx * 9 * s, by + ny * 9 * s};
        DrawLineEx(p1, p2, 9, (Color){255, 255, 255, 220});    /* white outline */
        DrawLineEx(p1, p2, 5, (Color){122, 75, 34, 255});      /* brown wood */
    }
    /* rungs: one every 22 pixels along the ladder */
    for (d = 16; d < len - 8; d += 22)
    {
        cx = ax + ux * d;
        cy = ay + uy * d;
        DrawLineEx((Vector2){cx - nx * 9, cy - ny * 9}, (Vector2){cx + nx * 9, cy + ny * 9}, 4, (Color){185, 122, 63, 255});
    }
}

/* Draws a snake as a wavy line from its head to its tail.
   We take 41 points along the straight line from head to tail, and push each
   point sideways by a sine wave - that gives the "S" shaped body. */
void drawSnake(int head, int tail, Color color)
{
    float ax, ay, bx, by, dx, dy, len, ux, uy, nx, ny, t, amp, off, angle;
    int waves, i, N = 40;                /* N = number of segments */
    Vector2 prev, pt;

    squareCenter(head, &ax, &ay);
    squareCenter(tail, &bx, &by);
    dx = bx - ax; dy = by - ay;
    len = sqrtf(dx * dx + dy * dy);
    ux = dx / len; uy = dy / len;        /* direction head -> tail */
    nx = -uy; ny = ux;                   /* sideways direction */
    waves = (int)(len / 80.0f + 0.5f);   /* longer snakes get more wiggles */
    if (waves < 2) waves = 2;

    /* PASS 1: a thick white outline under the body */
    for (i = 0; i <= N; i++)
    {
        t = (float)i / N;                                        /* 0 at head, 1 at tail */
        amp = 12.0f * (0.35f + 0.65f * sinf(PI * t));            /* wave height: biggest in the middle */
        off = amp * sinf(2 * PI * waves * t);                    /* sideways push at this point */
        pt.x = ax + dx * t + nx * off;                           /* point on the line + sideways push */
        pt.y = ay + dy * t + ny * off;
        if (i > 0)
        {
            DrawLineEx(prev, pt, 17, (Color){255, 255, 255, 220});
        }
        DrawCircleV(pt, 8.5f, (Color){255, 255, 255, 220});      /* circles hide gaps between segments */
        prev = pt;
    }
    /* PASS 2: the same path again, thinner, in the snake's colour */
    for (i = 0; i <= N; i++)
    {
        t = (float)i / N;
        amp = 12.0f * (0.35f + 0.65f * sinf(PI * t));
        off = amp * sinf(2 * PI * waves * t);
        pt.x = ax + dx * t + nx * off;
        pt.y = ay + dy * t + ny * off;
        if (i > 0)
        {
            DrawLineEx(prev, pt, 11, color);
        }
        DrawCircleV(pt, 5.5f, color);
        if (i % 2 == 0) DrawCircleV(pt, 2, (Color){255, 255, 255, 120});   /* little pattern spots */
        prev = pt;
    }

    /* head */
    angle = atan2f(dy, dx) + PI;     /* direction the head faces (away from the tail) */
    DrawCircleV((Vector2){ax, ay}, 14, WHITE);
    DrawCircleV((Vector2){ax, ay}, 12, color);
    {
        float fx = cosf(angle), fy = sinf(angle);    /* "forward" direction of the head */
        float sx = -fy, sy = fx;                     /* "sideways" direction */
        /* tongue */
        DrawLineEx((Vector2){ax + fx * 10, ay + fy * 10}, (Vector2){ax + fx * 20, ay + fy * 20}, 2, C_MADDER);
        /* eyes: white circles, then smaller dark pupils */
        DrawCircleV((Vector2){ax + fx * 4 + sx * 6, ay + fy * 4 + sy * 6}, 3.5f, WHITE);
        DrawCircleV((Vector2){ax + fx * 4 - sx * 6, ay + fy * 4 - sy * 6}, 3.5f, WHITE);
        DrawCircleV((Vector2){ax + fx * 5 + sx * 6, ay + fy * 5 + sy * 6}, 1.6f, C_INK);
        DrawCircleV((Vector2){ax + fx * 5 - sx * 6, ay + fy * 5 - sy * 6}, 1.6f, C_INK);
    }
}

/* Where is player i drawn right now? Handles the walking and sliding animation.
   The answer is written into *x and *y.
   Animations use "interpolation": a = start, b = end, t = progress 0..1,
   and   a + (b - a) * t   gives the point part-way between them. */
void tokenPosition(int i, float *x, float *y)
{
    float x1, y1, x2, y2, t;

    /* only the player who is moving right now is animated */
    if (mode == MODE_GAME && i == mover)
    {
        if (phase == PH_WALK)
        {
            squareCenter(walkSq, &x1, &y1);          /* square we are leaving */
            squareCenter(walkSq + 1, &x2, &y2);      /* square we are stepping to */
            if (walkSq + 1 > BOARD_SIZE) { x2 = x1; y2 = y1; }   /* no square 101: stay put */
            t = stepTimer / 0.16f;                   /* progress of this step */
            if (t > 1) t = 1;
            *x = x1 + (x2 - x1) * t + tokenOffset[i][0];
            *y = y1 + (y2 - y1) * t + tokenOffset[i][1] - 10 * sinf(PI * t);   /* little hop */
            return;
        }
        if (phase == PH_SLIDE)
        {
            squareCenter(landedSq, &x1, &y1);
            squareCenter(finalSq, &x2, &y2);
            t = phaseTimer / 0.9f;
            if (t > 1) t = 1;
            t = t * t * (3 - 2 * t);       /* ease in and out (starts and ends slowly) */
            *x = x1 + (x2 - x1) * t + tokenOffset[i][0];
            *y = y1 + (y2 - y1) * t + tokenOffset[i][1];
            return;
        }
        if (phase == PH_BOUNCE)
        {
            squareCenter(position[i], &x1, &y1);
            *x = x1 + tokenOffset[i][0] + 6 * sinf(phaseTimer * 60);   /* shake left-right */
            *y = y1 + tokenOffset[i][1];
            return;
        }
    }
    /* normal case: just stand still on the player's square */
    squareCenter(position[i], &x1, &y1);
    *x = x1 + tokenOffset[i][0];
    *y = y1 + tokenOffset[i][1];
}

/* Draws the board: frame, squares, snakes, ladders, numbers, highlights, tokens.
   The order matters - things drawn later appear ON TOP of earlier things. */
void drawBoard(void)
{
    int row, col, sq, i, n = 0;
    float cx, cy;
    int hover = (mode == MODE_EDITOR) ? squareAtMouse() : 0;   /* editor only: square under the mouse */

    /* frame */
    roundRect(BX - 12, BY - 6, 10 * CELL + 24, 10 * CELL + 24, 18, C_DEEP);      /* shadow */
    roundRect(BX - 12, BY - 12, 10 * CELL + 24, 10 * CELL + 24, 18, C_SAFFRON);  /* yellow frame */

    /* squares */
    for (row = 0; row < 10; row++)
    {
        for (col = 0; col < 10; col++)
        {
            /* (row + col) % 2 alternates 0,1,0,1... which makes the checkerboard */
            DrawRectangle(BX + col * CELL, BY + (9 - row) * CELL, CELL, CELL, (row + col) % 2 ? C_CELLB : C_CELLA);
        }
    }

    /* snakes and ladders */
    for (sq = 1; sq <= BOARD_SIZE; sq++)
    {
        if (jump[sq] > sq) drawLadder(sq, jump[sq]);
    }
    for (sq = 1; sq <= BOARD_SIZE; sq++)
    {
        /* n counts snakes so each gets the next colour; % 5 wraps back to the first colour */
        if (jump[sq] != 0 && jump[sq] < sq) drawSnake(sq, jump[sq], snakeColor[n++ % 5]);
    }

    /* numbers on top */
    for (sq = 1; sq <= BOARD_SIZE; sq++)
    {
        squareCenter(sq, &cx, &cy);
        drawNumber(sq, cx - CELL / 2 + 6, cy - CELL / 2 + 3);   /* top-left corner of the square */
    }

    /* editor highlights */
    if (mode == MODE_EDITOR)
    {
        if (hover != 0)
        {
            handCursor = 1;
            squareCenter(hover, &cx, &cy);
            DrawRectangleLinesEx((Rectangle){cx - CELL / 2, cy - CELL / 2, CELL, CELL}, 3, C_BG);
        }
        if (editFrom != 0)       /* the first square you clicked gets a red outline */
        {
            squareCenter(editFrom, &cx, &cy);
            DrawRectangleLinesEx((Rectangle){cx - CELL / 2, cy - CELL / 2, CELL, CELL}, 5, C_MADDER);
        }
    }

    /* tokens */
    if (mode == MODE_GAME)
    {
        for (i = 0; i < numPlayers; i++)
        {
            tokenPosition(i, &cx, &cy);
            drawToken(cx, cy, i, 12);
        }
    }
}

/* Draws the die with its white face and black dots ("pips") */
void drawDie(float x, float y, float size, int value)
{
    float pip = size * 0.09f;                      /* dot radius */
    float a = size * 0.25f, b = size * 0.5f, c = size * 0.75f;
    /* The face is a 3x3 grid of possible dot spots, numbered 0..8:
           0 1 2
           3 4 5
           6 7 8
       px/py are the pixel positions of those 9 spots. */
    float px[9] = {a, b, c, a, b, c, a, b, c};
    float py[9] = {a, a, a, b, b, b, c, c, c};
    /* pattern[value][spot] = 1 means "draw a dot at that spot".
       Row 0 is for value 0 (blank die, before the first roll). */
    int pattern[7][9] = {
        {0,0,0,0,0,0,0,0,0},
        {0,0,0,0,1,0,0,0,0},
        {1,0,0,0,0,0,0,0,1},
        {1,0,0,0,1,0,0,0,1},
        {1,0,1,0,0,0,1,0,1},
        {1,0,1,0,1,0,1,0,1},
        {1,0,1,1,0,1,1,0,1}
    };
    int i;

    roundRect(x, y + 6, size, size, 16, C_INK);            /* shadow */
    roundRect(x, y, size, size, 16, C_INK);                /* dark border */
    roundRect(x + 3, y + 3, size - 6, size - 6, 13, WHITE);   /* white face */
    for (i = 0; i < 9; i++)
    {
        if (pattern[value][i]) DrawCircleV((Vector2){x + px[i], y + py[i]}, pip, C_INK);
    }
}

/* ====================================================== */
/*                       THE PANEL                        */
/* ====================================================== */

/* The right-hand panel when the main menu is showing */
void drawMenuPanel(void)
{
    const char *counts[] = {"2 players", "3 players", "4 players"};
    const char *kinds[] = {"Human", "Computer"};
    int i, y, c;

    drawHeading("PLAYERS", PANEL_X, 120);
    c = uiChoice(PANEL_X, 134, PANEL_W, 40, counts, 3, setupCount - 2);
    if (c != -1) setupCount = c + 2;       /* option 0 means 2 players, so add 2 */

    /* one row per possible player */
    for (i = 0; i < MAX_PLAYERS; i++)
    {
        y = 186 + i * 46;                  /* each row is 46 pixels below the previous */
        if (i < setupCount)
        {
            /* this player is in the game: show Human / Computer choice */
            drawToken(PANEL_X + 16, y + 19, i, 14);
            drawLabel(fontSemi, TextFormat("Player %d", i + 1), PANEL_X + 40, y + 19, 19, ALIGN_LEFT, C_INK);
            c = uiChoice(PANEL_X + 128, y, 172, 38, kinds, 2, setupComputer[i]);
            if (c != -1) setupComputer[i] = c;
        }
        else
        {
            /* a faded row for a player who is not in this game */
            DrawCircleV((Vector2){PANEL_X + 16, y + 19}, 14, C_LINE);
            DrawCircleV((Vector2){PANEL_X + 16, y + 19}, 11.5f, C_PANEL);
            drawLabel(fontBold, TextFormat("%d", i + 1), PANEL_X + 16, y + 19, 18, ALIGN_CENTER, C_MUTED);
            drawLabel(fontSemi, TextFormat("Player %d", i + 1), PANEL_X + 40, y + 19, 19, ALIGN_LEFT, C_MUTED);
            if (uiButton(PANEL_X + 128, y + 1, 172, 36, "+ Add player", STYLE_NORMAL, 1)) setupCount = i + 1;
        }
    }

    drawHeading("RULES", PANEL_X, 384);
    if (uiToggle(PANEL_X, 398, PANEL_W, 42, "Exact roll to finish", exactFinish))
    {
        exactFinish = !exactFinish;          /* ! flips 1 to 0 and 0 to 1 */
        if (!isPlayable())                   /* this rule would make the board unfinishable */
        {
            exactFinish = !exactFinish;      /* flip it back */
            setMessage("This board cannot be finished with that rule.", 0);
        }
    }
    if (uiToggle(PANEL_X, 448, PANEL_W, 42, "Roll again after a six", extraTurnOnSix))
        extraTurnOnSix = !extraTurnOnSix;

    if (uiButton(PANEL_X, 508, PANEL_W, 52, "Play game", STYLE_PRIMARY, 1)) startNewGame();
    if (uiButton(PANEL_X, 576, 146, 42, "Board editor", STYLE_NORMAL, 1))
    {
        mode = MODE_EDITOR;
        editFrom = 0;
        computeAnalysis();
        setMessage("Click a start square, then an end square.", 1);
    }
    if (uiButton(PANEL_X + 154, 576, 146, 42, "Win records", STYLE_NORMAL, 1))
    {
        loadRecords();
        mode = MODE_RECORDS;
    }
}

/* One "name ........ value" row of the analysis box */
void drawStatRow(const char label[], const char value[], float cy)
{
    drawLabel(fontBody, label, PANEL_X + 16, cy, 18, ALIGN_LEFT, C_SOFT);
    drawLabel(fontSemi, value, PANEL_X + PANEL_W - 16, cy, 18, ALIGN_RIGHT, C_INK);
}

/* The right-hand panel when the board editor is showing */
void drawEditorPanel(void)
{
    const char *tools[] = {"Place", "Remove"};
    int sq, changed = 0, i, c;        /* changed = 1 if the board was modified this frame */
    Color levelColor;

    wrapText(fontBody, "Click a start square, then an end square. Going up makes a ladder, going down makes a snake.",
             PANEL_X, 108, PANEL_W, 18, C_SOFT, 1);

    c = uiChoice(PANEL_X, 180, PANEL_W, 40, tools, 2, removeTool);
    if (c != -1) { removeTool = c; editFrom = 0; }
    if (uiButton(PANEL_X, 230, 146, 36, "Classic board", STYLE_NORMAL, 1)) { loadDefaultBoard(); editFrom = 0; setMessage("Classic board loaded.", 1); changed = 1; }
    if (uiButton(PANEL_X + 154, 230, 146, 36, "Clear board", STYLE_NORMAL, 1)) { clearBoard(); editFrom = 0; setMessage("Board cleared.", 1); changed = 1; }

    /* clicking squares on the board */
    sq = squareAtMouse();
    if (sq != 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !uiBlocked)
    {
        if (removeTool)
        {
            changed = removeJumpAt(sq) || changed;
            if (!changed) play(sndError);
        }
        else if (editFrom == 0)
        {
            /* first click: remember the start square */
            editFrom = sq;
            setMessage(TextFormat("Starting on %d. Click where it ends (Esc cancels).", sq), 1);
        }
        else if (sq == editFrom)
        {
            /* clicked the same square again: cancel */
            editFrom = 0;
            setMessage("Cancelled.", 1);
        }
        else
        {
            /* second click: try to create the snake/ladder from editFrom to sq */
            if (addJump(editFrom, sq)) changed = 1;
            else play(sndError);
            editFrom = 0;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE) && editFrom != 0)
    {
        editFrom = 0;
        setMessage("Cancelled.", 1);
    }
    if (changed) computeAnalysis();      /* refresh the stats only when the board changed */

    /* analysis box */
    roundBox(PANEL_X, 282, PANEL_W, 136, 12, WHITE, C_LINE, 1.5f);
    drawStatRow("Ladders", TextFormat("%d", anaLadders), 302);
    drawStatRow("Snakes", TextFormat("%d", anaSnakes), 326);
    drawStatRow("Fastest possible win", TextFormat("%d throws", anaFastest), 350);
    drawStatRow("Average game", TextFormat("%.1f throws", anaAverage), 374);
    drawLabel(fontBody, "Difficulty", PANEL_X + 16, 400, 18, ALIGN_LEFT, C_SOFT);
    /* check only the first letter of the level: E(asy), M(edium) or H(ard) */
    if (anaLevel[0] == 'E') levelColor = C_GOOD;
    else if (anaLevel[0] == 'M') levelColor = (Color){168, 107, 0, 255};
    else levelColor = C_BAD;
    roundRect(PANEL_X + PANEL_W - 112, 388, 96, 24, 12, levelColor);
    drawLabel(fontBold, anaLevel, PANEL_X + PANEL_W - 64, 400, 15, ALIGN_CENTER, WHITE);

    /* save / load slots */
    drawHeading("SAVE TO A SLOT", PANEL_X, 436);
    for (i = 1; i <= 3; i++)
    {
        /* buttons are placed 102 pixels apart: (i - 1) gives 0, 1, 2 */
        if (uiButton(PANEL_X + (i - 1) * 102, 448, 96, 36, TextFormat("Save %d", i), STYLE_NORMAL, 1)) saveBoardSlot(i);
    }
    drawHeading("LOAD FROM A SLOT", PANEL_X, 504);
    for (i = 1; i <= 3; i++)
    {
        if (uiButton(PANEL_X + (i - 1) * 102, 516, 96, 36, TextFormat("Load %d", i), STYLE_NORMAL, 1))
        {
            loadBoardSlot(i);
            editFrom = 0;
            computeAnalysis();
        }
    }
    if (uiButton(PANEL_X, 576, PANEL_W, 42, "Back to menu", STYLE_NORMAL, 1)) { mode = MODE_MENU; editFrom = 0; }
}

/* The right-hand panel while a game is being played */
void drawGamePanel(void)
{
    int i, y, active;
    float barW = PANEL_W - 166;      /* width of the progress bar */
    int humanTurn = (phase == PH_WAIT && !isComputer[current]);   /* is it a human's turn to roll? */
    const char *rollLabel;           /* text on the big roll button */

    if (humanTurn) rollLabel = TextFormat("%s, roll", names[current]);
    else if (phase == PH_WON) rollLabel = "Game over";
    else if (isComputer[current] && phase == PH_WAIT) rollLabel = "Computer thinks...";
    else rollLabel = "Rolling...";

    /* one card per player */
    drawHeading("PLAYERS", PANEL_X, 120);
    for (i = 0; i < numPlayers; i++)
    {
        y = 134 + i * 54;
        active = (i == current);     /* the player whose turn it is gets a red border */
        roundBox(PANEL_X, y, PANEL_W, 46, 10, active ? (Color){255, 243, 241, 255} : WHITE,
                 active ? C_MADDER : C_LINE, active ? 2.0f : 1.5f);
        drawToken(PANEL_X + 24, y + 23, i, 14);
        drawLabel(fontSemi, names[i], PANEL_X + 48, y + 15, 18, ALIGN_LEFT, C_INK);
        drawLabel(fontBody, TextFormat("Square %d", position[i]), PANEL_X + 48, y + 33, 15, ALIGN_LEFT, C_SOFT);
        drawLabel(fontBody, TextFormat("%d throws", throwsCount[i]), PANEL_X + PANEL_W - 14, y + 15, 15, ALIGN_RIGHT, C_SOFT);
        /* progress bar: how far along the board this player is */
        roundRect(PANEL_X + 152, y + 30, barW, 7, 3.5f, C_LINE);
        roundRect(PANEL_X + 152, y + 30, barW * position[i] / BOARD_SIZE, 7, 3.5f, playerColor[i]);   /* filled part = position / 100 */
    }

    drawDie(PANEL_X + 4, 360, 92, shownDie);
    if (uiButton(PANEL_X + 116, 360, 184, 54, rollLabel, STYLE_PRIMARY, humanTurn)) startRoll();
    if (uiButton(PANEL_X + 116, 424, 184, 36, "Hint", STYLE_NORMAL, humanTurn))
        sprintf(hintText, "Fastest win from here: %d throws.", minThrowsFrom(position[current]));
    if (hintText[0] != '\0') drawLabel(fontSemi, hintText, PANEL_X, 492, 17, ALIGN_LEFT, C_BG);
    drawLabel(fontBody, "Space: roll     M: mute     F11: full screen", PANEL_X, 524, 15, ALIGN_LEFT, C_MUTED);

    if (uiButton(PANEL_X, 576, PANEL_W, 42, "End game", STYLE_NORMAL, 1)) mode = MODE_MENU;
}

/* The right-hand panel that lists past wins */
void drawRecordsPanel(void)
{
    int i, y;

    drawHeading("LAST 10 WINS", PANEL_X, 120);
    if (recordCount == 0)
    {
        wrapText(fontBody, "No wins yet. Finish a game and it shows up here.", PANEL_X, 140, PANEL_W, 18, C_SOFT, 1);
    }
    for (i = 0; i < recordCount; i++)
    {
        y = 134 + i * 43;
        roundBox(PANEL_X, y, PANEL_W, 37, 8, WHITE, C_LINE, 1.5f);
        roundRect(PANEL_X + 10, y + 9, 4, 19, 2, C_SAFFRON);     /* small yellow marker */
        drawLabel(fontBody, recordLines[i], PANEL_X + 24, y + 18.5f, 17, ALIGN_LEFT, C_INK);
    }
    if (uiButton(PANEL_X, 576, PANEL_W, 42, "Back to menu", STYLE_NORMAL, 1)) mode = MODE_MENU;
}

/* The status message at the bottom of the panel. The box grows with the text. */
void drawMessageBox(void)
{
    float y = 640;
    float h = wrapText(fontSemi, message, 0, 0, PANEL_W - 34, 17, BLANK, 0) + 22;   /* measure only (draw = 0) */
    Color tint = messageGood ? (Color){228, 243, 234, 255} : (Color){251, 233, 231, 255};
    Color ink = messageGood ? C_GOOD : C_BAD;

    roundRect(PANEL_X, y, PANEL_W, h, 12, ink);          /* shows as a coloured left edge */
    roundRect(PANEL_X + 5, y, PANEL_W - 5, h, 12, tint);
    wrapText(fontSemi, message, PANEL_X + 20, y + 11, PANEL_W - 34, 17, ink, 1);
}

/* The "X wins!" popup drawn over the board when the game ends */
void drawWinnerOverlay(void)
{
    float cx = BX + 5 * CELL, cy = BY + 5 * CELL;      /* centre of the board */
    float bw = 420, bh = 270;                          /* popup size */
    float bx = cx - bw / 2, by = cy - bh / 2;          /* popup top-left corner */

    DrawRectangle(BX, BY, 10 * CELL, 10 * CELL, (Color){9, 55, 59, 150});   /* dim the board */
    roundRect(bx, by + 8, bw, bh, 20, C_DEEP);
    roundRect(bx, by, bw, bh, 20, C_PANEL);
    drawToken(cx, by + 48, winnerIndex, 22);
    drawLabel(fontBold, TextFormat("%s wins!", names[winnerIndex]), cx, by + 104, 38, ALIGN_CENTER, C_INK);
    drawLabel(fontBody, TextFormat("Reached square 100 in %d throws.", throwsCount[winnerIndex]), cx, by + 146, 19, ALIGN_CENTER, C_SOFT);
    if (uiButton(bx + 30, by + 186, 170, 52, "Play again", STYLE_PRIMARY, 1)) startNewGame();
    if (uiButton(bx + 220, by + 186, 170, 52, "Menu", STYLE_NORMAL, 1)) mode = MODE_MENU;
}

/* ====================================================== */
/*                         MAIN                           */
/* ====================================================== */

/* Every C program starts running here */
int main(void)
{
    float dt;     /* "delta time": seconds since the previous frame */

    /* ----- one-time setup ----- */
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);   /* smooth edges + resizable window */
    InitWindow(SCREEN_W, SCREEN_H, "SnakeForge - Snakes and Ladders");
    SetWindowMinSize(SCREEN_W / 2, SCREEN_H / 2);
    fitWindowToMonitor();
    SetTargetFPS(60);                                 /* run at 60 frames per second */
    SetRandomSeed((unsigned int)time(NULL));          /* different dice every run */
    SetExitKey(KEY_NULL);                             /* Esc does not close the program */
    loadFonts();
    loadSounds();
    loadDefaultBoard();

    /* ----- the game loop: repeats until the window is closed ----- */
    while (!WindowShouldClose())
    {
        updateView();                                     /* keep the canvas fitted to the window */
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        dt = GetFrameTime() * speed;
        if (dt > 0.05f * speed) dt = 0.05f * speed;   /* avoid big jumps */
        /* while the winner popup is open, buttons behind it must not react */
        uiBlocked = (mode == MODE_GAME && phase == PH_WON);

        if (mode == MODE_GAME) updateGame(dt);
        if (IsKeyPressed(KEY_M)) soundOn = !soundOn;

        /* ----- drawing ----- */
        handCursor = 0;                  /* buttons set this to 1 if the mouse is over them */
        BeginDrawing();
        ClearBackground(C_BG);
        BeginMode2D(view);               /* everything until EndMode2D is scaled to the window */

        drawBoard();

        /* panel background and title */
        roundRect(PANEL_X - 20, BY - 6, PANEL_W + 40, 10 * CELL + 18, 18, C_DEEP);
        roundRect(PANEL_X - 20, BY - 12, PANEL_W + 40, 10 * CELL + 18, 18, C_PANEL);
        drawLabel(fontBold, "SnakeForge", PANEL_X - 1, 54, 40, ALIGN_LEFT, C_MADDER);
        drawLabel(fontBody, "Snakes and Ladders", PANEL_X, 84, 16, ALIGN_LEFT, C_MUTED);
        if (uiButton(PANEL_X + 196, 39, 104, 32, soundOn ? "Sound on" : "Muted", STYLE_NORMAL, 1))
            soundOn = !soundOn;
        DrawRectangle(PANEL_X, 102, PANEL_W, 1, C_LINE);

        /* draw the part of the panel that belongs to the current screen */
        switch (mode)
        {
            case MODE_MENU:    drawMenuPanel(); break;
            case MODE_EDITOR:  drawEditorPanel(); break;
            case MODE_GAME:    drawGamePanel(); break;
            case MODE_RECORDS: drawRecordsPanel(); break;
        }
        drawMessageBox();

        uiBlocked = 0;                   /* the popup's own buttons must work */
        if (mode == MODE_GAME && phase == PH_WON) drawWinnerOverlay();

        EndMode2D();
        EndDrawing();
        updateCursor();
    }

    /* ----- cleanup when the window closes ----- */
    if (soundReady) CloseAudioDevice();
    unloadFonts();
    CloseWindow();
    return 0;
}
