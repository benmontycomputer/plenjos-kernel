#ifndef PLENJA_FS_H
#define PLENJA_FS_H
#include "libplenja.h"

int writeFile(char *filename, pstring *lines, long lineCount);
pstring* openFile(char *filename);
long countLines(char *filename);

#endif
#ifndef PLENJA_STR_H_
#define PLENJA_STR_H_

typedef struct pstring {
    char *chars;
    long len;
} pstring;

/*struct pstring pstr_empty = {
    "",
    0
};*/

long utf8len(char *s);

pstring pstr_new();
pstring pstr_new_from_chars(char *chars, long len);
pstring pstr_insert_char(pstring pstr, char c, long pos);
pstring pstr_remove_char(pstring pstr, long pos);
pstring pstr_start_at(pstring pstr, long pos);
pstring pstr_first_n(pstring pstr, long pos);

pstring pstr_combine(pstring pstr1, pstring pstr2);
pstring pstr_get_terminated(pstring pstr);

char* insert_char_malloc (char *str, long len, char c, long pos);
char* insert_char_realloc (char *str, long len, char c, long pos);

#endif