#include "termios.h"

int tcgetattr(int fd, struct termios *t) {
    *t = (struct termios){ 0 };
}

int tcsetattr(int fd, int optional_actions, const struct termios *t) {
    return 0;
}