#ifndef _TERMIOS_H
#define _TERMIOS_H 1

/* TODO: complete this; this just has the minimum for pint */

#include "sys/types.h"

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
};

/* c_iflag */
#define BRKINT 0x0002
#define ICRNL  0x0100
#define INPCK  0x0010
#define ISTRIP 0x0020
#define IXON   0x0400

/* c_oflag */
#define OPOST 0x0001

/* c_cflag */
#define CS8 0x0030

/* c_lflag */
#define ISIG   0x0001
#define ICANON 0x0002
#define ECHO   0x0008
#define IEXTEN 0x8000

/* c_cc indexes */
#define VMIN  6
#define VTIME 5

/* tcsetattr actions */
#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int optional_actions, const struct termios *t);

#endif