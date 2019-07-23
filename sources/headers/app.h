#ifndef APPH
#define APPH
/*
 * MAIN APP HEADER, most useful definition of macros and structures
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
///STANDARD EXIT RESULT FOR FUNCTION TO PROPAGATE ERR OR SUCESS TO CALLER
#define RESULT_FAILURE (-1)
#define RESULT_SUCCESS (1)


///CONNECTION COSTANTS
//binding addr  //REDUCE TO LOOPBACK IN DEBUG DEFINING ONLYLOOPBACK
#ifdef ONLYLOOPBACK                             //reduce binding to loopback
    #define DEFAULT_ADDRBIND INADDR_LOOPBACK
#else
    #define DEFAULT_ADDRBIND INADDR_ANY         //DEFAULT BIND INTERFACE
#endif
#define _SERV_PORT   5300                    //well known server port
#define _SERV_ADDR_BIND DEFAULT_ADDRBIND    //server main socket bind addr
#define _CLI_ADDR_BIND DEFAULT_ADDRBIND     //client new socket per op bind addr

const char* _dfl_servAddress="127.0.0.1";	//address on default command line input
#define REQUSTCONNECTION 96
#define MAXRANDOMSERVER 255
#define TIMEOUTCONNECTION 150

///MESSAGES CLI<->SERV
typedef char controlcode;			//to wrap MSGs excanged between client<->server
///control messages
/*
 * CONTROL MESSAGES are data preceded by a control code witch identify the client request
 * for each possible request there's one of the next 3 macros
 */
#define PUT_REQUEST_CODE        1
#define GET_REQUEST_CODE        2
#define LIST_REQUEST_CODE       3
#define DISCONNECT_REQUEST_CODE 0
#define TERMINATE_TXs_AND_DISCONNECT 4
char help[]="\n->PUT \t\t 1\n->GET \t\t 2\n->LIST \t\t 3\n->DISCONNECT \t 0\n";

/// answer messages
#define ERR_OCCURRED            (-100)      /*generic err occurred during OP*/
#define ERR_FILENOTFOUND        (-101)      /*file ID not mapped to any file in files_sendable folder on server*/
#define ERR_INVALID_INPUT       (-102)
#define ERR_INIT                (-103)      /*generic err occurred during init_serv side OP*/
//re used RESULT_SUCESS for positive result notification
#define READY2START             55
///FILE EXCHANGE
#ifndef PCKPAYLOADSIZE
#define PCKPAYLOADSIZE 1000				                    //byte of payload for pck...
#endif
#define SERIALIZED_SIZE_PCK ( sizeof(int) + PCKPAYLOADSIZE)
#define SERIALIZED_ACK_SIZE ( sizeof(int))                         //ack only seq num sended
///end of work macros
#define EOF_REATCHED (-64)         //to indicate End reached in file read, used to mark pck in ring buf
#define FIN	(-69)					//seqN to notify to receiver file transfer has ended... out of number space

///DEBUG MACROS
#define RETRASMISSION      99                //to enable pck retrasmission on timer expire..

///DEFAULT TX CONFIG ON DEFINED NEXT MACRO..
//#define TX_CONFIG_FIXED
#define WINDOWSIZE 10
//extra buff support consumer time to move..
#define EXTRABUFRINGLOGIC (1+4)              //extra space for win size because of cbuf semantics->disambiguity
#define P_LOSS 0
/*
 * to hold basic configs for trasmission
 */
struct tx_config{
    int wsize;                          //S.R. WIN SIZE (= # PCK UNACKKED ON FLY)
    int maxseqN;                        //MAX SEQ N FOR EACH PCKs
    int extraBuffSpace; //extra buf space in ringbuff to let concurrent thread to have more avaible space to move during dead times
    double p_loss;                  /// DEBUG PROBABILITY OF PCK LOSS
};
struct timer_pck{
    timer_t timer_id;                           //posix timer id related to pck
    char flag;                                  //timer flag  disambiguity posix timer,
};

struct pck {
    struct pck* nextPck;                //point to next pck in cbuf
    int seqNum;
    int pck_index;                               //index  pck in ringbuf (not seqN)
    volatile char flag_internal;                //may be ackked,unakked or EOF_REATCHED...
    char data[PCKPAYLOADSIZE];         //data in computer endianess
#ifdef RETRASMISSION
    struct timer_pck timerPck;
#endif
};
#define Pck struct pck
///PCKS FLAGS
///receiver
#define RECEIVED 96                    //single time received pck
//MULTIRECEIVED all VALS >= 96  indicating ammount of repetition received...
#define WRITTED   95                //pck payload written to file    or empty(first circualr iteration)
///sender
#define ACKKED 99
#define UNACKKED 11
#define MAX_FILENAME_SIZE   40  /*max size of a filename usable on app*/
#define PREFIX_SIZE         6
char* prefix_server="serv_";
char* prefix_client="cli_";

///DEFINITION OF RING BUFFER STRUCT WIDELY USED IN APP
struct circularBuf{
    int dimension;
    int* maxseqN;                   //reference to max sequence num in scorebord of client || server
    Pck* ringBuf;                            //data as pcks
    //indexes of producer and consumer
    volatile int consumerIndex;
    volatile int producerIndex;
};

//ring buff allocation
struct circularBuf* circularBufInit(unsigned int cbufSize);

///usefull macros...
#define	min(a,b)	((a) < (b) ? (a) : (b))
#define	max(a,b)	((a) > (b) ? (a) : (b))
#define	SA	struct sockaddr	//2 typecast short

#define N_IN_RANGE(start,ends,n) \
    (start)<=(n) && (n)<=(ends)

//set inRangeBool with true if n is in [lowBoundary,highBoundary] MOD circularSize...
#define N_IN_RANGE_MOD_N(lowBoundary,highBoundary,circularSize,n,inRangeBool)\
    if ((lowBoundary) <= (highBoundary)) {                  /*range contigue*/ \
        (inRangeBool) = N_IN_RANGE(lowBoundary, highBoundary, n); \
    } else  /*lowst>highest => disjoint range*/ \
    { (inRangeBool) = N_IN_RANGE(lowBoundary, (circularSize)-1, n)|| N_IN_RANGE(0, highBoundary, n);}

///global return value for threads..
const int pthread_exit_success=RESULT_SUCCESS;
const int pthread_exit_failure=RESULT_FAILURE;
//TODO DEBUG PRINTs TRIGGER a lot of print are disabled if not def this macro
//#define PRINT_DEBUG


typedef void Sigfunc(int signo);                                    //sigfunc proto


#endif
