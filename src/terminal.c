#include "terminal.h"
#include "utils.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>

static termKey_e termParseBracketKeys();
static termKey_e termParseXtermKeys();

static struct termios userTerm;


static void sigHandler(int sig)
{
    UNUSED(sig);

    printf("GOT SIG: %d\n", sig);

    // restore starting terminal state
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &userTerm) == -1)
    {
        printf("[%s] - Failed to set tcsetattr\r\n", __func__);
    }

    _exit(EXIT_SUCCESS);
}

int termSetupSignals()
{
    struct sigaction sa = { 0 };
    sa.sa_flags = SA_NODEFER;
    sa.sa_handler = sigHandler;
    if (sigaction(SIGTERM, &sa, NULL) == -1) return -1;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) return -1;
    return 0;
}

void setEcho(bool state)
{
    struct termios tp;

    tcgetattr(STDIN_FILENO, &tp);
    
    if (state == true)
    {
        tp.c_lflag |= ECHO;
    }
    else
    {
        tp.c_lflag &= ~ECHO;
    }

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);
}

void toggleEcho()
{
    struct termios tp;

    tcgetattr(STDIN_FILENO, &tp);
    
    tp.c_lflag ^= ECHO;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tp);

}

int termEnableRawMode()
{
    struct termios term;

    if (tcgetattr(STDIN_FILENO, &term) == -1) return -1;

    // copy the startup state of the terminal so we can restore later
    userTerm = term;

    term.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    term.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | 
            INPCK | ISTRIP | IXON | PARMRK);
    term.c_oflag &= ~OPOST;

    term.c_cc[VMIN] = 0;    // make it a pure timed read
    term.c_cc[VTIME] = 1;   // set read timeout to 100 ms

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == -1) return -1;

    return 0;
}

int termDisableRawMode()
{
    // restore the saved config
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &userTerm) == -1) return -1;
    return 0;
}

int termGetWindowSize(int *rows, int *cols)
{
    // TODO(noxet): Implement a getCursorPos function by using escape sequence for systems that don't
    // implement the ioctl
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    }

    return 0;
}

termKey_e termReadKey()
{
    char ch = '\0';
    ssize_t nread;
    while ((nread = read(STDIN_FILENO, &ch, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN) errExit("Failed to read input key");
    }

    if (ch == ESC_KEY)
    {
        // If read times out or failes, assume user only pressed ESC key and return that
        char controlChar;
        if (read(STDIN_FILENO, &controlChar, 1) != 1) return ESC_KEY;

        if (controlChar == LEFT_BRACKET)
        {
            return termParseBracketKeys();
        }
        else if (controlChar == 'O')
        {
            return termParseXtermKeys();
        }
    }

    return ch;
}

char *termGetColor(termColor_e color)
{
    switch(color)
    {
        case TERM_COLOR_NONE: return "";
        case TERM_COLOR_WHITE: return FG_COLOR_WHITE;
        case TERM_COLOR_RED: return FG_COLOR_RED;
        case TERM_COLOR_RESET: return FG_COLOR_RESET;
    }

    return NULL;
}

static termKey_e termParseBracketKeys()
{
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return ESC_KEY;

    if (seq[0] >= '1' && seq[0] <= '9')
    {
        // For the VT sequence of the Page keys, eg PageUp, the command is <ESC>[5~
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return ESC_KEY;
        if (seq[1] == '~')
        {
            switch(seq[0])
            {
                case '1': // 2 for HOME
                case '7': return HOME;
                case '2': return INSERT;
                case '3': return DELETE;
                case '4': // 2 for END
                case '8': return END;
                case '5': return PAGE_UP;
                case '6': return PAGE_DOWN;
            }
        }
    }
    else
    {
        switch (seq[0])
        {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'F': return END;
            case 'H': return HOME;
            default: return ESC_KEY;

        }
    }

    return ESC_KEY;
}

static termKey_e termParseXtermKeys()
{
    char ch;
    if (read(STDIN_FILENO, &ch, 1) != 1) return ESC_KEY;

    switch (ch)
    {
        case 'F':
            return END;
        case 'H':
            return HOME;
    }

    return ESC_KEY;
}

