//
// Created by andysnake on 07/11/18.
//
#pragma once

#include "GUI.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "app.h"

const int PIPEMAX=1048576;
struct gui_basic_link open_GUI_connection() {
    int pid;                    //new worker for operation
    int pipes[2][2];            //PIPEs WHERE WILL BE REDIRECTED I-O OF NEW TASK
    pipe(pipes[0]);
    pipe(pipes[1]);
    ///TRY EXTEND PIPE, USEFUL BECAUSE PIPE OVERFLOW MAY OCCUR WITH SLOW GUI MODULE
    printf("pipe sizes 0:%d 1:%d \n",fcntl(pipes[1][0],F_GETPIPE_SZ),fcntl(pipes[1][1],F_GETPIPE_SZ));
    if(fcntl(pipes[1][0], F_SETPIPE_SZ,PIPEMAX)<0)
        perror("pipe extend buffer err: ");
    if(fcntl(pipes[1][1],F_SETPIPE_SZ,PIPEMAX)<0)
        perror("pipe extend buffer err: ");
    pid = fork();
    int dup_res;
    if (pid == 0) {
        close(pipes[0][0]);         //close read end from first pip
        close(pipes[1][1]);         //close write end to second pipe

        //STD FILE DESCRIPTOR REDIRECTION FOR WORKER INTERACTION
        /// FIRST PIPE 0,1->stdout&stderr
        /// SECOND PIPE 1,0->stdin
        dup_res = dup2(pipes[0][1], STDOUT_FILENO);   //<--- worker output
        if (dup_res == -1) {        //IF ANY ONE OF REDIRECTION HAS FAILED EXIT
            perror("dup error ...");
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(pipes[0][1], STDERR_FILENO);   //<--- worker output
        if (dup_res == -1) {        //IF ANY ONE OF REDIRECTION HAS FAILED EXIT
            perror("dup error ...");
            exit(EXIT_FAILURE);
        }
        dup_res = dup2(pipes[1][0], STDIN_FILENO);
        if (dup_res == -1) {        //IF ANY ONE OF REDIRECTION HAS FAILED EXIT
            perror("dup error ...");
            exit(EXIT_FAILURE);
        }
        //finaly execute GUI script
        chdir("GUI");
        int exit_res = 0;
        if((exit_res=system("python GUI.py"))==-1){
            perror("GUI EXECVE ");
            exit(EXIT_FAILURE);
        }
        exit(exit_res);
    }

    //father
    char* readBUF=NULL;
    ssize_t readed=0;
    printf("starting echo loop \n");
    usleep(1000000);
    close(pipes[0][1]);
    close(pipes[1][0]);
    //copy fd of pipe of redirected IO of worker child on symbolic meaningfull vars
    _GUI.inp=pipes[1][1];
    _GUI.out=pipes[0][0];
    INTYPE=GUI;
    return _GUI;
}
int read_line_gui(char* buff,int max_size) {
    fprintf(stderr,"\nGUI LINK READLINE from :%d  \n",getpid());
    int  c=0;                //ammount of readed chars
    //read a line on stdout from in<--GUI link fd
    int in=_GUI.out;
    int r=0;
    while (r!='\n' || c==(max_size-1)) {
        if (read(in,&r, 1) == -1) {
            perror("read worker out ");
            return RESULT_FAILURE;
        }
        printf("%c",r);
        buff[c]= (char) r;
        c++;
    }
    buff[c]=0;
    fprintf(stderr,"RETRIVED %s \n",buff);
    return c;
}


