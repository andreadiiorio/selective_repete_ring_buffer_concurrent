// Created by andysnake on 07/08/18.
/// SENDER CONTROLLER
/*
 * SENDER THREADS WORKS AND SCOREBOARD INIT CODE
 * see server.h
 *
 * for server there're 3 threads:   consumer_sender,ackker,producer
 * :::RINGBUF COSTRAINTS ADAPTED FOR S.R. logic :::::
 * $)   producer is limited to produce until prev pos of sndbase
 *          (=> sndbase is first of a series of pck to retrasmit(potentially))
 *      it produce pcks ready to be sent in ringbuf
 * $$)  consumer_sender is limited by producer (wait ready to be sent pcks)
 *                                 and by sndbase+winsize-1 (S.R. SENDING COSTRAINT)
 *      it consume pcks produced from circular buffer and send them through socket (eventually resend on timer expire)
 * $$$) ackker
 *          it receive ack from socket,mark pcks in ringbuf (if not duplicated) and eventually move sndbase forward
 *                                                                        (lettin avaible space for more production :) )
 */

#ifndef SCOREBOARDSERVER_H
#define SCOREBOARDSERVER_H

#include "app.h"
#include "timers.h"
#define FAKEPCKLOSS                 //to enable pck loss simulation....

/*
 * main structure for SENDER
 * hold S.R. server window & protocol refs with a ringbuffer of PCKs
 * it hold timers,io stuff and  server threads too
 */
struct scoreboardSender{
    struct circularBuf* cbuf;           //circular buffer &co
    volatile Pck* sendBase;             //first pck unackked in server window (S.R. semantic)
    int maxSeqN;                        //up limit(not included) of seqN usable in a sended pck...
    int winsize;                        //sending window size...
    ///IO
    char filename[MAX_FILENAME_SIZE] ;
    int fd;
    unsigned int fileIn_BlockSize;
    unsigned long fileSize;             //filesize of file in sending...
    int sockfd;
    ///timers
    struct timers_scheduled timersScheduled;
    ///SENDER threads...
    pthread_t  producer;
    pthread_t  consumer_sender;
    pthread_t  consumer_ackker_th;
    ///statistics
    int totPckSent,totPckLosed,totAckRcvd;
#ifdef FAKEPCKLOSS
    double PCK_LOSS_PROB;               //probability for pck loss to debug application
#endif
} static *srdScoreboardSnder;           ///MODULE PNTR RESTRICTED FOR DEALLOCATION ON FAULT..
#define FILEIN_BLOCKSIZE 5              //producer max block of pck movement in ringbuff
//allocate and init server scorebord struct inputs by names; pck loss on future send will be simulated with loss_p probability
//returned RESULT MACRO to caller
struct scoreboardSender* initScoreboardSender(const char *fileName, unsigned long file_size, int socket,
                                              const struct tx_config *tx_configs);

//start server threads to send file gived in initScoreboardSender and wait joins, SEND OPERATION result returned
///entry point SENDER CONTROLLER
int startSender(struct scoreboardSender* scoreboard);

 //deallocate scoreboard... and close descriptors...
void deallocateScoreboardSender(struct scoreboardSender* scoreboardSend);

///ERRORS HANDLERS
void errExitHandler_Sender();               //handle termination of threads
void errSpreadHandler_Sender(int signo);    //handle termination spreading
////     :::: SENDER WORKER THREADS ::::      /////////////
void* ackker_thread(void* cbufPntr);            //ackker work..handle rsndbase
void* socket_sender_thread(void* cbufPntr);     //consumer work, spinlock on producer
void* fillPcks_thread(void* cbufPntr);          //producer work, spinlock on sndvbase

///SPINLOCK VARS
const unsigned int producerWaitTime=1;
const unsigned int consumerWaitTime=3;
#endif
