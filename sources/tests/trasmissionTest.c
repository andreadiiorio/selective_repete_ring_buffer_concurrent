/*
 * TRASMISSION TEST
 * TODO IT'S SUPPOSED FREE UDP PORT 5300 ON LOCALHOST and file generated
 * execute file exchange between SENDER and RECEIVER process
 * client process will store received file in same dir of source file (files_sendable)
 * with destFilename as name
 * client ,on threads join , will check with shell diff command with system syscall
 * in that way will be reported with diff exit result if the files source and received are the same or not
 *
 * CONFIGURABILITY:
 * to use gcc atomics version of receiver and sender define macro GCCATOMICS(de comment next commented line)
 * by argv can be defined winsize, loss probability, extra buff space and file to exchange
 * other configure macros: in app.h -> PCKPAYLOADSIZE
 * ->in sender.h -> FILEIN_BLOCK_SIZE-> max read ammount...
 */

///TEST SENDER/RECEIVER VERSION WITH GCC ATOMICS read/write TRIGGER MACRO
//#define GCCATOMICS    //uncomment to test with gcc atomic read/write

#include <stdio.h>
#include <stdlib.h>
#include <wait.h>
#include <headers/connectedClient.h>
#include "utils.h"
#include "utils.lc"
#include "sender.h"
#include "receiver.h"

#ifdef GCCATOMICS
#include "serverScoreboard_ATOMIC_GCC.lc"
#include "clientScoreboard_ATOMIC_GCC.lc"
#else
#include "senderScoreboard.c"
#include "receiverScoreboard.c"
#endif
///FORCE GUI PIPE PRINT DISABLING...
#include "GUI/GUI.h"
char INTYPE=CLI;
/*
 * these costants hold destination filename
 * and source filename as the name of source file to send
 * and the relative size
 */
char* const destFilename="a.mp4";

long fileSize;
///DEFAULT FILE NAME FOR TRASMISSION
//char*  filename="enjoy.mp4";
//char* filename="tag.mp4";
char* filename="k.mp4";

///TX DEFAULT CONFIGUARATION PARAMETER
#define _WINDOWSIZE        22
#define _PLOSS             0.0
#define _EXTRABUFRINGLOGIC 3	///*
///configurable ring buf block sizes
#define SENDER_BLOCK_SIZE 3
#define RECEIVER_BLOCK_SIZE 2
//#define SENDER_BLOCK_SIZE FILEIN_BLOCKSIZE
//#define RECEIVER_BLOCK_SIZE FILEOUT_BLOCK_SIZE
struct tx_config txConfig={ .wsize=_WINDOWSIZE, .extraBuffSpace=_EXTRABUFRINGLOGIC,
        .maxseqN=(2 * (_EXTRABUFRINGLOGIC+_WINDOWSIZE)), .p_loss=_PLOSS};

extern char **environ;

int diffFiles(){
/*
 * test files sent and received with diff inside a shell using system
 * setting string for diff
 */
    int diffRes;
    char cmd[30];
    memset(cmd,0,sizeof(cmd));
    strcat(cmd,"diff ");
    strcat(cmd,filename);
    strcat(cmd," ");
    strcat(cmd,destFilename);
    printf(" command string for test :%s \n",cmd);
    diffRes=system(cmd);                                    ///EXECECUTE DIFF COMMAND
    //return convention 0 for success !0 for error
    return diffRes ? RESULT_FAILURE : RESULT_SUCCESS;           //map diff res to macro
}

int _receive(int connectedSocket){
    //RETURN RECEIVE OPERATION RESULT AND DIFF SRC FILE<->RCVD FILE
    ///initiating structures for fileExchange
        struct scoreboardReceiver* scoreboardReceiver1=initScoreboardReceiver(destFilename,fileSize,connectedSocket,&txConfig);
        if(!scoreboardReceiver1)
            exit(EXIT_FAILURE);
        scoreboardReceiver1->fileOut_BlockSize=RECEIVER_BLOCK_SIZE; ///OVERRIDE BLOCK SIZE
        int res=startReceiver(scoreboardReceiver1);
        if(res==RESULT_FAILURE) {
            fprintf(stderr, "RECEIVE FAIL OCCURRED \n");
            return RESULT_FAILURE;
        }
        shutdown(connectedSocket,SHUT_RDWR);
        close(connectedSocket);
    return RESULT_SUCCESS;
}

int _send(int connectedSocket){
    //INIT SENDER CONTROLLER  SCOREBOARD STRUCT AND SEND
    //RETURN SEND OPERATION RESULT
    struct scoreboardSender *scoreboardSender1 = initScoreboardSender(filename,fileSize,connectedSocket,&txConfig);
//    scoreboardSender1->sockfd=connectedSocket;
    if (!scoreboardSender1)
        exit(EXIT_FAILURE);
    scoreboardSender1->fileIn_BlockSize=SENDER_BLOCK_SIZE;  ///OVERRIDE BLOCK SIZE
    int result=startSender(scoreboardSender1);
    shutdown(connectedSocket,SHUT_RDWR);
    close(connectedSocket);
    return result==RESULT_FAILURE?EXIT_FAILURE:EXIT_SUCCESS;
}

// usage with default file and settings <>
char*usage="usage: <filename,winsize,extraRingBuffSize,lossP>\n";
int main(int argc, char **argv, char **envp){
    ///COMMAND LINE PARSING TX CONFIGS AND FILENAME
    for (int i=0;i<argc;i++)
    	fprintf(stderr,"%s ",*(argv+i));
    fprintf(stderr,"\n");
    if(argc ==5){
    	filename=*(argv+1);
	txConfig.wsize= (int) strtol(*(argv + 2), NULL, 0);
	txConfig.extraBuffSpace= (int) strtol(*(argv + 3), NULL, 0);
	if(txConfig.extraBuffSpace<=0){
		fprintf(stderr,"INVALID INITIALIZATION OF EXTRA RINGBUF, AT LEAST 1\n");
		txConfig.extraBuffSpace=1;
	}
	txConfig.p_loss=strtod(*(argv+4),NULL);
	txConfig.maxseqN=2*(txConfig.extraBuffSpace+txConfig.wsize);
	}
    else
        fprintf(stderr,"usage :%s USING DEFAULT FILE AND CONFIGS\n", usage);

    fprintf(stderr,"TX_CONFIGS: wsize %d extraRingBuffSpace %d lossProb %f \n",
            txConfig.wsize,txConfig.extraBuffSpace,txConfig.p_loss);
    //getting filesize
    chdir("../../files_sendable");
    struct stat file_stat;
    if(stat(filename,&file_stat)){
            perror("stat error  ..");
            return RESULT_FAILURE;
        }
    fileSize=file_stat.st_size;
    fprintf(stderr,"TESTING WITH FILE:%s of %lu bytes \n",filename,fileSize);
#ifdef GCCATOMICS
    fprintf(stderr,"\n GCC ATOMIC VERSION IN USE \n");
#endif
    int pid;                        ///WILL HOLD CHILD =SERVER PID
    int connectedSocket_Server=SocketWrap();
    if(connectedSocket_Server==RESULT_FAILURE) {
        exit(EXIT_FAILURE);
    }
    socklen_t len = sizeof(struct sockaddr_in);
    ///INIT SENDER SOCKET
    if(BindWrap(connectedSocket_Server,_SERV_PORT,_SERV_ADDR_BIND)==RESULT_FAILURE)    //INIT AT DEFAULT SERV PORT
        exit(EXIT_FAILURE);
    pid = fork();
    if (!pid){      ///CHILD= SENDER PROCESS
        ///getting connection
        controlcode receivedCode=0;
        struct sockaddr_in destAddr;
        memset(&destAddr,0, sizeof(destAddr));                                      //preset dest addr struct to 0
        ssize_t nRcvd;
        errno=0;
        //received control code from client and taking source address
        if((nRcvd=recvfrom(connectedSocket_Server, &receivedCode, sizeof(controlcode), 0, (SA*)&destAddr, &len))==-1){
            perror("recevfrom err");
            exit(EXIT_FAILURE);
        } else if(nRcvd==0)
            printf("received emtpy pck from %s \n",sock_ntop((SA*)&destAddr,len));
        if(receivedCode!=REQUSTCONNECTION){
            fprintf(stderr,"INVALID CODE...\n");
            exit(EXIT_FAILURE);
        }
        //connecting socket with dest addre from received code on rcvfrom
        if(connect(connectedSocket_Server,(SA*)&destAddr,len)<0){
            perror("connect error ");
            exit(EXIT_FAILURE);
        }
        printf("CONNECTED WITH %s\n",sock_ntop((SA*)&destAddr,len));
        ///SENDER OP
        int res=_send(connectedSocket_Server);
        printf("RESULT SENDER SIDE :%s \n",res==EXIT_SUCCESS?"success":"fail");
        exit(res);                              ///CHILD=SENDER DIE
    }
    else if(pid==-1) {
        fprintf(stderr, "FORK ERROR\n");
        exit(EXIT_FAILURE);
    }
    else {  ///father = receiver
        int connectedSocket_Client = SocketWrap();
        if (connectedSocket_Client == RESULT_FAILURE)
            exit(EXIT_FAILURE);
        remove(destFilename);
        ///getting connection
        struct sockaddr_in servaddr;
        //init structures & socket
        memset((void *) &servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(_SERV_PORT);
//        servaddr.sin_addr.s_addr=htonl(_SERV_ADDR_BIND);
        if (inet_pton(AF_INET, _dfl_servAddress, &servaddr.sin_addr) <=
            0) {        //fill serv address
            fprintf(stderr, "error in inet_pton \t %s", _dfl_servAddress);
            exit(RESULT_FAILURE);
        }
        ssize_t sndTo;
        controlcode reqConnectio = REQUSTCONNECTION;
        errno = 0;
        ///CONNECTING SOCKET TO SENDER
        if (connect(connectedSocket_Client, (SA *) &servaddr, len) < 0) {
            perror("connect error ");
            exit(EXIT_FAILURE);
        }
        if (writeWrap(connectedSocket_Client, sizeof(reqConnectio),
                      &reqConnectio) ==
            RESULT_FAILURE)  //sending request connection code
            exit(EXIT_FAILURE);

        printf("connected with server at %s",
               sock_ntop((SA *) &servaddr, sizeof(servaddr)));
        ///RECEIVE OPERATION
        int res_tx = _receive(connectedSocket_Client);
        printf("RESULT RECEIVER SIDE :%s \n",res_tx == RESULT_SUCCESS ? "success" : "fail");
        if(res_tx==RESULT_SUCCESS) {
            int diff_res = diffFiles();
            printf("RESULT DIFF OF FILES:%s \n",
                   diff_res == RESULT_SUCCESS ? "success" : "fail");
        }
        wait(NULL);         //UNZOMBIE SENDER
    }
    return EXIT_SUCCESS;
}

