#include "connectedClient.h"
#include "receiver.h"
#include "receiverScoreboard.c"
#include "utils.h"
#include "sock_ntop.lc"
#include "app.h"
#include "PckFunctions.h"
#include "../PcKFunctions.lc"
#include "sender.h"
#include "senderScoreboard.c"
#include <unistd.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <wait.h>
#include "GUI/GUI.h"
//#include "../utils.c"
///GUI SHARED LINKS
char INTYPE;     ///HOLD THE INPUT KIND (GUI||CLI) gived at client start by argv
struct gui_basic_link _GUI;

void client_mainloop(int connectedSocket) {
    /* get request to send to server by connected socket
     * for PUT&GET ops will be handled init err (until fork) on calling task
     * operation result will be displayed from newly created task for the op.
     */
    int opInitRes;              //init result of operation demanded until fork
    controlcode request=0;      //input request code to send to server
    const int MAXRAWREQ_SIZE=6; //max size of raw request size
    char _rawRequest[MAXRAWREQ_SIZE];
    char* endpntr;
    int pid,stat;           //exit resoults of workers for propagated termination...
    fflush(0);
    errno=0;
    if(chdir("../files_sendable")==-1){  //change dir to files to send/receive
        perror("changing dir to sendable files folder... ");
        goto exit_cli_abort;
    }
    ///TAKE TX CONFIGS AND COMUNICATE THEM TO SERVER
    struct tx_config requestedTxConfigs=tx_configCreate(connectedSocket);
    printf("\nUSAGE\n%s\n",help);
    for(;;){
        if(INTYPE!=GUI) {
            if (fgets(_rawRequest, MAXRAWREQ_SIZE, stdin) == NULL) {
                fprintf(stderr, "err reading input request\n");
                goto exit_cli_abort;
            }
        } else{                ///GUI
            memset(_rawRequest,0, sizeof(_rawRequest));             //preset ?
            if(read_line_gui(_rawRequest, sizeof(_rawRequest))==RESULT_FAILURE){
                fprintf(stderr,"GUI INPUT ERROR \n");
                goto exit_cli_abort;
            }
        }
        _rawRequest[strcspn(_rawRequest, "\n")] = 0;    //substitute \n
        if(!strncmp(_rawRequest,"help",4)){            ///show help if requested
            printf("%s",help);
            continue;
        }
        request= (controlcode) strtol(_rawRequest, &endpntr, 0);    //get request code from raw input
        if(endpntr==_rawRequest||*endpntr) {                        //code parsing err
            fprintf(stderr, "INVALID INPUT\n");
            continue;
        }
        ///sending CONTROL MESSAGE with request code to server
        if(writeWrap(connectedSocket,sizeof(request),&request)==RESULT_FAILURE)
            goto exit_cli_abort;
        ///request identify
        switch (request){
            case LIST_REQUEST_CODE:
                printf("AVAIBLE FILES ON SERVER:\n");
                list_client(connectedSocket);
                break;
            case GET_REQUEST_CODE:
                printf("\nINSERT FILENAME TO GET\n");
                opInitRes=get_client(connectedSocket,&requestedTxConfigs);
                if(opInitRes==RESULT_FAILURE)
                    fprintf(stderr,"init err occurred \n");
                break;
            case PUT_REQUEST_CODE:
                printf("\nINSERT FILENAME TO PUT\n");
                opInitRes= put_client(connectedSocket,&requestedTxConfigs);
                if(opInitRes==RESULT_FAILURE)
                    fprintf(stderr,"init err occurred \n");
                break;
            case DISCONNECT_REQUEST_CODE:
                printf("disconnected request sent \n");
                goto exit_cli_abort;
            case TERMINATE_TXs_AND_DISCONNECT:  ///useful mostly for concurrency test
                fprintf(stderr,"txs will be terminated ... then disconnected\n");
                goto exit_cli;                     //just wait the end of childs workers and exit...
            default:
                fprintf(stderr,"INVALID REQUEST ....\n");
                printf("%s\n",help);
        }
    }
   exit_cli_abort:
            //abort all operations, kill childs and unzombies avoiding daemons rising ..
            fprintf(stderr,"\nABORTING ALL OP \n\n");
            Sigaction(SIGUSR1,SIG_IGN);          //to survive to slaughter of workers
            kill(0, SIGUSR1);                   //propagate quit
            exit_cli:                           //lighter term-->wait the end of workers
                while ((pid = waitpid(-1, &stat, 0)) > 0)    //unzombies
                    fprintf(stderr,"<::child    %d terminated\t with:%s\n", pid,stat==EXIT_SUCCESS?"success":"fail");
                shutdown(connectedSocket,SHUT_RDWR);
                exit(EXIT_SUCCESS);
}
void _get(char *filename_src, unsigned long fileSize,int socketOP,struct tx_config* txConfigs) {
    /*
     * handle  GET operation from a new task
     * will be notified GET OP result (client side only) just before task exiting
     */
    ///presetted prefix to filename to avoid same machine reopen
    char filename_dst[MAX_FILENAME_SIZE+PREFIX_SIZE];
    memset(filename_dst,0, sizeof(filename_dst));
    strcat(filename_dst,prefix_client);
    strcat(filename_dst,filename_src);
    printf("requested file(with prefix) : %s of size %ld\n",filename_dst,fileSize);

    int resultOP,exitRes;                          //support vars meaning by name
    /// move to RECEIVER CONTROLLER MODULE
    struct scoreboardReceiver* scoreboardReceiver1;
        scoreboardReceiver1 = initScoreboardReceiver(filename_dst,fileSize, socketOP, txConfigs);
    if(scoreboardReceiver1==NULL){
        fprintf(stderr,"invalid client init...\n");
        exitRes=EXIT_FAILURE;
        goto exit;
    }
    resultOP=startReceiver(scoreboardReceiver1);    //GET -> RECEIVER CONTROLLOER .JOB

    if(INTYPE==GUI){
        char _endTx[MAX_FILENAME_SIZE+3];
        memset(_endTx,0, sizeof(_endTx));
        int len=snprintf(_endTx, sizeof(_endTx),"%s,1",filename_dst);
        writeWrap(_GUI.inp,len,_endTx);     ///NOTIFY GUI TX END
    }
    if(resultOP!=RESULT_SUCCESS){
        fprintf(stderr,"GET FAIL of file %s \n",filename_dst);
        exitRes=EXIT_FAILURE;
        goto exit;
    }
    exitRes=EXIT_SUCCESS;
    exit:
        fprintf(stdout,"GET of %s result: %s \n",filename_dst,exitRes==EXIT_FAILURE?"failure":"success");
        shutdown(socketOP,SHUT_RDWR);
        usleep(10000);                  //hopefully wait pipe flushing from slow python :((
        memset(filename_dst,0, sizeof(filename_dst));
        int len=snprintf(filename_dst, sizeof(filename_dst),"%s,1.0,?\n",filename_dst);
        if(len>0)
            writeWrap(_GUI.inp, (size_t) len, filename_dst);    //notify get finished
        exit(exitRes);
}


int get_client(int connectedSocket,struct tx_config* txConfigs){
    //handle file download from server with GET REQUESTS to server ,
    //return init result as macro,
    // GET op result notified from forked task before exit
    ///taking filename from input
    char filename[MAX_FILENAME_SIZE];       //getting related filename to last list received..
    if(INTYPE!=GUI) {
        if (fgets(filename, MAX_FILENAME_SIZE, stdin) == NULL) {
            fprintf(stderr, "err reading fileID\n");
            return RESULT_FAILURE;
        }
    } else{
        if(read_line_gui(filename, sizeof(filename))==RESULT_FAILURE){
            fprintf(stderr,"GUI INPUT TAKING ERR\n");
            return RESULT_FAILURE;
        }
    }
    filename[strcspn(filename, "\n")] = 0;                  //substitute \n
    ///sending  requested filename string
    if(writeWrap(connectedSocket, sizeof(filename),filename)==RESULT_FAILURE){
        fprintf(stderr,"err write file to server1\n");
        return RESULT_FAILURE;
    }
    ///TAKE FILESIZE OR  INIT ERROR
    long fileSize;
    if(readWrap(connectedSocket,sizeof(fileSize),&fileSize)==RESULT_FAILURE){
        fprintf(stderr,"invalid read init response from server\n");
        return RESULT_FAILURE;
    }
    fileSize=be64toh(fileSize);
    if(fileSize<0){                 //ERR HAS OCCURRED SERVER SIDE...
        if(INTYPE==GUI)             //GUI NOTIFY FILE NOT FOUNDED
            writeWrap(_GUI.inp, sizeof(FILENOTFOUNDED_STR),
                      (void *) FILENOTFOUNDED_STR);
        fprintf(stderr, "GET ERROR RECEIVED SERVER ERR CODE :%d \n", (controlcode) fileSize);
        if(fileSize==ERR_FILENOTFOUND)
            fprintf(stderr,"requested file NOT FOUNDED ON SERVER files_sendable dir \n");
        return RESULT_FAILURE;
    }

    ///get new socket for GET operation
    int socketOP=getNewConnection_ClientSide(connectedSocket);
    if(socketOP==RESULT_FAILURE){
        fprintf(stderr,"NEW CONNECTION FAILED DURING GET\n");
        return RESULT_FAILURE;
    }
    printf("GET CONNECTION OBTAINED ");fflush(0);

    int pid=fork();
    if(pid==0) {            ///cld for GET OPERATION
        _get(filename, (unsigned long) fileSize, socketOP, txConfigs);
    }
    else if(pid==-1){
        fprintf(stderr,"new child for operation creation faild\n");
        return RESULT_FAILURE;
    } else
        printf("correctly created a new process to handle GET req on pid :%d\n",pid);
    return RESULT_SUCCESS;
}

int list_client(int connectedSocket){
    //GUI LINK
    char file_item_list[33];    //printable list item
    int list_item_len;          //effectively item len
    uint32_t bytesOfLs=0;
    ///get ls size in byte expected to be readed
    if(readWrap(connectedSocket,sizeof(bytesOfLs),&bytesOfLs)==RESULT_FAILURE)
        return RESULT_FAILURE;

    ntohl( bytesOfLs);
    char ls[bytesOfLs];
    char* lstmp=ls;
    ///read ls string and put on output(pipe or stdout)
    if(readWrap(connectedSocket,bytesOfLs,ls)==RESULT_FAILURE)      //getting ls size
        return RESULT_FAILURE;
    char filename_i[MAX_FILENAME_SIZE];
    unsigned long file_i_size;
    int byteCpyd=0;
    while(byteCpyd<bytesOfLs) {
        memcpy(&filename_i, lstmp, MAX_FILENAME_SIZE);                       //get +
        lstmp+= MAX_FILENAME_SIZE;
        byteCpyd+=MAX_FILENAME_SIZE;
        memcpy(&file_i_size,lstmp,sizeof(file_i_size));
        file_i_size=be64toh(file_i_size);                                       //deserialize filesize
        lstmp+=sizeof(file_i_size);
        byteCpyd+=sizeof(file_i_size);
        list_item_len = snprintf(file_item_list, sizeof(file_item_list),
                                 "-%s\t%lu\n", filename_i, file_i_size);  ///output list
        if(list_item_len<0){
            fprintf(stderr,"snprintf FAILED \n");
            return RESULT_FAILURE;
        }
        printf("%s\n",file_item_list);
        if(INTYPE==GUI){
            if(writeWrap(_GUI.inp,list_item_len,file_item_list)==RESULT_FAILURE){
                fprintf(stderr,"LIST GUI WRITE ERR FATAL \n");
                return RESULT_FAILURE;
            }
        }
    }
    return RESULT_SUCCESS;
}


void _put(char *file, unsigned long filesize,int socketOP,struct tx_config* tx_configs) {
    /*
     * HANDLE FILE PUT OPERATION ON NEW PROCESS
     * DISPLAY OPERATION RESULT OF CLIENT AND SERVER SIDE...
     */
    int putOpRes=EXIT_SUCCESS;                  //WILL HOLD FINAL PUT OP RESULT
    struct scoreboardSender* scoreboardSender1;
    char PUT_RES_FINAL[MAX_FILENAME_SIZE+4];
    int len_out_str;
    scoreboardSender1 = initScoreboardSender(file, filesize,socketOP,tx_configs);
    if(scoreboardSender1==NULL){
        fprintf(stderr,"INVALID SENDER INIT ON CLIENT\n");
        putOpRes=EXIT_FAILURE;
        goto exit_;
    }
    int putRES_cli_side=startSender(scoreboardSender1);
    if(INTYPE==GUI){
        char _endTx[MAX_FILENAME_SIZE+3];
        memset(_endTx,0, sizeof(_endTx));
        int len=snprintf(_endTx, sizeof(_endTx),"%s,1",file);
        writeWrap(_GUI.inp,len,_endTx);     ///NOTIFY GUI TX END
    }
    controlcode putRES;
    if(putRES_cli_side!=RESULT_SUCCESS){
        fprintf(stderr,"PUT FILE ERR OCCURRED _cli side \n");
        putOpRes=EXIT_FAILURE;
        goto exit_;
    } else{                                                     //OK client side
        printf("PUT CLINT-SIDE ==> OK\n");fflush(0);
        /// get PUT OP RESULT SERVER SIDE...
        if(readWrap(socketOP,sizeof(controlcode),&putRES)==RESULT_FAILURE){
            fprintf(stderr,"INVALID READ PUT RESULT SERVER SIDE \n");
            putOpRes=EXIT_FAILURE;
            goto exit_;
        }
        /// OPERATION RESULT UI NOTIFY
        if(putRES==RESULT_FAILURE){
            fprintf(stderr,"SERVER PUT ERR OCCURRED with code :%d\n",putRES);
            putOpRes=EXIT_FAILURE;
            goto exit_;
        }
        printf("SERVER PUT OK!!!!!!!!!!\n");                    //OK server side
        putOpRes=EXIT_SUCCESS;
    }
    exit_:
            shutdown(socketOP,SHUT_RDWR);
            close(socketOP);
            usleep(100000);
            if(INTYPE==GUI) {       ///output PUT OP. RESULT TO UI
                memset(PUT_RES_FINAL,0, sizeof(PUT_RES_FINAL));
                len_out_str = snprintf(PUT_RES_FINAL, sizeof(PUT_RES_FINAL),
                                       "_%s PUT RESULT FINAL:%s\n", file,putOpRes?"FAIL":"SUCCESS!!");
                if(len_out_str<0) {
                    fprintf(stderr, "snprintf error\n");
                    exit(putOpRes);
                }
                fprintf(stdout,"%s\n",PUT_RES_FINAL);
                writeWrap(_GUI.inp, (size_t) len_out_str, PUT_RES_FINAL);
                memset(PUT_RES_FINAL,0, sizeof(PUT_RES_FINAL));
                len_out_str = snprintf(PUT_RES_FINAL, sizeof(PUT_RES_FINAL),
                                       "%s,1.0,?\n", file);
                if(len_out_str>0)
                    writeWrap(_GUI.inp, (size_t) len_out_str, PUT_RES_FINAL);  //notify tx finished
            }
            exit(putOpRes);
}


int put_client(int connectedSocket,struct tx_config* tx_config1) {
    //handle files upload to server using PUT message..
    //init result returned from caller task,
    // PUT op result notified from forked task just before exiting
    char filename[MAX_FILENAME_SIZE];
    if(INTYPE!=GUI) {
        if (fgets(filename, MAX_FILENAME_SIZE, stdin) == NULL) {
            fprintf(stderr, "err reading file name to put\n");
            exit(EXIT_FAILURE);
        }
    } else{
        if(read_line_gui(filename, sizeof(filename))==RESULT_FAILURE){
            fprintf(stderr,"GUI ERR PUT \n");
            return RESULT_FAILURE;
        }
    }
    filename[strcspn(filename, "\n")] = 0;                  //substitute \n
    ///sending filename of file to PUT
    if(writeWrap(connectedSocket,MAX_FILENAME_SIZE,filename)==RESULT_FAILURE) {
        fprintf(stderr, "ERR WRITING TO SERVER FILE INFOS on socket :%d\n", connectedSocket);
        return RESULT_FAILURE;
    }
    ///getting filesize
    struct stat st;
    unsigned long fileSize;
    errno=0;
    if(stat(filename, &st)==-1){
        perror("accessing file to put infos...stat err :");
        long fileNotFound=ERR_FILENOTFOUND;
        fileNotFound=htobe64(fileNotFound);
        writeWrap(connectedSocket, sizeof(fileNotFound),&fileNotFound);
        return RESULT_FAILURE;
    }
    fileSize = (unsigned long) st.st_size;
    fileSize= htobe64(fileSize);
    ///sending filesize of file to PUT
    if(writeWrap(connectedSocket,sizeof(fileSize),&fileSize)==RESULT_FAILURE) {
        fprintf(stderr, "ERR WRITING TO SERVER FILE INFOS on socket :%d\n", connectedSocket);
        return RESULT_FAILURE;
    }
    fileSize=be64toh(fileSize);


    //// GET NEW SOCKET FOR PUT OP
    int socketOP;                                       //new socket for PUT operation
    if ((socketOP=getNewConnection_ClientSide(connectedSocket))==RESULT_FAILURE)
        exit(EXIT_FAILURE);

    printf("OBTAINED NEW CONNECTION FOR GET OP for filename %s on worker task PID:%d \n",filename,getpid());fflush(0);
    int pid=fork();
    ///put OP handled on new a process
    if(pid==0){
        _put(filename, fileSize, socketOP,tx_config1);
    } else if(pid==-1){
        fprintf(stderr,"err during fork\n");
        return RESULT_FAILURE;
    } else
        printf("newly created task with pid :%d to handle PUT of file %s \n",pid,filename);
    return RESULT_SUCCESS;
}

struct tx_config tx_configCreate(int connectedSocket){
    //take from UI configs for transimission
    //send them serialized to server and return that structure
    //TODO IF DEFINED MACRO IN NEXT LINE BASIC TX_CONFIGS TAKED BY MACRO
#ifdef TX_CONFIG_FIXED
    ///USE DEFAULT...
    const struct tx_config requestedTxConfigs= (struct tx_config)
            { .wsize=WINDOWSIZE, .extraBuffSpace=EXTRABUFRINGLOGIC,.maxseqN=(2 * (EXTRABUFRINGLOGIC+WINDOWSIZE)), .p_loss=P_LOSS};
#else
    struct tx_config requestedTxConfigs ;
    ///GETTING TX_CONFIGS FROM STDIN
    char _tmp_readBUF[5];
    char* endpntr;
    int winsize;
    double ploss;
    memset(_tmp_readBUF,0, sizeof(_tmp_readBUF));
    printf("\n");
    if(INTYPE!=GUI) {
        ///GETTIN WINSIZE
        fprintf(stderr,":>\tinsert min trasmission window size for file tx\n");
        if (fgets(_tmp_readBUF, 5, stdin) == NULL) {
            fprintf(stderr, "err reading input request\n");
            exit(EXIT_FAILURE);  //curr task  killed before any (wrong)interaction with server
            //server will timeout connection and kill servent task
        }
        _tmp_readBUF[strcspn(_tmp_readBUF,"\n")] = 0;            //substitute \n
        winsize = (int) strtol(_tmp_readBUF, &endpntr, 0);
        if (endpntr == _tmp_readBUF || *endpntr
            || winsize < 1) {            //input check
            fprintf(stderr, "\nMALFORMED INPUT INSERTED\n");
            exit(EXIT_FAILURE);
        }
        ///GETTIN PLOSS
        fprintf(stderr,":>\tinsert pck loss simulation  probability \n");
        memset(_tmp_readBUF, 0, sizeof(_tmp_readBUF));
        if (fgets(_tmp_readBUF, 5, stdin) == NULL) {
            fprintf(stderr, "err reading input request\n");
            exit(EXIT_FAILURE);                 //curr task will be killed before any interaction with server
            //server will timeout connection and kill servent task
        }
        _tmp_readBUF[strcspn(_tmp_readBUF,
                             "\n")] = 0;                  //substitute \n
        ploss = strtod(_tmp_readBUF, &endpntr);
        if (endpntr == _tmp_readBUF || *endpntr
                || ploss < 0 || ploss > 1) {            //input check
            fprintf(stderr, "\nMALFORMED INPUT INSERTED\n");
            exit(EXIT_FAILURE);
        }
    } else{ ///GUI
        //getting winsize
        if(read_line_gui(_tmp_readBUF, sizeof(_tmp_readBUF))==RESULT_FAILURE)
            exit(EXIT_FAILURE);
        winsize = (int) strtol(_tmp_readBUF, &endpntr, 0);
        //gettin ploss
        if(read_line_gui(_tmp_readBUF, sizeof(_tmp_readBUF))==RESULT_FAILURE)
            exit(EXIT_FAILURE);
        ploss = strtod(_tmp_readBUF, &endpntr);
    }
    requestedTxConfigs= (struct tx_config){ .wsize=winsize, .extraBuffSpace=EXTRABUFRINGLOGIC,
            .maxseqN=(2 * (EXTRABUFRINGLOGIC+winsize)), .p_loss=ploss};
#endif
    ///SERIALIZING TX CONFIG IN A TMP BUFF
    char serializedTxConfig[TX_CONGIS_SERIALIZED_SIZE];
    memset(serializedTxConfig,0, sizeof(serializedTxConfig));
    serializeTXConfigs(&requestedTxConfigs,serializedTxConfig);
    ///sending serialized tx_config demanded to server for file exchange
    if(writeWrap(connectedSocket,TX_CONGIS_SERIALIZED_SIZE,serializedTxConfig)==RESULT_FAILURE)
        exit(EXIT_FAILURE);     //directly exit on configuration handshake fail...
    return requestedTxConfigs;
}
