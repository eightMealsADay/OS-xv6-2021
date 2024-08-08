#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  //check param num
  //if unreasonable,give a tip before exit
  if(argc != 2){
    fprintf(2, "correct usage: sleep <milliseconds>\n");
    exit(1);
  }

  int time = atoi(argv[1]); //transfer the param into integer
  printf("nothing happes for a little while\n");
  sleep(time);
  exit(0);
}
