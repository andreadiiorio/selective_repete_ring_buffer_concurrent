/*
 * SERVER BOUNDARY MODULE
 * this module will be used by a task connected to 1! client
 * so connectedSocket is a name used to identify the per client UDP "connected" socket->reffered as socket liv=1 on diagram
        ->creation a new socket will block cli&server to avoid read call overlap on main socket (used to comunicate new cli wildcard addr port binding result)
 *      ->also for PUT/GET op new process will be created to serve the request
 */
#include <wait.h>
#include "connected_server.h"
#include "app.h"
#include "utils.h"
//#include "../utils.c"
#include "PckFunctions.h"
#include "../PcKFunctions.lc"
#include "sender.h"
#include "senderScoreboard.c"
#include "receiver.h"
#include "receiverScoreboard.c"

unsigned long lookupForFilename(char *fname);                   //lookup for filesize of filename fname in files dir

void waitOP(int connectedSocket) {                  ///server Client connected main loop
    /*
     * serve client requests (received on connectedSocket)
     * fails propagated  exiting -> EXIT_FAILURE will be returned to mainServer task
     * on NO_REQUEST_TIMEOUT  || DISCONNECT request  will be exited with EXIT_SUCCESS.
     */
    controlcode requestCode;                // will hold request code of control message arrived from client
    int readRes;                            //read wrap result
    ///put vars
    char filename[MAX_FILENAME_SIZE+PREFIX_SIZE];
    long fileSize;
    int put_res,get_res;                    //init res of file exchange OPs

    int pid,stat;                   //exit resoults of workers
    int exit_res=EXIT_SUCCESS;
    if(chdir("../files_sendable")==-1){
        perror("changing dir to files exchangeble err ..");
        goto exit_abort;
    }
    ///TAKING TRASMISSION CONFIGS FROM CLIENT
    struct tx_config client_tx_config;
    char serializedTrassmissionConfigs[TX_CONGIS_SERIALIZED_SIZE];      //tmp buf where will be placed received tx configs
    if(readWrap(connectedSocket,TX_CONGIS_SERIALIZED_SIZE,serializedTrassmissionConfigs)==RESULT_FAILURE)
        exit(EXIT_FAILURE);
    deserializeTXConfigs(&client_tx_config,serializedTrassmissionConfigs);  //deserialize tx configs from raw data received in buffer
    ///CLIENT OPERATION HANDLER MAINLOOP
    for(;;) {
        readRes = readWrap(connectedSocket, sizeof(controlcode), &requestCode); //wait request
        printf("RECEIVED REQUEST CODE: %d on process :%d\n",requestCode,getpid());
        //timeout setted on read, SIGALRM will be delivered nothing readed for TIMEOUTCONNECTION
        if (readRes == RESULT_FAILURE) {
            fprintf(stderr, "fail while reading request from socket\n");
            exit_res=EXIT_FAILURE;
            goto exit;
        }
        switch (requestCode) {
            case DISCONNECT_REQUEST_CODE:
                printf("CLIENT FROM SOCKET FD:%d DISCONNECTED \n", connectedSocket);
                goto exit_abort;
            case TERMINATE_TXs_AND_DISCONNECT:
                printf("client will be disconnected at the end of actual jobs\n");
                goto exit;
            case LIST_REQUEST_CODE:
                list(connectedSocket);                  //send list string back to cli on socket
                break;

            case PUT_REQUEST_CODE:
                ///reading filename and filesize from client of file
                strcat(filename,prefix_server);         //preset prefix to distinguish clie-serv files
                readRes=readline_socket(connectedSocket,MAX_FILENAME_SIZE,filename);
                if(readRes==RESULT_FAILURE){
                    fprintf(stderr,"err in put request handling \n");
                    exit_res=EXIT_FAILURE;
                    goto exit_abort;
                }
                readRes=readWrap(connectedSocket,sizeof(fileSize),&fileSize);       //read file size from client
                if(readRes==RESULT_FAILURE) {
                    fprintf(stderr, "err in put request handling \n");
                    exit_res=EXIT_FAILURE;
                    goto exit_abort;
                }
                fileSize= be64toh(fileSize);                         //deserialize size
                if(fileSize==ERR_FILENOTFOUND)
                    break;
                printf("put request of file:%s of bytes %ld\n",filename,fileSize);
                put_res= put(filename,fileSize,connectedSocket,&client_tx_config);
                if(put_res!=RESULT_SUCCESS){
                    fprintf(stderr,"err occurred during put from client on socket %d \n",connectedSocket);
                }
                break;

            case GET_REQUEST_CODE:
                ///read filename of requested file
                if(readline_socket(connectedSocket,sizeof(filename),filename)==RESULT_FAILURE){
                    fprintf(stderr,"err during get... reading on socket %d \n",connectedSocket);
                    exit_res=EXIT_FAILURE;
                    goto exit_abort;
                }
                printf("request file %s  from socket %d\n",filename,connectedSocket);
                if((get_res = get(filename, connectedSocket,&client_tx_config))==RESULT_FAILURE)
                    fprintf(stderr,"err in init for get..\n");
                break;

            default:
                fprintf(stderr, "INVALID REQEUST CODE RECEIVED FROM CLIENT ON SOCKET:%d\n",connectedSocket);
        }
    }
    exit_abort:
        Sigaction(SIGUSR1,SIG_IGN);                     //to survive to slaughter
        kill(0,SIGUSR1);                                //propagate quit to famili
        exit:
            while ((pid = waitpid(-1, &stat, 0)) > 0)    //unzombies
                fprintf(stderr,"<::child    %d terminated\t with:%s\n", pid,stat==EXIT_SUCCESS?"success":"fail");
            shutdown(connectedSocket,SHUT_RDWR);
            exit(exit_res);
}
int list(int connectedSocket) {
    //list files in current dir in a linked list, send them serialized to server
//    struct FILES_LLIST_TYPE head_LS=SLIST_HEAD_INITIALIZER(&head_LS);     //get llist head initated
    struct file_ll* file_node;                                          //generic item on ls linked list
    struct FILES_LLIST_TYPE head_LS=SLIST_HEAD_INITIALIZER(head_LS);     //get llist head initated
    SLIST_INIT(&head_LS);
    int res=listCurrDir(&head_LS);                                     //getting liked list of files info and ammount of files
    if(res==RESULT_FAILURE) {
        return RESULT_FAILURE;
    }
    ///comumnicating client ammount of ls in byte to read from socket
    uint32_t ls_ammount_bytes= res * (sizeof(file_node->filesize) + sizeof(file_node->filename));
    printf("ls size is :%d \n",ls_ammount_bytes);
    htonl(ls_ammount_bytes);
    if(writeWrap(connectedSocket,ls_ammount_bytes,&ls_ammount_bytes)==RESULT_FAILURE)
        return RESULT_FAILURE;
    unsigned long fileSize2send;
    SLIST_FOREACH(file_node,&head_LS,links){
        printf("name %s size %ld\n",file_node->filename,file_node->filesize);
        fileSize2send=file_node->filesize;
        fileSize2send= htobe64(fileSize2send);          //serialize in net byte order
        ///SEND FILENAME and FILESIZE
        if(writeWrap(connectedSocket, sizeof(file_node->filename),file_node->filename)==RESULT_FAILURE)
            return RESULT_FAILURE;
        if(writeWrap(connectedSocket, sizeof(file_node->filesize),&fileSize2send)==RESULT_FAILURE)
            return RESULT_FAILURE;
        SLIST_REMOVE_HEAD(&head_LS,links);
        free(file_node);
    }
//    chdir("sources/server");                                              //move back to old folder
    return RESULT_SUCCESS;
}


int get(char* fileName, int connectedSocket,struct tx_config* tx_configs) {
    /*
     * handle get requests of fileId file,
     * new process and new socket will be created for file sending (USING SENDER CONTROLLER MODULE)
     * init error will be returned to calling process as MACRO RESULT_FAILURE
     * client wait as init result filesize or ERR_* as unsigned long
     * OPERATION RESULT will be  handled from newly created task which will display them
     *
     */
    long opRes;
    ///INITIALIZATION WITH CLIENT NOTIFY FILESIZE or INIT ERR OCCURRED
    ///SEARCH REQUESTED FILENAME IN dir files_sendable
    unsigned long fileSize=lookupForFilename(fileName);
    if(fileSize==RESULT_FAILURE) {
        opRes=ERR_FILENOTFOUND;
        opRes= htobe64(opRes);
        if (writeWrap(connectedSocket, sizeof(opRes), &opRes) == RESULT_FAILURE) {       ///NOTIFY CLIENT FILE NOT FOUND
            printf("REQUESTED :%s \n",fileName);
            fprintf(stderr, "INVALID WRITE INIT RESULT\n");
        }
        return RESULT_FAILURE;
    }
    /// INIT END ..comunicating requested  file size to client
    fileSize=htobe64(fileSize);
    if(writeWrap(connectedSocket,sizeof(fileSize),&fileSize)==RESULT_FAILURE)
        exit(EXIT_FAILURE);
    fileSize=be64toh(fileSize);
    ///get new socket only for GET operation BEFORE! greet new client REQUEST
    int socketOp=getNewConnection_ServerSide(connectedSocket);
    if(socketOp==RESULT_FAILURE){
        fprintf(stderr,"GETTING NEW SOCKET FOR GET ERR\n");
        exit(EXIT_FAILURE);
    }
    int pid=fork();
    if(!pid){                                               //chld=new task for SENDING
        struct scoreboardSender* scoreboardSender1 = initScoreboardSender(fileName,fileSize,socketOp,tx_configs);
        //init scoreboard with requested file and connected socket
        if(scoreboardSender1==NULL){
//            opRes=ERR_INIT;
//            writeWrap(connectedSocket,
            fprintf(stderr,"SENDER SCOREBORD INTIALIZATION ERR OCCURRED \n");fflush(0);
            exit(EXIT_FAILURE);
        }
        scoreboardSender1->sockfd=socketOp;                     //SET PER OP CONNECTED SOCKET
        //// start SENDER THREADS and wait tx final result
        int getResult=startSender(scoreboardSender1);           //GET->SENDER CONTROLLER MODULE JOB
        if(getResult!=RESULT_SUCCESS){
            fprintf(stderr,"GET HAS FAILED on file %s \n",fileName);
            exit(EXIT_FAILURE);
        }
        printf("GET OP SUCCESS ON FILE %s \n",fileName);
        exit(EXIT_SUCCESS);
    }
    else if(pid==-1){
        fprintf(stderr,"fork has failed on socket %d\n",connectedSocket);
        opRes=ERR_INIT;
        opRes= htobe64(opRes);
        writeWrap(connectedSocket,sizeof(opRes),&opRes);    ///propagate init error..fork
        return ERR_OCCURRED;                                //propagate err
    } else
        fprintf(stderr,"::>created new process to handle get op with pid :%d \n",pid);

    return RESULT_SUCCESS;
}


unsigned long lookupForFilename(char *fname) {
    //lookup if fname is avaible in files_sendable and return filesize
    //otherwise RESULT_FAILURE
//    chdir("../files_sendable");
    unsigned long filesize=0;                                             //size of founded file

    struct FILES_LLIST_TYPE head_LS=SLIST_HEAD_INITIALIZER(head_LS);
    SLIST_INIT(&head_LS);                                               //LList def & INIT
    struct file_ll* file_node;                                          //generic item on ls linked list
    int nfiles=listCurrDir(&head_LS);
    struct file_ll* file_i;
    SLIST_FOREACH(file_i,&head_LS,links){
        if(!strncmp(file_i->filename, fname, MAX_FILENAME_SIZE))
            filesize = file_i->filesize;
        SLIST_REMOVE_HEAD(&head_LS,links);                              //assiming 1! match remove itmes founded in same loop
        free(file_i);
    }
//    chdir("server");
    printf("%s %s of %lu bytes \n",filesize?"founded":"NOT FOUNDED",
                    fname,filesize);fflush(0);
    return filesize?filesize:RESULT_FAILURE;                            //RETURN FILESIZE OR ERR NOT FOUNDED
}


int _put(int connectedSocket,int socketOP,struct tx_config* tx_configs,char* fileName,unsigned long fileSize){
    /*
     * handle put op for newly created process, OP RESULT will be notified back to client
     * process termination at the end
     * will be created new socket connected with client and new process
     */
        int exit_res = EXIT_SUCCESS;                                //will hold OP task exit result
        controlcode opRes;

        ///init structures for RECPETION
        struct scoreboardReceiver* scoreboardReceiver1;
        char dstFilename[MAX_FILENAME_SIZE+PREFIX_SIZE];
        memset(dstFilename,0, sizeof(dstFilename));                 //preset str
        strcat(dstFilename,prefix_server);
        strcat(dstFilename,fileName);
        printf("PUT OP WILL PRODUCE FILE %s \n\n",dstFilename);
        scoreboardReceiver1 = initScoreboardReceiver(dstFilename,fileSize,0,tx_configs);
        if (scoreboardReceiver1 == NULL) {
            fprintf(stderr, "err occurred during client scoreboard init \n");
            opRes = ERR_INIT;
            writeWrap(connectedSocket, sizeof(controlcode),&opRes);                 ///notify client INITIALIZATION FAIL
            return RESULT_FAILURE;
        }
        scoreboardReceiver1->socket=socketOP;                                   //set in scoreboard struct newly obtained socket
        int pid;
        if((pid=fork())==0) {
            int put_res = startReceiver(scoreboardReceiver1);                             //PUT -> receiver controller
            ///comunicate put result server side back to client
            if (put_res != RESULT_SUCCESS) {
                fprintf(stderr, "ERR DURING PUT OP of file :%s IN SERVER...on socket :%d \n", fileName, socketOP);
                opRes = RESULT_FAILURE;
                writeWrap(socketOP, sizeof(controlcode), &opRes);         ///notify client ERR OCCURRED
                shutdown(socketOP, SHUT_RDWR);
                close(socketOP);
                exit(EXIT_FAILURE);

            } else {
                printf("\n!!sucessful put op of file %s \n", fileName);
                opRes = RESULT_SUCCESS;
                writeWrap(socketOP, sizeof(controlcode), &opRes);         ///notify client OP OK
                shutdown(socketOP, SHUT_RDWR);
                close(socketOP);
                exit(EXIT_SUCCESS);
            }
        } else if (pid==-1){
            fprintf(stderr,"FORK FAILED\n");
//            opRes=ERR_INIT;
//            writeWrap(connectedSocket, sizeof(opRes),&opRes);
            return RESULT_FAILURE;
        } else
            fprintf(stderr,"::>created new task for PUT operation with pid :%d\n",pid);
    return RESULT_SUCCESS;
}
int put(char *fileName, unsigned long fileSize, int connectedSocket,struct tx_config* tx_configs) {
    /*
     * handle put operation server side,
     * will be handled the reception of filename of fileSize bytes on a new process/socket
    */
    int writeRes;
    int socketOP;
    ///GET NEW SOCKET FOR PUT OPERATION
    if((socketOP = getNewConnection_ServerSide(connectedSocket))==RESULT_FAILURE)
        return RESULT_FAILURE;
    controlcode  opRes;                                       //will hold if err has occurred 0=> no errors
    return _put(connectedSocket,socketOP,tx_configs,fileName,fileSize);            //handle put operation and exit
}
