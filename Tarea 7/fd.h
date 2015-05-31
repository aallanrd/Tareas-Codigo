/* Copyright (c) 2005 Russ Cox, MIT; see COPYRIGHT */

#ifndef _FD_H_
#define _FD_H_ 1

#include <stdarg.h>
#include <inttypes.h>

#define EOF       (-1)
#define BUFSIZ    1024
#define OPEN_MAX  20 /* max #files open at once */

typedef struct _iobuf {
    int cnt;          /* characters left */
    char *ptr;        /* next character position */
    char *base;       /* location of buffer */
    int flag;         /* mode of file access */
    int fd;           /* file descriptor */
} FILE;

extern FILE _iob[OPEN_MAX];
#define stdin   (&iob[0])
#define stdout  (&iob[1])
#define stderr  (&iob[2])

enum _flags {
    _READ   = 01,    /* file open for reading */
    _WRITE  = 02,    /* file open for writing */
    _UNBUF  = 04,    /* file is unbuffered */
    _EOF    = 010,   /* EOF has occurred on this file */
    _ERR    = 020    /* error occurred on this file */
};

int _fillbuf(FILE *);
int _flushbuf(int, FILE *);

#define feof(p)     (((p)->flag & _EOF) != 0)
#define ferror(p)   (((p)->flag & _ERR) != 0)
#define fileno(p)   ((p)->fd)

#define getc(p)   (--(p)->cnt >= 0 \
               ? (unsigned char) *(p)->ptr++ : _fillbuf(p))
#define putc(x,p)   (--(p)->cnt >= 0 \
               ? *(p)->ptr++ = (x) : _flushbuf((x), p))

#define getchar()   getc(stdin)
#define putchar(x)  putc(x), stdout)

/*
 * Threaded I/O.
 */

FILE *fopen(const char *filename, const char *mode);
int fdread(FILE*, void *, int);
int fdread1(FILE*, void *, int); /* always uses fdwait */
int fdwrite(FILE*, void *, int);
int fdnoblock(FILE*);

char *fgets(char *s, int n, FILE *iop);
int fputs(char *s, FILE *iop);

void fdtask(void *);

#endif
