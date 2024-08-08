#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  int p2c[2],c2p[2]; //a pair of pipes,0 for read and 1 for write
  char buf[20];//buffer
  
  //use pipe to create p2p,c2p
  if (pipe(p2c) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }
  if (pipe(c2p) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }


  int pid = fork();
  if (pid < 0) {
    fprintf(2, "fork failed\n");
    exit(1);
  } else if (pid == 0) { //child process
    //read "ping"
    close(p2c[1]);
    read(p2c[0], buf, 8); 
    printf("%d: received %s", getpid(),buf);
    //write "pong"
    close(c2p[0]);
    write(c2p[1], "pong\n", 5); 
  } else if (pid > 0) { //parent process
    //write "ping"
    close(p2c[0]);
    write(p2c[1], "ping\n", 5); 
    wait(0); 
    //read "pong"
    close(c2p[1]);
    read(c2p[0], buf, 8); 
    printf("%d: received %s", getpid(),buf);
  }

  exit(0);
}
