#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<ncurses.h>
#include<ctype.h>
#include<signal.h>
#include<errno.h>
#include "libplenja.h"
#include<stdint.h>
#include<locale.h>
#include<libgen.h>
#include<unistd.h>

long displayPos(pstring row, long arrayIndex);
int insertChar(long row, char insertChar, long pos);
int edit();
int *savePrompt();