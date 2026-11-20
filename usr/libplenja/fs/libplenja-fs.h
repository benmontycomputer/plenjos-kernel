#ifndef PLENJA_FS_H
#define PLENJA_FS_H
#include "libplenja-str.h"

int writeFile(char *filename, pstring *lines, long lineCount);
pstring* openFile(char *filename);
long countLines(char *filename);

#endif