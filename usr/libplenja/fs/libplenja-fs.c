#include <stdio.h>
#include "libplenja-fs.h"
#include<stdlib.h>
#include<string.h>
#include<errno.h>

int writeFile(char *filename, pstring *lines, long lineCount)
{
    int errnum;
    FILE *file = fopen(filename, "w");
    if (file)
    {
        long i = 0;
        while (i < lineCount)
        {
            fputs(pstr_get_terminated(lines[i]).chars, file);
            fputs("\n", file);

            i++;
        }

        fclose(file);
        return 0;
    }
    else
    {
        errnum = errno;
        //if (errnum == EAC)
        //return 1;
        return errnum;
    }
}

pstring* openFile(char *filename)
{
    FILE * file = fopen(filename, "r");
    if (file == NULL)
    {
        //printf("no file...");
        return NULL;
    }
    
    char* currentline;
    size_t currentread;
    size_t len = 0;
    
    long linenumber = 0;
    
    pstring * out = malloc(countLines(filename) * sizeof(pstring));
    //printf(" %d ", countlines(filename1));
    
    while ((currentread = getline(&currentline, &len, file)) != -1)
    {
        currentline[strlen(currentline) - 1] = '\0';
        //insertrow(linenumber, currentline);
        out[linenumber].chars = malloc(strlen(currentline));
        strncpy(out[linenumber].chars, currentline, strlen(currentline));
        out[linenumber].len = strlen(currentline);
        //free(currentline2);
        linenumber = linenumber + 1;
        //numberOfRows++;
    }
    fclose(file);
    //free(currentline);

    return out;
}

#define BUF_SIZE 65536

long countLines(char *filename)
{
    FILE *file = fopen(filename,"r");

    if (file == NULL)
    {
        return 0;
    }

    char buf[BUF_SIZE];
    int counter = 0;
    for(;;)
    {
        size_t res = fread(buf, 1, BUF_SIZE, file);
        if (ferror(file))
            return -1;

        int i;
        for(i = 0; i < res; i++)
            if (buf[i] == '\n')
                counter++;

        if (feof(file))
            break;
    }

    return counter;
}
/*
long countLines(char *filename)
{
    // count the number of lines in the file called filename                                    
    FILE *fp = fopen(filename,"r");
    int ch=0;
    long lines=0;

    if (fp == NULL)
    {
        return 0;
    }

    lines++;
    while(!feof(fp))
    {
        ch = fgetc(fp);
        if(ch == '\n')
        {
            lines++;
        }
    }
    fclose(fp);
    
    return lines;
}*/