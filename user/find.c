#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void
find(char *path, char *filename) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  //open the path,id of file<0
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  //fail to find the state of file
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
    case T_FILE:
        //compare the end of path with target filename
        //eg:find /dir1/file1 file2
        if(strcmp(path+strlen(path)-strlen(filename),filename)==0)
            printf("%s\n",path);
        break;

    case T_DIR:
        //check the length of path
        //"1"for'/'
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            break;
        } 
        strcpy(buf,path);//buf:path
        p=buf+strlen(buf);
        *p++='/';//buf:path/
        
        //directory/subdirectory are treated like file
        //read every subdirectory of present directory to find target filename
        while(read(fd,&de,sizeof(de))==sizeof(de)){
            //empty subdirectory
            if(de.inum==0){
                continue;
            }

            memmove(p,de.name,DIRSIZ);
            p[DIRSIZ] = 0;//buf:path/de.name

            if(stat(buf, &st) < 0){
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            
            //skip"."and"..",enter next subdirectory and search
            if(strcmp(".",de.name)!=0 && strcmp("..",de.name)!=0){
                find(buf,filename);
            }
        }
        break;
    }

  close(fd);     
}

int
main(int argc, char *argv[]) {
  char *path;
  char *filename;

  if (argc < 3) {
    fprintf(2, "correct usage: find <path> <filename>...\n");
    exit(1);
  }
  else{
      path = argv[1];//pointer 'path' point at param1--the start position of whole path
      filename = argv[2];//pointer 'filename' point at param2--the start position of filename
      find(path, filename);
      exit(0);
  }
}