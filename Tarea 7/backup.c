#include <unistd.h>
#include <fcntl.h>
#include <task.h>
#include <fd.h>

#define PERMS 0666 
#define BUFSIZE 1024
#define STACK 32768

void copytask(void *arg) {
  FILE *f1, *f2;
  char buf[BUFSIZE];
  char backfile[32];
  int n;
  
  if ((f1 = fopen(arg,"r")) == -1) {
    printf("cp: can't open %s\n", arg);taskexit(-1);
  }
  fdnoblock(f1);
  strcpy(backfile,arg);
  strcat(backfile,".old");
  if ((f2 = fopen(backfile, "w")) == -1) {
    printf("cp: can't create %s, mode %03o\n", backfile, PERMS);taskexit(-1);
  }
  fdnoblock(f2);
  while ((n = fdread(f1, buf, BUFSIZ)) > 0)
    if (fdwrite(f2, buf, n) != n)
      printf("cp: write error on file %s\n", backfile);
  fclose(f1);
  fclose(f2);
  taskexit(0);
}

void taskmain(int argc, char **argv) {
  int i;
  if (argc == 0) {
    printf("Usage: backup [files]\n"); exit(-1);
  }
  for (i=0; i < argc; i++) {
    taskcreate(copytask,(void*)argv[i],STACK);
  }
}