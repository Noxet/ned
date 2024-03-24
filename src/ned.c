
#define _DEFAULT_SOURCE

#include "utils.h"
#include "terminal.h"
#include "astring.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define NED_VERSION "0.1"

#define ESC_KEY '\x1b'
#define CTRL_KEY(k) ((k) & 0x1f)


static bool nedRunning = true;

typedef struct
{
    int size;
    char *string;
} edRow_s;

typedef struct
{
    int winRows;   // window size
    int winCols;
    int cx;     // cursor pos
    int cy;
    edRow_s *row;
    int numRows;
    int rowOffset;
} edConfig_s;


static edConfig_s edConfig;

static FILE *logFile = NULL;
#define LOG(format, ...) fprintf(logFile, format, __VA_ARGS__)

void edMoveCursor(int key)
{
    switch(key)
    {
        case ARROW_UP:
            edConfig.cy--;
            if (edConfig.cy < 0) edConfig.cy = 0;
            break;
        case ARROW_DOWN:
            edConfig.cy++;
            if (edConfig.cy >= edConfig.winRows- 1) edConfig.cy = edConfig.winRows- 1;
            break;
        case ARROW_RIGHT:
            edConfig.cx++;
            if (edConfig.cx >= edConfig.winCols - 1) edConfig.cx = edConfig.winCols - 1;
            break;
        case ARROW_LEFT:
            edConfig.cx--;
            if (edConfig.cx < 0) edConfig.cx = 0;
            break;
        case HOME:
            edConfig.cx = 0;
            break;
        case END:
            edConfig.cx = edConfig.winCols - 1;
            break;
    }
}

void edProcessKey()
{
    int key = termReadKey();

    switch (key)
    {
        case ESC_KEY:
            break;
        case ARROW_UP:
            if (edConfig.rowOffset > 0) edConfig.rowOffset--;
            edMoveCursor(key);
            break;
        case ARROW_DOWN:
            if (edConfig.rowOffset < edConfig.numRows) edConfig.rowOffset++;
            edMoveCursor(key);
            break;
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case HOME:
        case END:
            edMoveCursor(key);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = edConfig.winRows;
                while (times--) edMoveCursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case CTRL_KEY('q'):
            nedRunning = false;
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            break;
        case '\r':
            printf("\r\n");
            break;
        default:
            putchar(key);
            break;
    }
}

void edDrawWelcomeMsg(astring *frame)
{
    if (edConfig.numRows == 0)
    {

        char welcome[128] = { 0 };
        int welcomeLen = snprintf(welcome, sizeof(welcome),
                "ned, the blazingly fast text editor -- version %s", NED_VERSION);
        int padding = (edConfig.winCols - welcomeLen) / 2;

        if (padding)
        {
            astringAppend(frame, "~", 1);
            padding--;
        }

        while (padding--) astringAppend(frame, " ", 1);
        astringAppend(frame, welcome, welcomeLen);
    }
    else
    {
        astringAppend(frame, "~", 1);
    }
}

void edDrawRows(astring *frame)
{
    for (int y = 0; y < edConfig.winRows; y++)
    {
        int off = edConfig.rowOffset;
        if (y < (edConfig.numRows - off))
        {
            // limit text size to the window width
            int len = edConfig.row[y + off].size > edConfig.winCols ? edConfig.winCols : edConfig.row[y + off].size;
            astringAppend(frame, edConfig.row[y + off].string, len);
        }
        else if (y == edConfig.winRows/ 3)
        {
            edDrawWelcomeMsg(frame);
        }
        else
        {
            astringAppend(frame, "~", 1);
        }

        // Erase line by line as we print new text
        astringAppend(frame, DISPLAY_ERASE_LINE_CMD, DISPLAY_ERASE_LINE_LEN);
        if (y < edConfig.winRows- 1)
        {
            // Don't print a newline on the last line, otherwise it's empty (no tilde)
            astringAppend(frame, "\r\n", 2);
        }
    }
}

void edRefreshScreen()
{
    astring *frame = astringNew();

    astringAppend(frame, CURSOR_HIDE_CMD, CURSOR_HIDE_LEN);
    astringAppend(frame, CURSOR_ORIGIN_CMD, CURSOR_ORIGIN_LEN);

    edDrawRows(frame);

    // Set cursor position
    char cursorPos[32];
    // Terminal is 1-indexed, so we need to add 1 to the positions
    int cursorPosLen = snprintf(cursorPos, sizeof(cursorPos), "\x1b[%d;%dH", edConfig.cy + 1, edConfig.cx + 1);
    astringAppend(frame, cursorPos, cursorPosLen);

    astringAppend(frame, CURSOR_SHOW_CMD, CURSOR_SHOW_LEN);

    write(STDOUT_FILENO, astringGetString(frame), astringGetLen(frame));
    
    astringFree(&frame);
}

void edAppendRow(char *line, size_t lineLen)
{
    // TODO(noxet): reallocate row when we hit limit

    line[lineLen] = '\0';

    edConfig.row[edConfig.numRows].size = lineLen;
    // TODO(noxet): free this memory later, in edClose?
    edConfig.row[edConfig.numRows].string = strdup(line);
    edConfig.numRows++;
}

void edOpen(const char *filename)
{
    assert(filename != NULL);

    FILE *inputFile = fopen(filename, "r");
    if (!inputFile) errExit("Failed to open file: %s", filename);

    char *line = NULL;
    size_t bufferSize = 0;
    ssize_t lineLen = 0;
    while ((lineLen = getline(&line, &bufferSize, inputFile)) != -1)
    {
        // TODO(noxet): hack to avoid BO, fix later with reallocations
        if (edConfig.numRows >= 100) break;

        // remove newline char(s) if present and terminate string
        while (lineLen > 0 && (line[lineLen - 1] == '\n' || line[lineLen - 1] == '\r')) lineLen--;
        edAppendRow(line, lineLen);

        LOG("reading string: %s, of length: %zu\n", line, lineLen);
    }

    free(line);
    fclose(inputFile);
}

void edInit()
{
    edConfig.cx = 0;
    edConfig.cy = 0;
    edConfig.row = calloc(100, sizeof(*edConfig.row));
    edConfig.numRows = 0;
    edConfig.rowOffset = 0;

    // TODO(noxet): Handle window resize event
    if (termGetWindowSize(&edConfig.winRows, &edConfig.winCols) == -1) errExit("Failed to get window size");


    printf("window size, rows: %d, cols: %d\n", edConfig.winRows, edConfig.winCols);
}

int main(int argc, char *argv[])
{
    logFile = fopen("ned.log", "w");

    if (termEnableRawMode() == -1) errExit("Failed to set raw mode");
    if (termSetupSignals() == -1) errExit("Failed to set up signal handler");

    edInit();
    if (argc >= 2)
    {
        edOpen(argv[1]);
    }

    // disable stdout buffering
    setbuf(stdout, NULL);

    while (nedRunning)
    {
        edRefreshScreen();
        edProcessKey();
    }

    if (termDisableRawMode() == -1) errExit("Restoring userTerm failed");


    return 0;
}
