/*
 * RECEIVER MODULE HEADER
 * client work is structured in 2 thread, producer,consumer.
 *
 * $) CONSUMER write on file pcks from the ringbuffer,
 *    it has to spinlock to rcvbase(first pck not received)
 *    it can work in range (consumerIndx,rcvbase-1]
 *
 * $$) PRODUCER receive pck from socket, evaluate if it's duplicate
 * 	  and pong ack related to server .
 * 	  To not overlap consumer,producer has to evaluate if pck received
 * 	  fall in the consuming window(consumer,rcvbase) too...
 *    In this case producer has to spinlock to consumer until it move forward
 */
#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>
#include "app.h"
//kinds of pck receptible according actual client window seqNumebers
#define DUPLICATE 11                    //in [rcvbase-winsize,rcvbase-1]
#define INWINDOW	10                  //in [rcvbase,rcvbase+winszie)
#define OUTOFRANGE 12                   //otherwise...larger cbuf used

//receiver main structure to hold pcks,tx configs, io related and thread istances
struct scoreboardReceiver{
    ////basic logic 4 S.R.
    struct circularBuf* cbuf;
    int maxseqN;
    int winSize;                        //rcv win size
    volatile Pck* rcvbase;              //first missing pck in receive window
    ////io-&-net
    char filename[MAX_FILENAME_SIZE] ;
    unsigned long int fileSize;    //file size in reception...
    int fileOut_BlockSize;          //max ammount of pck writable from cbuf to file
    unsigned long int fileByteWritten;  //byte writed o
    unsigned long int fileByteReceived;
    int socket;                              //related socket
    int fd;                                  //related file  in transmission
    ////FLAG FOR EXIT
    volatile bool eof_reached;                       //shared flag of eof reached during write...
    volatile bool finRcvdFlg;                        //shared flag of fin rcvd from server
    char pckTmpBuffer[SERIALIZED_SIZE_PCK];//tmp buffer for receive 1 pck
    ////thread workers
    pthread_t producer; //pck_receiver_th;
    pthread_t consumer; //file_filler_th;
    pthread_t ackker; //ack_sender_th;

} static *srdScoreboardRcv;    /// module pntr restricted for deallocation of scoreboard on fault...

#define FILEOUT_BLOCK_SIZE 3    //consumer block movement size in ringbuf
//init scoreboard & related, will be returned pointer to the scoreboard or NULL if err has occurred,global pointer also setted
struct scoreboardReceiver* initScoreboardReceiver(char* filename, unsigned long filesize,
                                                  int socket,const struct tx_config* tx_configs);


//start client threads, wait joins and return RECEIVE OP. result to caller
///entrypoint RECEIVER CONTROLLER
int startReceiver(struct scoreboardReceiver* scoreboard);

///DEALLOCATIONS
//relase client scoreboard resources ..
void deallocateScoreboardReceiver(struct scoreboardReceiver *scoreboard); //deallocate scoreboard and tmp buff... and close descriptors...


//ERR HANDLER -> DEALLOCATIONS  JUST ONCE AND EXIT --USING GLOBAL SCOREBOARD PNTR-
pthread_mutex_t deallocationContention_rcv=PTHREAD_MUTEX_INITIALIZER;
void errExitHandler_rcv();
void errSpreadHandler_Recv(int signo);



////receiver worker threads
void* fileFiller(void* ringBuf);    //consumer, spinlock to rcvbase=producer...
void* pck_receiver(void* ringBuf);  //producer, spinlock to consumer...

///SPINLOCK VARS
const unsigned int producer_sleeptime=3;
const unsigned int consumer_sleeptime=1;

#endif //CLIENT_H
