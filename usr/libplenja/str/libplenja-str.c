#include "libplenja-str.h"
#include <stdio.h>
#include<stdlib.h>
#include<string.h>

long utf8len(char *s)
{
    size_t len = 0;
    for (; *s; ++s) if ((*s & 0xC0) != 0x80) ++len;
    return len;
}

pstring pstr_new()
{
    pstring returnvalue = {malloc(0), 0};
    returnvalue.chars = malloc(0);
    return returnvalue;
}

pstring pstr_new_from_chars(char *chars, long len)
{
    //pstring returnvalue;
    //returnvalue.chars = malloc(len);
    pstring returnvalue = {malloc(len), len};
    strncpy(returnvalue.chars, chars, len);
    return returnvalue;
}

pstring pstr_start_at(pstring pstr, long pos)
{
    if (pstr.len - pos <= 0)
    {
        return (pstring){NULL, 0};
    }
    pstring returnvalue = {malloc(pstr.len - pos), pos};
    //returnvalue.chars = malloc(pstr.len - pos);
    strncpy(returnvalue.chars, pstr.chars+pos, pstr.len - pos);
    returnvalue.len = pstr.len - pos;
    return returnvalue;
}

pstring pstr_first_n(pstring pstr, long pos)
{
    pstring returnvalue = {malloc(pos), pos};
    //returnvalue.chars = malloc(pos);
    strncpy(returnvalue.chars, pstr.chars, pos);
    returnvalue.len = pos;
    return returnvalue;
}

pstring pstr_combine(pstring pstr1, pstring pstr2)
{
    if (pstr1.len == 0 && pstr2.len == 0)
    {
        return pstr_new_from_chars("", 0);
    }
    pstring returnvalue = {malloc((pstr1.len + pstr2.len) * sizeof(char)), pstr1.len + pstr2.len};
    //returnvalue.chars = malloc(pstr1.len + pstr2.len);
    if (pstr1.len > 0)
    {
        strncpy(returnvalue.chars, pstr1.chars, pstr1.len);
        strncat(returnvalue.chars, pstr2.chars, pstr2.len);
    }
    else
    {
        strncpy(returnvalue.chars, pstr2.chars, pstr2.len);
    }
    returnvalue.len = pstr1.len + pstr2.len;
    return returnvalue;
}

pstring pstr_get_terminated(pstring pstr)
{
    pstring returnvalue;
    //pstring returnvalue = {(char*)malloc(pstr.len + 1), pstr.len + 1};
    returnvalue.chars = malloc(pstr.len + 1);
    strncpy(returnvalue.chars, pstr.chars, pstr.len);
    //returnvalue.chars = realloc(pstr.chars, pstr.len + 1);
    returnvalue.chars[pstr.len] = '\0';
    returnvalue.len = pstr.len + 1;
    return returnvalue;
}

pstring pstr_insert_char(pstring pstr, char c, long pos)
{
    pstring tmp = pstr;
    if (tmp.len > 0)
    {
        tmp.chars = insert_char_malloc(tmp.chars, tmp.len, c, pos);
        tmp.len++;
    }
    else
    {
        char *tmp2 = malloc(1);
        tmp2[0] = c;
        tmp = pstr_new_from_chars(tmp2, 1);
    }
    return tmp;
}

pstring pstr_remove_char(pstring pstr, long pos)
{
    //pstring tmp = pstr;
    pstring tmp;
    tmp.chars = malloc(pstr.len);
    tmp.len = pstr.len;
    strncpy(tmp.chars, pstr.chars, pstr.len);
    //long i = tmp.len;
    long i = pos + 2;
    while (i - 1 < tmp.len)//(i - 2 > pos)
    {
        //memmove(tmp.chars[i - 2], tmp.chars[i - 1], sizeof(char));
        tmp.chars[i - 2] = tmp.chars[i - 1];

        i++;
    }

    tmp.chars = realloc(tmp.chars, (tmp.len - 1) * sizeof(char));

    tmp.len--;
    return tmp;
}

char* insert_char_realloc (char *str, long len, char c, long pos)
{
    long i;

    str = realloc(str, len + 2);
    for (i = len + 1 ; i > pos ; --i)
    str[i] = str[i - 1];
    str[i] = c;
    return str;
}

char* insert_char_malloc (char *str, long len, char c, long pos)
{
    char *p;
    long i;

    p = malloc(len + 2);
    for (i = 0 ; i < pos ; ++i)
    p[i] = str[i];
    p[i++] = c;
    for ( ; i < len + 2 ; ++i)
    p[i] = str[i - 1];
    //free(str);
    return p;
}