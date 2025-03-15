#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int p[2][2];
  char buf[2][1];

  pipe(p[0]); // parent -> child
  pipe(p[1]); // child -> parent

  if (fork() == 0) {
    close(p[0][1]);
    close(p[1][0]);
    read(p[0][0], buf[0], 1);
    close(p[0][0]);
    printf("%d: received ping\n", getpid());
    write(p[1][1], buf[0], 1);
    close(p[1][1]);
    exit(0);
  } else {
    close(p[0][0]);
    close(p[1][1]);
    write(p[0][1], "a", 1);
    close(p[0][1]);
    read(p[1][0], buf[1], 1);
    close(p[1][0]);
    printf("%d: received pong\n", getpid());
    exit(0);
  }
}