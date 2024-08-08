#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAX_PRIME 35

//child process read from the p2c port
//filter the numbers read from parent
void filter(int p2c) {
    int num1;
    if(read(p2c,&num1,sizeof(int))>0){//read succeed
        printf("prime %d\n",num1);
        //create a new pipe
        int p[2];
        if(pipe(p)<0){
        fprintf(2,"subpipe failed\n");
        exit(1);
        }

        //v=create a new pid
        int pid=fork();
        if(pid<0){
            fprintf(2,"subfork failed\n");
            exit(1);
        }else if(pid==0){//child process
            //read from parent
            close(p[1]);
            filter(p[0]);
        }else if(pid>0){//parent process
            //write for child
            close(p[0]);
            int num2;
            while(read(p2c,&num2,sizeof(int))>0){
                //filtering
               if(num2%num1!=0){//if the number just read can't be divided by num1,write
                   write(p[1],&num2,sizeof(int));
               }
            }
            close(p[1]);
            close(p2c);
            wait(0);
        }
    }
    else{//read failed
        close(p2c);
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    int p[2];
    if(pipe(p)<0){
        fprintf(2,"pipe failed\n");
        exit(1);
    }

    int pid=fork();
    if(pid<0){
        fprintf(2,"fork failed\n");
        exit(1);
    }else if(pid==0){//child process
        close(p[1]);
        filter(p[0]);
    }else if(pid>0){//parent process
        close(p[0]);
        for(int i=2;i<=MAX_PRIME;i++){
            write(p[1],&i,sizeof(int));
        }
        close(p[1]);
        wait(0);
    }    
    exit(0);
}
