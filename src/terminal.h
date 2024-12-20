#pragma once

#define CURSOR_ORIGIN_CMD       "\x1b[H"
#define CURSOR_ORIGIN_LEN       3
#define CURSOR_HIDE_CMD         "\x1b[?25l"
#define CURSOR_HIDE_LEN         6
#define CURSOR_SHOW_CMD         "\x1b[?25h"
#define CURSOR_SHOW_LEN         6
#define DISPLAY_ERASE_ALL_CMD   "\x1b[2J"
#define DISPLAY_ERASE_ALL_LEN   4
#define DISPLAY_ERASE_LINE_CMD  "\x1b[K"
#define DISPLAY_ERASE_LINE_LEN  3


#define FG_COLOR_SIZE       9           // a color + reset
#define FG_COLOR_RESET      "\x1b[0m"
#define FG_COLOR_RESET_SIZE 4
#define FG_COLOR_WHITE      "\x1b[30m"
#define FG_COLOR_RED        "\x1b[31m"

typedef enum
{
    TERM_COLOR_NONE,
    TERM_COLOR_WHITE,
    TERM_COLOR_RED,
    TERM_COLOR_RESET,
} termColor_e;

typedef enum
{
    ESC_KEY         = '\x1b',
    ENTER_KEY       = '\x0D',
    LEFT_BRACKET    = '[',
    BACKSPACE       = 127,

    // Choose numbers of out real character range
    ARROW_UP        = 1000,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,

    // Home/PgUp keys
    HOME,
    INSERT,
    DELETE,
    END,
    PAGE_UP,
    PAGE_DOWN,
} termKey_e;

int termSetupSignals();
int termEnableRawMode();
int termDisableRawMode();
int termGetWindowSize(int *rows, int *cols);
termKey_e termReadKey();
char *termGetColor(termColor_e color);
