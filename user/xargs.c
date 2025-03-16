#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  char c;
  char buf[1024];
  char* args[MAXARG];
  char *p = buf;

  for(int i = 0; i < argc - 1; i++){
    args[i] = argv[i + 1];
  }
  while(read(0, &c, 1) > 0){
    if(c == '\n') {
      if(fork() == 0){
        *p = '\0';
        args[argc - 1] = buf;
        args[argc] = 0;
        exec(argv[1], args);
      } else {
        wait(0);
        p = buf;
      }
    } else {
      *p = c;
      p++;
    }
  }
  exit(0);
}