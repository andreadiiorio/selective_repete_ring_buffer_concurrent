#include "sender.h"
#include "PckFunctions.h"
#include "PcKFunctions.lc"
#include "timers.h"
#include "timers/timers.c"
#include "utils.h"
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/timeb.h>
#include "GUI/GUI.h"
///GUI links
char INTYPE;
struct gui_basic_link _GUI;
/*
 * core of sending pcks respecting S.R. semantics ,
 * sending handled with decoupling  socket's read & write op. with 2 different thread
 * main structure of server is scoreboardSender
 * see header for thread details
 */

void errSpreadHandler_Sender(int signo){
    fprintf(stderr,"sig:%d -sender controller- exiting task %d \n",signo,getpid());fflush(0);
    if(signo==SIGUSR1)                                  //err spread reached this task..
        errExitHandler_Sender();
    else if (signo==SIGALRM) {
        fprintf(stderr, "TIMEOUT ON SENDER WORK \n");
        exit(EXIT_FAILURE);
    }
}
///DEALLOCATION
pthread_mutex_t deallocationContention=PTHREAD_MUTEX_INITIALIZER;
void errExitHandler_Sender(){
    //on err occurred deallocate scoreboard and related JUST ONCE, then kill all threads and exit...
    int lockRes=0;
    /*
     * FIRST CALLER WILL ACQUIRE THE LOCK AND WILL DEALLOCATE STUFF, OTHER WILL BE PAUSED ON LOCK AND THEN KILLED BY EXIT
     */
    if((lockRes=pthread_mutex_lock(&deallocationContention))){
        fprintf(stderr,"lock error on deallocation...%s \n",strerror(lockRes));
    }
    deallocateScoreboardSender(srdScoreboardSnder);
    exit(EXIT_FAILURE);
}
struct scoreboardSender* initScoreboardSender(const char *fileName, unsigned long file_size,
                                              int socket,const struct tx_config *tx_configs){
    /*
     * allocate a new scoreboard server
     * will be allocated all nested strucutres by subcalls too
     * on fail returned NULL to caller on success pointer to scoreboard allocated
     */

    int fileDescriptor;
    if((fileDescriptor=Openfile(fileName,O_RDONLY))==RESULT_FAILURE){
        fprintf(stderr,"err opening %s \n",fileName);
        return NULL;
    }
    struct scoreboardSender* scoreboardSender1=(struct scoreboardSender*) malloc(sizeof(struct scoreboardSender));
    if(!scoreboardSender1){
        fprintf(stderr,"scoreboard allocating error\n");
        return NULL;
    }
    memset(scoreboardSender1->filename, 0,sizeof(fileName));
    strcpy(scoreboardSender1->filename, fileName);
    //circ buff init with windowSize + extra buf space for buf&ring logic
    scoreboardSender1->cbuf=circularBufInit((unsigned int) (tx_configs->wsize+tx_configs->extraBuffSpace ));
    if(!scoreboardSender1->cbuf){
        fprintf(stderr,"scoreboard ring buf malloc err\n");
        free(scoreboardSender1);
        return NULL;
    }
    //set redundant ref of maxseqN from scoreboard avoiding global var &backlinks
    // !To keep Pckfunction general 4 other version of S.R.!
    scoreboardSender1->cbuf->maxseqN=&scoreboardSender1->maxSeqN;
    if(!scoreboardSender1->cbuf)
        return NULL;
    ///io configs
    scoreboardSender1->fd=fileDescriptor;
    scoreboardSender1->sockfd=socket;
    scoreboardSender1->fileSize=file_size;
    scoreboardSender1->fileIn_BlockSize=FILEIN_BLOCKSIZE;               //max block of pck readable from file...
    scoreboardSender1->winsize=tx_configs->wsize;                  //this is max sendable costrain of sender_thread without ack
    scoreboardSender1->maxSeqN=tx_configs->maxseqN;                  //max seqN to use in pck//ack header
    scoreboardSender1->sendBase=&scoreboardSender1->cbuf->ringBuf[0]; //init sndbase with first pck in ring buf
    if(initTimers(scoreboardSender1->cbuf,&scoreboardSender1->timersScheduled,&scoreboardSender1->sendBase,&scoreboardSender1->totPckSent)==RESULT_FAILURE){
        fprintf(stderr,"err intializing timers...\n");
        free(scoreboardSender1->cbuf);
        free(scoreboardSender1);
        return NULL;
    }
    scoreboardSender1->PCK_LOSS_PROB=tx_configs->p_loss;

    srdScoreboardSnder=scoreboardSender1;       ///SET GLOBAL MODULE PNTR FOR DEALLOCATION ON FAULT
    srand48(96 + time(NULL));               //init rand generation
    printf("transmission configs : winsize :%d extrabuf in ring:%d maxseqN:%d \n",tx_configs->wsize,tx_configs->extraBuffSpace,tx_configs->maxseqN);
    return scoreboardSender1;
}

///SENDER controller entry function
int startSender(struct scoreboardSender* scoreboard){
    /*
     * calling thread  will start SENDER WORKER THREADS, and wait until all joined back
     * returned SEND OP RESULT...
     */
    int resultOP;                                                                           //operation result
    ////register signal handlers
    if(Sigaction(SIGALRM,errSpreadHandler_Sender)==RESULT_FAILURE ||   Sigaction(SIGUSR1,errSpreadHandler_Sender)==RESULT_FAILURE){
        fprintf(stderr,"SIGNAL HANDLER RECORDING ERROR \n");
        return RESULT_FAILURE;
    }
    ////start threads
    if( pthread_create(&scoreboard->producer,NULL,fillPcks_thread, (void *) scoreboard)!=0){
        fprintf(stderr,"producer  creataing error");
        resultOP= RESULT_FAILURE;
        goto exit;
    }
    if(pthread_create(&scoreboard->consumer_sender,NULL,socket_sender_thread, (void *) scoreboard) != 0){
        fprintf(stderr,"server    creating error");
        resultOP= RESULT_FAILURE;
        goto exit;
    }
    if(pthread_create(&scoreboard->consumer_ackker_th,NULL,ackker_thread, (void *) scoreboard) != 0){
        fprintf(stderr,"ack handler creating error");
        resultOP= RESULT_FAILURE;
        goto exit;
    }
    int* retvalGenThread;
    int threadRetErrCode;
    //joining...
    if((threadRetErrCode= pthread_join(scoreboard->producer,NULL))) {
        fprintf(stderr, "PRODUCER JOIN ERR\n %s__\n", strerror(threadRetErrCode));
        resultOP = RESULT_FAILURE;
        goto exit;
    }


    if((threadRetErrCode= pthread_join(scoreboard->consumer_ackker_th,
                                       (void **) &retvalGenThread))){
        fprintf(stderr,"ACKKER JOIN ERR\n %s__\n",strerror(threadRetErrCode));
        resultOP = RESULT_FAILURE;
        goto exit;
    }

    if((threadRetErrCode= pthread_join(scoreboard->consumer_sender,NULL))) {
        fprintf(stderr, "SENDER JOIN ERR\n %s__\n", strerror(threadRetErrCode));
        resultOP = RESULT_FAILURE;
        goto exit;
    }
    resultOP=*retvalGenThread;                                          //will be setted as SUCESS if at lest ackker retuend ok
    resultOP==RESULT_FAILURE?printf("fail acker..\n"):printf("acker OK!:)\n");

    exit:
        deallocateScoreboardSender(scoreboard);           //release resources..
        return resultOP;
}
void deallocateScoreboardSender(struct scoreboardSender* scoreboardSend){  //deallocate scoreboard's stuff

    close(scoreboardSend->fd);
    shutdown(scoreboardSend->sockfd,SHUT_RDWR);
    freeTimers(scoreboardSend->cbuf,&scoreboardSend->timersScheduled);
    free(scoreboardSend->cbuf->ringBuf);
    free(scoreboardSend->cbuf);
    free(scoreboardSend);
#ifdef PRINT_DEBUG
    fprintf(stderr,"DEALLOCATED SENDER SCOREBOARD ON :%d \n",getpid());
#endif
}

////    :::::::SERVER THREADS :::::::::::
void* fillPcks_thread(void* scoreboard){
    /*
     * producer of block of pcks... for circbuf server: read from file data and fill pcks in circbuf
     * !! always produced pcks from producer position , at least ,to prev position of SNDBASE
     * on EOF reached during file read mark 1 last pck, move produce index and die...
     *
     */
    struct scoreboardSender* scoreboardSend=scoreboard;
    struct circularBuf* cbuf=scoreboardSend->cbuf;
    int myIndx,sendbase_pos;					//sndbase and producer index in cbuf
    int productible=1;				            //ammount of pck productible in ring buf till prev consumer position
    int nextSeqNumProduce=0;                    //contain seq num for next pck to produce
    unsigned int numPckToWrite;                 //ammount of pck to write from file to cbuf
    off64_t bytesReaded;                        //will contain curr seek of file in read...
    int produceResult;
    for(;;){
        myIndx= cbuf->producerIndex;
        while((myIndx+1)%cbuf->dimension == scoreboardSend->sendBase->pck_index){
#ifdef PRINT_DEBUG
            DEBUG_PROB_PRINT("producer SPINLOCK\n");
#endif
            usleep(consumerWaitTime);
            sched_yield();
        }
        sendbase_pos=scoreboardSend->sendBase->pck_index;                 //get sndbase position in ringbuf
                                                                                //as upper limit of potential production
        //now at least 1 pck productible
        ///////     calculating ammount of pck fillable (by consumer position)
        //produce always until first position behind consumer,from actual position of producer
        //mod N range semantics... ammount of items in physically disjointed regions
        if(sendbase_pos<myIndx)					//behind me
            productible=(cbuf->dimension-1-myIndx) + (sendbase_pos);
        else if(sendbase_pos>myIndx)
            productible=sendbase_pos-myIndx-1;

        //write in ring buf a tradeoff of io buffering and producible amount of pcks
//        O.S. optimize larger ammount of read&write
        numPckToWrite = min(productible, scoreboardSend->fileIn_BlockSize);
        // tradeoff block reading from file & blocking server thread(waiting on producer index)
        ////evalutate readable data from file...may be less than numPckToWrite...
        /*bytesReaded = lseek64(scoreboardSenderGlbl->fd, 0, SEEK_CUR);
        if(bytesReaded==(off64_t)-1){
            fprintf(stderr,"seek\n");
            perror("seek err ");
            errExitHandler_Sender();
        }
        printf("readed %ld \n",bytesReaded);fflush(0);
        if(numPckToWrite*PCKPAYLOADSIZE>scoreboardSenderGlbl->fileSize-bytesReaded){         //round num pck to produce...
            fprintf(stderr,"\n!!!!!LAST Production\n");
            //round ammount of pck to read in accord with residue data to read in file
            numPckToWrite= (unsigned int) lrint(ceil((scoreboardSenderGlbl->fileSize - bytesReaded) / PCKPAYLOADSIZE));
        }*/
        //// write in ring buf
        produceResult = readFileIntoCircBuff(myIndx, scoreboardSend->fd, nextSeqNumProduce, numPckToWrite, cbuf);
        //update indexes ..
        cbuf->producerIndex=(cbuf->producerIndex+numPckToWrite)%cbuf->dimension;
        nextSeqNumProduce=(nextSeqNumProduce+numPckToWrite)%scoreboardSend->maxSeqN;

        if(produceResult==RESULT_FAILURE){
            fprintf(stderr,"err producing pck\n");
//            pthread_exit(&pthread_exit_failure);
            errExitHandler_Sender();
        }
        else if(produceResult==EOF_REATCHED){
            //marking EOF reached and updating prodindx

            //MARK 1 EXTRA PCK IN RINGBUF TO NOTIFY SENDER and ACKER THREAD of EOF REACHED and move forward
            (cbuf->ringBuf+cbuf->producerIndex)->flag_internal=EOF_REATCHED;	//marked pck...
            (cbuf->ringBuf+cbuf->producerIndex)->seqNum=FIN;                    //negative seqN=>EOF SENT THROUGH SOCKET :)
            /*PRODUCER CAN PRODUCE WITHOUT PROBLEM IN HIS POSITION BUT CANNOT MOVE IF NEXT OF HIM IS CONSUMER
              if he don't move but produce result will never be visible to consumer ...
              */
            while (scoreboardSend->sendBase->pck_index== (cbuf->producerIndex+1)%cbuf->dimension) {
                usleep(producerWaitTime);
//                DEBUG_PROB_PRINT("PRODUCER EXITING....\n");
            }//loop exited=> filler can now move foreward
            //move forward produce index to let consumer see last marked pck...
            cbuf->producerIndex = (cbuf->producerIndex+1)%cbuf->dimension;
            break;
        }
    }

    ////producer exiting ..
//    fprintf(stderr,"\n\nProducer end!!!!!!!!!!!!!!!!!!!!!!\n");
    pthread_exit((void *) &pthread_exit_success);
}

void* socket_sender_thread(void* scoreboard){

    /* COSUMER_sender
     * send thread of pck taken from cbuf to client
     * it hold a private version of consumer index,global version is handled from ackker th(for S.R.window constraints)!
     * send pcks by producer index and sendingSemaphore,
     *      ->(if there's 1 pck ready to send and enought space in sendingwindow (semaphore...)
     * sending start from next of sender_consumer_index in cbuf (when there's not contention on a Pck in ringBuf)
     */
    struct scoreboardSender* scoreboardSend=scoreboard;
    struct circularBuf* cbuf=scoreboardSend->cbuf;
    int maxseqN=scoreboardSend->maxSeqN;

    /* this private consumer index indicate the last pck sent (for the fist time) on socket
     * it's always limited to be < producer >= global consumer */
    int sender_consumerIndex=cbuf->dimension-1;
    int sendWin_limit_up;                                 //upper limit of sending windows (S.R.)
    bool eof_reached=false;                              //flag for eof reached in cbuf..end is near
    bool winfull;                                        //sending window full condition
    int consumable = 1;                                 //# pck ready to be consumed to consume
    int numPckToConsume=1;                              //final num of pck to consume

    const int POLLRATE=11;                          //default timers poll:= 1/POLLRATE of #iterations_tot
                                                    //extra poll on winfull only if not already done...
    for(int j=1;;j=(j+1)%POLLRATE) {
        if(!j)  //polling may lock ackker thread, polling evalutated only sometimes, default poll on 0
            pollTimers(scoreboardSend->sockfd,cbuf,&scoreboardSend->timersScheduled,sender_consumerIndex);
        //get snd window up limit by sendbase
        sendWin_limit_up = (scoreboardSend->sendBase->pck_index + scoreboardSend->winsize ) % cbuf->dimension;
        winfull=(sendWin_limit_up==(sender_consumerIndex+1)%cbuf->dimension);                 //win full detect condition

        //wait the producer has filled from file (at least 1) sendable pck
        if (cbuf->producerIndex != (sender_consumerIndex + 1)%cbuf->dimension) { //at least 1 pck ready 2 be sent  !
            if(!winfull){
                //at least 1 pck sendable on send.win
                ////calculating consumable ammount of pck, checking by ready pcks and S.R. window
//                if (cbuf->producerIndex > sender_consumerIndex) {
//                    consumable = cbuf->producerIndex - 1 - sender_consumerIndex;
//                }
//                else if (cbuf->producerIndex < sender_consumerIndex) {
//                    consumable = cbuf->dimension - 1 - sender_consumerIndex + cbuf->producerIndex;
//                }
//                 evaluting consumable as tradeoff S.R window,block send,sendable in ring buff...
//                numPckToConsume = min(consumable, *avaibleOnWin);

                numPckToConsume = 1;
                int pck_1_st_toSendIndex = (sender_consumerIndex + 1) % cbuf->dimension;  //consumer from next position

#ifdef FAKEPCKLOSS  //here pck loss simulation occur
                int numPckToConsume_prev=numPckToConsume;
                for(int z=0;z<numPckToConsume;z++) {
                    if (probabilityHandler(scoreboardSend->PCK_LOSS_PROB)) {
#ifdef PRINT_DEBUG
                        fprintf(stderr, "\n\n!!  pck loss simulated on pck:%d \n", (pckToSend_1st->seqNum + z) % maxseqN);
#endif
                        scoreboardSend->totPckLosed++;                                              //statistics
                        int pck_indx = (z + pck_1_st_toSendIndex) % cbuf->dimension;
                        struct timer_pck* pck_timer=&(cbuf->ringBuf+pck_indx)->timerPck;
                        numPckToConsume--;               //pck loss simulation
                        // schedule timer  for loss simulated pck on pck_indx
                        if (startTimer(pck_timer, &scoreboardSend->timersScheduled) == RESULT_FAILURE) {
                            fprintf(stderr, "starting pck timer err\n");                        //err->propagated
                            errExitHandler_Sender();
                        }
                    }
                }
                int loss_simulated=numPckToConsume_prev-numPckToConsume;
                //update server index of loss simulated
                sender_consumerIndex= (sender_consumerIndex+loss_simulated)%cbuf->dimension;
                pck_1_st_toSendIndex=(pck_1_st_toSendIndex+loss_simulated)%cbuf->dimension;
#endif
                ////send pcks through socket
                if (sendPckThroughSocket(pck_1_st_toSendIndex, numPckToConsume, scoreboardSend->sockfd,cbuf,
                                                                     &scoreboardSend->timersScheduled)==RESULT_FAILURE){
                    fprintf(stderr,"err in send on socket has occurred\n");
                    errExitHandler_Sender();
                }
                scoreboardSend->totPckSent+=numPckToConsume;    //statistics

                //update server index
                sender_consumerIndex= (sender_consumerIndex+numPckToConsume)%cbuf->dimension;
                //eof sent detection...
//                int lastWrittenIndex=(sender_consumerIndex-1)%cbuf->dimension;
//                if(lastWrittenIndex<0)
//                    lastWrittenIndex=cbuf->dimension-1;
                continue;                                                                     //not evalutate resting...
            }
        }
        //else server blocked right behind producer  -> also win may be full...
        if(winfull){
#ifdef PRINT_DEBUG
                printf("WIN FULL...prdIndx%d \t me %d \t akker %d, \n",cbuf->producerIndex,sender_consumerIndex,cbuf->consumerIndex);fflush(NULL);
#endif
                if(j)                           //extra poll if not already did before in this iteration
                    pollTimers(scoreboardSend->sockfd,cbuf,&scoreboardSend->timersScheduled,sender_consumerIndex);
                /////sleep till next pck expire...
                struct timespec sleeptime;
                if(reamaining_timers(&sleeptime,cbuf,&scoreboardSend->timersScheduled)==RESULT_FAILURE)
                    errExitHandler_Sender();
                nanosleep(&sleeptime,NULL);
            usleep(consumerWaitTime);   //rest
        } else                                                                             //only block 4 slow producer
            usleep(consumerWaitTime);   //HERE SENDER WILL WAIT PTHREAD_CANCEL FROM ACKKER(ON FIN RCV)...

#ifdef PRINT_DEBUG
        if(probabilityHandler(0.04))
            printf("...prdIndx%d \t me %d \t akker %d, up_limit_sending_win %d\n",cbuf->producerIndex,sender_consumerIndex,cbuf->consumerIndex,sendWin_limit_up);fflush(NULL);
#endif
    }
}


int ackHandle(unsigned int seqN,struct scoreboardSender* scoreboardSnd) {
    /*
     * handle ack confirming in ringbuf , update sendBase
     * return ammount of consecutively confirmed pcks
     */
    int skippableOut=0;
    //getting pck to confirm pointer in cbuf ...
    int pckToConfirmIndx=seqN%scoreboardSnd->cbuf->dimension;
    Pck* pckToConfirm=scoreboardSnd->cbuf->ringBuf + pckToConfirmIndx;
    ////////     --duplicate Ack detection--         ////////
    //is a New ack received if in [sendbase,sendbase+windowsize) in MOD_MAXSEQN and not ACKKED FLAGGED
    //otherwise ack is ignored...duplicate or outofrange..
    int sndBase=scoreboardSnd->sendBase->seqNum;
    //not duplicate pck boundaries
    int highestSqN= (sndBase+scoreboardSnd->winsize-1)%(scoreboardSnd->maxSeqN);//highest seqN in actual sending window
    bool seqNInNewRange=false;
    //getting if ack seqN is in snd window range...
    N_IN_RANGE_MOD_N(sndBase,highestSqN,scoreboardSnd->maxSeqN,seqN,seqNInNewRange);

    //duplicate ack condition -> ACK ALREADY SEEN OR NOT IN SENDING WINDOW SEQN SPACE...
    if(!seqNInNewRange || pckToConfirm->flag_internal==ACKKED)
    {
#ifdef PRINT_DEBUG
        fprintf(stderr,"!DUPLICATE ACK? %d but sndbase %d  || already ackked flag..\n",seqN,sndBase);fflush(0);
#endif
        return 0;
    }
     pckToConfirm->flag_internal=ACKKED;                //set ack flag,nb sent pck=>produced pck => safe updatable...

#ifdef RETRASMISSION
     if(stopTimer(&pckToConfirm->timerPck,&scoreboardSnd->timersScheduled)==RESULT_FAILURE){
         fprintf(stderr,"err in stop \n");
         return RESULT_FAILURE;
     }
#endif

    ///////         SENDING WINDOW MOVING           ///////
    if(seqN==scoreboardSnd->sendBase->seqNum) {      //WINDOW HAS TO MOVE FOREWARD AT LEAST OF 1 PCK
        volatile Pck* futureSndBase=scoreboardSnd->sendBase; //retriving new sendbase or next pck unackked in sending window
        //searching from sendbase  next pck UNAKKED checking in  linked ringbuffer pcks flags
        while (futureSndBase->flag_internal == ACKKED){
            skippableOut++;                                                    //ackked pck there=>skippable
            futureSndBase = futureSndBase->nextPck;                            //get next pck in sending window to check
            //next pck to evalutate in ringBuf is in production=> not sent => UNAKKED =>founded, in this case pck is in production(prev ring cycle ackked)
            if (futureSndBase->pck_index == scoreboardSnd->cbuf->producerIndex) {
                //PCK IN PRODUCTION=>NOT YET FULLY WRITTED,OBSOLETE SEQN=>MANUALY UPDATE FROM PREV PCK EVALUTATED  :)
                //flag may be in production,server thread will not send until producer move (so othre ack will not come)
#ifdef PRINT_DEBUG
                printf("in production pck..> sndbase->>-producer...\n");fflush(0);
#endif
                break;                                                          //NEXT IN PRODUCTION=>NOT ACKKED
            }
        }
        scoreboardSnd->sendBase=futureSndBase;   //  UPDATE SENDBASE PCK POINTER...
        if(futureSndBase->seqNum==FIN){
            return EOF_REATCHED;
        }
    }
    return skippableOut;
}

void* ackker_thread(void* scoreboard) {
    /*
     * thread that read from socket ack from client...
     * on ack received disble associated timers and update pointers related...
     * and notify server if there's enought space in sendWindow to send new pcks ...
     */
    struct scoreboardSender *scoreboardSend = scoreboard;
    struct circularBuf *cbuf = scoreboardSend->cbuf;
    int cbufSize = cbuf->dimension;
    int ackTmp = 0;
    int winSkippable;                                               //ammount of pck moved forward in sending window
    unsigned int seqNum;
    int readRes;
    //gui vars for string sending...
    char increment_str[MAX_FILENAME_SIZE + 5];
    int len;
    for (;;) {
        ////RECEIVE ACK from receiver on socket
        readRes = readWrap(scoreboardSend->sockfd, SERIALIZED_ACK_SIZE,
                           &ackTmp);
        if (readRes == RESULT_FAILURE) {
            fprintf(stderr, "err occurred reading for an ack on socket\n");
            errExitHandler_Sender();
        }
        scoreboardSend->totAckRcvd++;                                                           //statistics
        seqNum = ntohl((uint32_t) ackTmp);
        ///EXIT CONDITION=>FIN ACK RCVD => ALL RECVD FROM RECEIVER
        if (seqNum == FIN) {
            printf("\r ACKKED 100%% \n");
            fflush(0);
            goto ack_th_exit;
        }
        ////Move sending window foreward if ack==sndbase
        winSkippable = ackHandle(seqNum, scoreboardSend);
        if (winSkippable == RESULT_FAILURE) {
            fprintf(stderr, "handling ack has encountered an error\n");
            errExitHandler_Sender();
        }
        static int pckConfirmedTot;
        pckConfirmedTot += winSkippable;             //DEBUG statistics and UI light update
#ifdef PRINT_DEBUG
        printf("skippable..:%d <-> seqN %d ackked %d bytes\n",winSkippable,seqNum, pckConfirmedTot*PCKPAYLOADSIZE);fflush(NULL);
#endif
#ifndef TEST_QUIET_PRINT
        printf("\r ACKKED %f of tot ", (double) (pckConfirmedTot * PCKPAYLOADSIZE)/scoreboardSend->fileSize);
#endif
        if (INTYPE == GUI) {    ///GUI notify
            errno=0;
            off64_t bytesWritten;
            bytesWritten = lseek64(scoreboardSend->fd, 0, SEEK_CUR);
            if(bytesWritten==(off64_t)-1){
                fprintf(stderr,"seek\n");
                perror("seek");
                INTYPE = CLI;            //GUI FAIL RESET TO CLI MODE...
                continue;

            }
            memset(increment_str, 0, sizeof(increment_str));
            /// create & send a formatted string for GUI PROGRESS UPDATE
            len = snprintf(increment_str, sizeof(increment_str),
                           "%s,%f\n", scoreboardSend->filename, bytesWritten/scoreboardSend->fileSize);
            if (len < 0)
                fprintf(stderr, "SNPRINTF ERR\n");
            else {
                if (writeWrap(_GUI.inp, (size_t) len, increment_str) ==
                    RESULT_FAILURE)
                    INTYPE = CLI;            //GUI FAIL RESET TO CLI MODE...
            }
        }
    }
    ack_th_exit:
        pthread_cancel(scoreboardSend->consumer_sender);    //kill sender when last ack reacged
    if(INTYPE==GUI){
        memset(increment_str,0, sizeof(increment_str));
        int len = snprintf(increment_str, sizeof(increment_str),
                           "%s,1.0,?\n", scoreboardSend->filename);
        writeWrap(_GUI.inp, (size_t) len, increment_str); //send to gui last increment
    }
    pthread_exit((void *) &pthread_exit_success);    //EXITING
}

