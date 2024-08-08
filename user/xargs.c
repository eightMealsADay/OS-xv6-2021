#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
    char *args[MAXARG];//store the params in command line
    int cur_args=0;
    for(;cur_args<argc-1;cur_args++){
        args[cur_args]=argv[cur_args+1];
    }

    char buf[40];
    int cur_buf=0;
    while(read(0,&buf[cur_buf],sizeof(char))>0){
        if(buf[cur_buf]==' '){//end of a param,store the param into args
            buf[cur_buf]='\0';
            args[cur_args] = malloc(cur_buf + 1); 
            strcpy(args[cur_args], buf);         
            cur_buf=0;
            cur_args++;
            continue;
        }else if(buf[cur_buf]=='\n'){//all params read,end of input
            buf[cur_buf]='\0';
            args[cur_args] = malloc(cur_buf + 1); 
            strcpy(args[cur_args], buf);
            args[cur_args+1]=0;

            int pid=fork();
            if(pid<0){
                fprintf(2,"fork failed\n");
                exit(1);
            }else if(pid==0){
                exec(args[0],args);
                exit(0);
            }else if(pid>0){
                wait((int*)0);
                cur_buf=0;
                cur_args=argc-1;
            }
        }else{
            cur_buf++;
        }
    }
    exit(0);
}