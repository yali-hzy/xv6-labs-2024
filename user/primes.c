#include "kernel/types.h"
#include "user/user.h"

void primes(int) __attribute__((noreturn));
void primes(int pipeleft) {
  int prime;
  int n;
  int piperight[2];
  if (read(pipeleft, &prime, sizeof(prime)) == 0) {
    exit(0);
  }
  printf("prime %d\n", prime);
  pipe(piperight);
  if (fork() == 0) {
    close(pipeleft);
    close(piperight[1]);
    primes(piperight[0]);
  } else {
    close(piperight[0]);
    while (read(pipeleft, &n, sizeof(n)) != 0) {
      if (n % prime != 0) {
        write(piperight[1], &n, sizeof(n));
      }
    }
    close(pipeleft);
    close(piperight[1]);
    wait(0);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int pipefd[2];
  printf("prime 2\n");
  pipe(pipefd);
  if (fork() == 0) {
    close(pipefd[1]);
    primes(pipefd[0]);
  } else {
    for (int i = 3; i <= 280; i += 2) {
        write(pipefd[1], &i, sizeof(i));
    }
    close(pipefd[1]);
    wait(0);
  }
  exit(0);
}