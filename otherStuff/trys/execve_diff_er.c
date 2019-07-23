//
// Created by andysnake on 28/10/18.
//

#include <stdio.h>
#include <endian.h>
#include <unistd.h>
//#include <app.h>
#include <memory.h>
#include <wait.h>
#include <stdlib.h>

char* const destFilename="a.mp4";
char* const filename="enjoy.mp4";
//const char* filename="tag.mp4";
const int fileSize=62047639;                           //enjoy
//const int fileSize=1726672266;                      //tag
extern char **environ;
int main(){
    int pid=fork();
    if(!pid){   //child exec diff
//        size_t Len= max (strlen(destFilename), strlen(filename));
        char* const parameters[3]={"diff",filename,destFilename};
         printf("parameters debug %s \n",parameters[2]);fflush(0);
        chdir("../files_sendable");
        printf("argv %s %s %s \n",parameters[0],parameters[1],parameters[2]);
        if(execve("diff", (char *const *) parameters, environ) == -1){
            perror("EXECVE ");
            exit(EXIT_FAILURE);
        }
    }
    else if(pid ==-1)
        fprintf(stderr,"test forking to diff failed\n");
    else            //father
        printf("created child with pid %d to check diffs\n",pid);
    int res ;
    waitpid(pid, &res, 0);
    printf("res:%s \n",res==EXIT_FAILURE?"RESULT_FAILURE":"RESULT_SUCCESS");
    return res;
}