#ifndef PRJNETWORKING_UTILS_H
#define PRJNETWORKING_UTILS_H

#include <sys/queue.h>
#include <netinet/in.h>
#include "app.h"


struct file_ll{
    /*
     * file node of linked list of SLIST In queue.h
     */
    char filename[MAX_FILENAME_SIZE];
    unsigned long filesize;
    SLIST_ENTRY(file_ll) links;               //1 linked list  ref
};


/*
 * list current dir items and store basic info in a singel linked list of file_ll nodes
 * return RESULT_FAILURE ON ERR or ammount of files in dir
 */
#define FILES_LLIST_TYPE files_linked            //linked list as struct type
SLIST_HEAD(FILES_LLIST_TYPE,file_ll);            //define linked list type
int listCurrDir(struct FILES_LLIST_TYPE *head);

void errExit(int signo);
int SocketWrap();

//bind socket to specified inputs return macro RESULT_*
int BindWrap(int sockfd,int port,in_addr_t ADDR);

//dissolving connection from socket to know new client port for new connected socket
//return RESULT_* macro indicating exit result
int dissolveConnection(int socket);

//int connection3Handshake_serverSide(int socket);
//int connection3Handshake_clientSide(struct sockaddr_in*servBoundedaddr);

/*
 *  GET NEW CONNECTED SOCKET FOR OP. API CLI-SERVER SIDE
 *  client will create a new socket and bind with wildcard port to obtain a new port bounded to this socket
 *  this new address for the newly created socket will be notified to server using old connected socket cli<-->server
 *  server will connect a new socket to this newly client socket prev. created and send a connection confirm code (RESULT_SUCCES MACRO)
 *  in both case will be returned the new connected socket (client and server side) or RESULT_FAILURE
 */
int getNewConnection_ClientSide(int connectedSocket);           //get a new socket connected to server or err code
int getNewConnection_ServerSide(int connectedSocket);           //get a new socket connected to client or err code

typedef void Sigfunc(int signo);                                    //sigfunc proto
int Sigaction(int signo, Sigfunc *func);
//sig child handler, will unzombies all dead chields
void sig_chld(int signo);

///TX CONFIGS (DE) SERIALIZE
//SERIALIZE reqested tx configs in buff pointed by serializedTmpBuffPntr

void SwapBytes(void *pv, size_t n,char dstEndianess,char srcEndianess);
void serializeTXConfigs(const struct tx_config *requestedTxConfigs,void* serializedTmpBuffPntr);
//DESERIALIZE tx configs from tmp buffer into client_tx_config
void deserializeTXConfigs(struct tx_config *client_tx_config,void* _serializedTxTmpBuffPntr);

bool probabilityHandler(double p);
#define  DEBUG_PROB_PRINT(msg)\
    if(probabilityHandler(0.01))\
        fprintf(stderr,msg)
/*
 * stopwatch MACROs to get elapsed time of an operation with simply call START AND STOP
 * meaning by name of used timevals vars in STOPWATCH_INIT
 * result will be printed on stdout
 */
///TMP VAR NAMES MACROS
#define STOPWATCH_START_VAR startime
#define STOPWATCH_STOP_VAR stoptime
#define STOPWATCH_DELTA_VAR delta
///STOP WATCH BASIC OP MACROS
#define STOPWATCH_INIT struct timeval STOPWATCH_START_VAR,STOPWATCH_STOP_VAR,STOPWATCH_DELTA_VAR;
#define STARTSTOPWACHT(startime)\
    printf("starting stopwatch \n");\
    gettimeofday(&(startime),NULL);

#define STOPWACHT_STOP(startime,stoptime,delta)\
    gettimeofday(&(stoptime),NULL);\
    timersub(&(stoptime),&(startime),&(delta));\
    fprintf(stderr,"\n\n Elapsed secs and micros: %ld , %ld \n\n",(delta).tv_sec,(delta).tv_usec);fflush(0);


#define TX_CONGIS_SERIALIZED_SIZE (sizeof(int)+sizeof(int)+sizeof(int)+sizeof(double))
//MACRO TO PRINT TX CONFIG FROM A PNTR
#define TX_CONFIG_PRINT( tx_config1)\
    printf("TX_CONFIG: WSIZE %d \t MAX SEQN %d \t EXTRA RING BUF SPACE %d \t LOSS PROBABILITY %f \n",\
        (tx_config1)->wsize,(tx_config1)->maxseqN,(tx_config1)->extraBuffSpace,(tx_config1)->p_loss);



#endif //PRJNETWORKING_UTILS_H
