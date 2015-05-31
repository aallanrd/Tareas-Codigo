#include "taskimpl.h"
#include <sys/poll.h>
#include <fcntl.h>
#include "fd.h"

enum { MAXFD = 1024 };

static struct pollfd pollfd[MAXFD];
static Task *polltask[MAXFD];
static int npollfd;
static int startedfdtask;
static Tasklist sleeping;
static int sleepingcounted;
static uvlong nsec(void);

#define PERMS 0666 /* RW for owner, group, others*/

FILE _iob[OPEN_MAX] = {/* stdin, stdout, stderr: */
                       {0, (char *)0, (char *)0, _READ, 0},
                       {0, (char *)0, (char *)0, _WRITE, 1},
                       {0, (char *)0, (char *)0, _WRITE | _UNBUF, 2}};

void fdtask(void *v) {
  int i, ms;
  Task *t;
  uvlong now;

  tasksystem();
  taskname("fdtask");
  for (;;) {
    /* let everyone else run */
    while (taskyield() > 0)
      ;
    /* we're the only one runnable - poll for i/o */
    errno = 0;
    taskstate("poll");
    if ((t = sleeping.head) == nil)
      ms = -1;
    else {
      /* sleep at most 5s */
      now = nsec();
      if (now >= t->alarmtime)
        ms = 0;
      else if (now + 5 * 1000 * 1000 * 1000LL >= t->alarmtime)
        ms = (t->alarmtime - now) / 1000000;
      else
        ms = 5000;
    }
    if (poll(pollfd, npollfd, ms) < 0) {
      if (errno == EINTR)
        continue;
      fprint(2, "poll: %s\n", strerror(errno));
      taskexitall(0);
    }

    /* wake up the guys who deserve it */
    for (i = 0; i < npollfd; i++) {
      while (i < npollfd && pollfd[i].revents) {
        taskready(polltask[i]);
        --npollfd;
        pollfd[i] = pollfd[npollfd];
        polltask[i] = polltask[npollfd];
      }
    }

    now = nsec();
    while ((t = sleeping.head) && now >= t->alarmtime) {
      deltask(&sleeping, t);
      if (!t->system && --sleepingcounted == 0)
        taskcount--;
      taskready(t);
    }
  }
}

uint taskdelay(uint ms) {
  uvlong when, now;
  Task *t;

  if (!startedfdtask) {
    startedfdtask = 1;
    taskcreate(fdtask, 0, 32768);
  }

  now = nsec();
  when = now + (uvlong)ms * 1000000;
  for (t = sleeping.head; t != nil && t->alarmtime < when; t = t->next)
    ;

  if (t) {
    taskrunning->prev = t->prev;
    taskrunning->next = t;
  } else {
    taskrunning->prev = sleeping.tail;
    taskrunning->next = nil;
  }

  t = taskrunning;
  t->alarmtime = when;
  if (t->prev)
    t->prev->next = t;
  else
    sleeping.head = t;
  if (t->next)
    t->next->prev = t;
  else
    sleeping.tail = t;

  if (!t->system && sleepingcounted++ == 0)
    taskcount++;
  taskswitch();

  return (nsec() - now) / 1000000;
}

void fdwait(int fd, int rw) {
  int bits;

  if (!startedfdtask) {
    startedfdtask = 1;
    taskcreate(fdtask, 0, 32768);
  }

  if (npollfd >= MAXFD) {
    fprint(2, "too many poll file descriptors\n");
    abort();
  }

  taskstate("fdwait for %s",
            rw == 'r' ? "read" : rw == 'w' ? "write" : "error");
  bits = 0;
  switch (rw) {
  case 'r':
    bits |= POLLIN;
    break;
  case 'w':
    bits |= POLLOUT;
    break;
  }

  polltask[npollfd] = taskrunning;
  pollfd[npollfd].fd = fd;
  pollfd[npollfd].events = bits;
  pollfd[npollfd].revents = 0;
  npollfd++;
  taskswitch();
}

/* Like fdread but always calls fdwait before reading. */
int fdread1(FILE *tfile, void *buf, int n) {
  int m;

  do
    fdwait(tfile->fd, 'r');
  while ((m = read(tfile->fd, buf, n)) < 0 && errno == EAGAIN);
  return m;
}

int fdread(FILE *tfile, void *buf, int n) {
  int m;

  while ((m = read(tfile->fd, buf, n)) < 0 && errno == EAGAIN)
    fdwait(tfile->fd, 'r');
  return m;
}

int fdwrite(FILE *tfile, void *buf, int n) {
  int m, tot;

  for (tot = 0; tot < n; tot += m) {
    while ((m = write(tfile->fd, (char *)buf + tot, n - tot)) < 0 &&
           errno == EAGAIN)
      fdwait(tfile->fd, 'w');
    if (m < 0)
      return m;
    if (m == 0)
      break;
  }
  return tot;
}

int fdnoblock(FILE *tfile) {
  return fcntl(tfile->fd, F_SETFL, fcntl(tfile->fd, F_GETFL) | O_NONBLOCK);
}

FILE *fopen(const char *name, const char *mode) {
  int fd;
  FILE *fp;
  if (*mode != 'r' && *mode != 'w' && *mode != 'a')
    return NULL;
  for (fp = _iob; fp < _iob + OPEN_MAX; fp++)
    if ((fp->flag & (_READ | _WRITE)) == 0)
      break;                 /* found free slot */
  if (fp >= _iob + OPEN_MAX) /* no free slots */
    return NULL;
  if (*mode == 'w')
    fd = creat(name, PERMS);
  else if (*mode == 'a') {
    if ((fd = open(name, O_WRONLY, 0)) == -1)
      fd = creat(name, PERMS);
    lseek(fd, 0L, 2);
  } else
    fd = open(name, O_RDONLY, 0);
  if (fd == -1) /* couldn't access name */
    return NULL;
  fp->fd = fd;
  fp->cnt = 0;
  fp->base = NULL;
  fp->flag = (*mode == 'r') ? _READ : _WRITE;
  return fp;
}

/* Solution by Gregory Pietsch */
int fseek(FILE *fp, long offset, int whence) {
  if ((fp->flag & _UNBUF) == 0 && fp->base != NULL) {
    /* deal with buffering */
    if (fp->flag & _WRITE) {
      /* writing, so flush buffer */
      fflush(fp); /* from 8-3 */
    } else if (fp->flag & _READ) {
      /* reading, so trash buffer */
      fp->cnt = 0;
      fp->ptr = fp->base;
    }
  }
  return (lseek(fp->fd, offset, whence) < 0);
}

int _fillbuf(FILE *fp) {
  int bufsize;
  if ((fp->flag & (_READ | _EOF | _ERR)) != _READ)
    return EOF;
  bufsize = (fp->flag & _UNBUF) ? 1 : BUFSIZ;
  if (fp->base == NULL) /* no buffer yet */
    if ((fp->base = (char *)malloc(bufsize)) == NULL)
      return EOF; /* can't get buffer */
  fp->ptr = fp->base;
  fp->cnt = read(fp->fd, fp->ptr, bufsize);
  if (--fp->cnt < 0) {
    if (fp->cnt == -1)
      fp->flag |= _EOF;
    else
      fp->flag |= _ERR;
    fp->cnt = 0;
    return EOF;
  }
  return (unsigned char)*fp->ptr++;
}

/* Solution by Gregory Pietsch */
int _flushbuf(int c, FILE *f) {
  int num_written, bufsize;
  unsigned char uc = c;

  if ((f->flag & (_WRITE | _EOF | _ERR)) != _WRITE)
    return EOF;
  if (f->base == NULL && ((f->flag & _UNBUF) == 0)) {
    /* no buffer yet */
    if ((f->base = malloc(BUFSIZ)) == NULL)
      /* couldn't allocate a buffer, so try unbuffered */
      f->flag |= _UNBUF;
    else {
      f->ptr = f->base;
      f->cnt = BUFSIZ - 1;
    }
  }
  if (f->flag & _UNBUF) {
    /* unbuffered write */
    f->ptr = f->base = NULL;
    f->cnt = 0;
    if (c == EOF)
      return EOF;
    num_written = write(f->fd, &uc, 1);
    bufsize = 1;
  } else {
    /* buffered write */
    if (c != EOF) {
      f->ptr = uc;
	  f->ptr++;
    }
    bufsize = (int)(f->ptr - f->base);
    num_written = write(f->fd, f->base, bufsize);
    f->ptr = f->base;
    f->cnt = BUFSIZ - 1;
  }
  if (num_written == bufsize)
    return c;
  else {
    f->flag |= _ERR;
    return EOF;
  }
}

/* Solution by Gregory Pietsch */
int fflush(FILE *fp) {
  int retval;
  int i;

  retval = 0;
  if (fp == NULL) {
    /* flush all output streams */
    for (i = 0; i < OPEN_MAX; i++) {
      if ((_iob[i].flag & _WRITE) && (fflush(&_iob[i]) == -1))
        retval = -1;
    }
  } else {
    if ((fp->flag & _WRITE) == 0)
      return -1;
    _flushbuf(EOF, fp);
    if (fp->flag & _ERR)
      retval = -1;
  }
  return retval;
}

/* Solution by Gregory Pietsch */
int fclose(FILE *f) {
  int fd;

  if (f == NULL)
    return -1;
  fd = f->fd;
  fflush(f);
  f->cnt = 0;
  f->ptr = NULL;
  if (f->base != NULL)
    free(f->base);
  f->base = NULL;
  f->flag = 0;
  f->fd = -1;
  return close(fd);
}

/* fgets: get at most n chars from iop */
char *fgets(char *s, int n, FILE *iop) {
  register int c;
  register char *cs;
  cs = s;
  while (--n > 0 && (c = getc(iop)) != EOF)
    if ((*cs++ = c) == '\n')
      break;
  *cs = '\0';
  return (c == EOF && cs == s) ? NULL : s;
}

/* fputs: put string s on file iop */
int fputs(char *s, FILE *iop) {
  int c;
  while (c = *s++)
    putc(c, iop);
  return ferror(iop) ? EOF : 0;
}

static uvlong nsec(void) {
  struct timeval tv;

  if (gettimeofday(&tv, 0) < 0)
    return -1;
  return (uvlong)tv.tv_sec * 1000 * 1000 * 1000 + tv.tv_usec * 1000;
}
