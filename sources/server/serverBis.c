#include "app.h"
#include "utils.h"
#include "utils.lc"
#include "sock_ntop.lc"
#include "connected_server.h"
#include "connected_server.c"

#include <sys/types.h>
#include <sys/socket.h> 
#include <arpa/inet.h>

#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <wait.h>
#include <utils.h>
#include "GUI/GUI.h"
/*
 * SERVER ENTRY POINT
 * a main task will wait new connection request
 * forked task will serve clients calling connected_server mainloop
 */
char INTYPE=CLI;                //GUI FORCE TO CLI
static int sockServerMain;      //main socket for server, global ref for shutdown on fatal fault
void exit_server(){
    kill(0, SIGUSR1);           //propagate exit intention
    pid_t	pid;
    int		stat;
     //wait eventually dead child to UNZOMBIES
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
        fprintf(stderr,"\n<:: child %d terminated\t with:%s\n",pid,stat==EXIT_SUCCESS?"success":"fail");
    shutdown(sockServerMain,SHUT_RDWR); //release main server socket
    exit(EXIT_FAILURE);

}
//signal handler to terminate correctly server on fatal fault, called ONLY ONCE
void serverErrExit(int signo);
void serverErrExit(int signo){
    fprintf(stderr,"SIGNAL :%d\n",signo);
    exit_server();
}

int main(int argc, char **argv) {
    INTYPE=CLI;                                     //force GUI disabling for server
    ///structures init
    controlcode receivedControlCode;                //will hold client connection code
    struct sockaddr_in addrCli;
    memset(&addrCli,0,sizeof(addrCli));
    int lenAddrCli = sizeof(struct sockaddr_in);
    int serviceSocket;                        //per client connected socket
    ///INIT MAIN SOCKET ANN BIND TO WELL KNOWN PORT
    if ((sockServerMain = SocketWrap()) == RESULT_FAILURE)
        exit(RESULT_FAILURE);
    if (BindWrap(sockServerMain, _SERV_PORT,_SERV_ADDR_BIND) == RESULT_FAILURE) {
        shutdown(sockServerMain, SHUT_RDWR);
        exit(RESULT_FAILURE);
    }
    /// REGISTER SIGNAL HANDLERS
    if(Sigaction(SIGCHLD,sig_chld)==RESULT_FAILURE) //to "unzombies" worker tasks
        exit(EXIT_FAILURE);

    if(Sigaction(SIGINT,serverErrExit)==RESULT_FAILURE)  //to force termination of server
        exit(EXIT_FAILURE);

    if(Sigaction(SIGUSR1,SIG_IGN)==RESULT_FAILURE)  //error propagation
        exit(EXIT_FAILURE);
    ///     :: SERVER MAIN TASK -> MAINLOOP ::
    while (96) {
        //blocking client request wait on main task
        if ((recvfrom(sockServerMain, &receivedControlCode, sizeof(controlcode),
                      0, (SA *) &addrCli, &lenAddrCli)) < 0) {  //**<-- REQ
            perror("error in recvfrom in  main server socket ");
            exit_server();
        }
        receivedControlCode= (controlcode) (receivedControlCode);
        printf("received   :%d from %s\n", receivedControlCode,
               sock_ntop((SA *) &addrCli, lenAddrCli));
        if(receivedControlCode!=REQUSTCONNECTION)
            continue;                        //wait for connection requests
        ///**3 WAY HANDSHAKE
        int pid=fork();             ///concurrent server -> new task to serve client !
        if (pid == 0) {             //child->           SERVER SERVICER PROCESS
            if ((serviceSocket = SocketWrap()) == RESULT_FAILURE)
                exit(RESULT_FAILURE);

            addrCli.sin_family=AF_INET;
            if (connect(serviceSocket, (SA *) &addrCli, (socklen_t) lenAddrCli) < 0) {   ///connect new socket from new port
                perror("error   in connect..");
                exit(RESULT_FAILURE);
            }
            controlcode toEchoBack,repliedBack;//control codes for handshake
            toEchoBack = (controlcode) (random() % MAXRANDOMSERVER);
            //**--> ping
            if(write(serviceSocket, (void *) &toEchoBack, sizeof(toEchoBack))<0) {
                perror("write ping ...");
                exit(EXIT_FAILURE);
            }
            //**<--pong
            if(read(serviceSocket, (void *) &repliedBack, sizeof(repliedBack))<=0){            //receive pong connection code & check if it's correct
                perror("read pong ...");
                exit(EXIT_FAILURE);
            }
            if (repliedBack == toEchoBack) {
                ///correctly achived a new connected socket with client
                printf("connected with client:%s\n", sock_ntop((SA *) &addrCli, lenAddrCli));

                waitOP(serviceSocket);           //server mainloop
                shutdown(serviceSocket,SHUT_RDWR);
                exit(RESULT_SUCCESS);     //child die
            }
            fprintf(stderr,"received invalid code for 3-handshake..\n");
            exit(EXIT_FAILURE);
        } else if(pid ==-1) {
            fprintf(stderr, "client connection to main port fork error\n");
        } else                  //father debug print
            fprintf(stderr,"::>new child to serve client created with pid:%d\n",pid);
    }
}
