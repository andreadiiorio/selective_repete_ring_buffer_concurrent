#include "receiver.h"
#include <pthread.h>
#include "PckFunctions.h"
#include "PcKFunctions.lc"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "utils.h"

//see header for thread works description...
void errSpreadHandler_Recv(int signo){
    fprintf(stderr,"SIG %d -receiver controller - exiting task %d \n",signo,getpid());fflush(0);
    if(signo==SIGUSR1)
        errExitHandler_rcv();
    else if (signo==SIGALRM) {
        fprintf(stderr, "TIMEOUT \n");  //TODO UI PIPE..
		exit(EXIT_FAILURE);
    }
}
///DEALLOCATION
pthread_mutex_t deallocationContention_rcv=PTHREAD_MUTEX_INITIALIZER;
void errExitHandler_rcv(){
	//on err occurred deallocate scoreboard and related JUST ONCE, then kill all threads and exit...
	int lockRes=0;
	/*
     * FIRST CALLER WILL ACQUIRE THE LOCK AND WILL DEALLOCATE STUFF, OTHER WILL BE PAUSED ON LOCK AND THEN KILLED BY EXIT
     */
	if((lockRes=pthread_mutex_lock(&deallocationContention_rcv))){
		fprintf(stderr,"lock error on deallocation...%s \n",strerror(lockRes));
	}
	deallocateScoreboardReceiver(srdScoreboardRcv);
	exit(EXIT_FAILURE);
}

struct scoreboardReceiver* initScoreboardReceiver( char* filename,unsigned long filesize,
												  int socket,const struct tx_config* tx_configs){
	//init client scoreboard and related,return pointer to allocated scoreboard (or NULL if err)
	///opening & creating new file
//	remove(filename);							//TODO OVERWRITE EXISTING FILENAME?
	int fp;
	errno=0;
	fp = open(filename,O_WRONLY|O_CREAT,S_IRWXU);
	if(fp<0){
		perror("open write and create...");
		fprintf(stderr,"OPEN ERR OCCURRED\n");
		return NULL;
	}
	struct scoreboardReceiver* scoreboardRcv=calloc(1,sizeof(*scoreboardRcv));
	//buffer extended for ringbuffer semantics and server extra buf needs
	scoreboardRcv->cbuf=circularBufInit((unsigned int) (tx_configs->wsize + tx_configs->extraBuffSpace));             //ring buffer
    if(!scoreboardRcv->cbuf){
        fprintf(stderr,"invalid cbuf init..\n");
        free(scoreboardRcv);
		return NULL;
    }
	scoreboardRcv->fd=fp;
	scoreboardRcv->socket=socket;
	scoreboardRcv->rcvbase=scoreboardRcv->cbuf->ringBuf;           //RCVBASE PCK pntr init with first pck
	//exit flag init at false
	scoreboardRcv->eof_reached=false;
	scoreboardRcv->finRcvdFlg=false;
	//set redundant ref of maxseqN from scoreboard avoiding global var
	// To keep Pckfunction general 4 other version of S.R.
	scoreboardRcv->cbuf->maxseqN=&scoreboardRcv->maxseqN;
	scoreboardRcv->maxseqN=tx_configs->maxseqN;
	scoreboardRcv->winSize=tx_configs->wsize;
	scoreboardRcv->fileSize=filesize;
	scoreboardRcv->fileOut_BlockSize=FILEOUT_BLOCK_SIZE;

	srdScoreboardRcv=scoreboardRcv;			///SET GLOBAL MODULE PNTR FOR DEALLOCATION ON FAULT
	srand48(96 + time(NULL));               //init rand generation
	printf("transmission configs : winsize :%d extrabuf in ring:%d maxseqN:%d \n",tx_configs->wsize,tx_configs->extraBuffSpace,tx_configs->maxseqN);
	return scoreboardRcv;
}

int startReceiver(struct scoreboardReceiver* scoreboard){
	/*set var opts and start client threads, caller thread will wait here until workers joins
	 * try extend kernel socket receive buffer
	 * returned RECEIVE OPERATION result to caller
	 */
	int resultOP;														//operation result returned to caller
	///extend OS SOCKET RCV BUFF SIZE
    int extendedUDPRCVBUFFSIZE=2336160;
    if(setsockopt(scoreboard->socket,SOL_SOCKET,SO_RCVBUF,&extendedUDPRCVBUFFSIZE,sizeof(int))<0){
        perror("sock option buff extend fail..");
        resultOP=RESULT_FAILURE;
        goto exit;
    }
    STOPWATCH_INIT;
	STARTSTOPWACHT(STOPWATCH_START_VAR);

    ///sighandlers
    Sigaction(SIGUSR1,errSpreadHandler_Recv);
	Sigaction(SIGALRM,errSpreadHandler_Recv);
    //// START RECEIVER THREADS
    if( pthread_create(&scoreboard->producer,NULL,pck_receiver, (void *) scoreboard)){
        fprintf(stderr,"producer  creataing error");
		resultOP=RESULT_FAILURE;
		goto exit;
    }
    if(pthread_create(&scoreboard->consumer,NULL,fileFiller, (void *) scoreboard) != 0){
        fprintf(stderr,"server    creating error");
		resultOP=RESULT_FAILURE;
		goto exit;
    }

    int* retvalGenThread;
    int threadRetErrCode;
    ///joining...
    if((threadRetErrCode= pthread_join(scoreboard->consumer, (void **) &retvalGenThread))) {
        fprintf(stderr,"CONSUME  JOIN ERR\n %s__\n",strerror(threadRetErrCode));
		resultOP=RESULT_FAILURE;
		goto exit;
    }
    resultOP=*(retvalGenThread);										//set operation result from consumer(filefiller return val)
	if((threadRetErrCode= pthread_join(scoreboard->producer, (void **) &retvalGenThread))) {
		fprintf(stderr, "PRODUCER JOIN ERR\n %s__\n", strerror(threadRetErrCode));
		resultOP=RESULT_FAILURE;
		goto exit;
	}

	STOPWACHT_STOP(STOPWATCH_START_VAR,STOPWATCH_STOP_VAR,STOPWATCH_DELTA_VAR);

	exit:
		deallocateScoreboardReceiver(scoreboard);
		return resultOP;

}

void deallocateScoreboardReceiver(struct scoreboardReceiver *scoreboard) {  //deallocate scoreboard and tmp buff... and close descriptors...
	close(scoreboard->fd);
//	shutdown(scoreboard->socket,SHUT_RDWR);					//socket close elsewhere
	free(scoreboard->cbuf->ringBuf);
	free(scoreboard->cbuf);
	free(scoreboard);
#ifdef PRINT_DEBUG
	fprintf(stderr,"DEALLOCATED RECEIVER SCOREBOARD ON :%d \n",getpid());
#endif
}

////CLIENT THREADS....
// consumer thread
void* fileFiller(void* scoreboard) {
		/*
		 * SPINLOCK ON RCVBASE
		 * flush consecutive Pck data to file from cbuf until fileseek==filesize
		 * will be consumed always pck in range [consumer_index+1,rcvbase)
		 */


	struct scoreboardReceiver* scoreboardRcv=scoreboard;
	struct circularBuf* cbuf=scoreboardRcv->cbuf;
    int cbSize=cbuf->dimension;
	int firstPckIndexToConsume,consumable=1;                           //default file consumable ammount...
	int myindex;
	int rcvbaseindx;
	int writeRes;
	unsigned int numPckToWriteInFile;									//will contain ammount of pck to flush from cbuf
	int fin=FIN;
	fin = htonl(fin);													//serialized fin ack to echo back to server
	Pck* rcvbase_curr;
	for(;;){
		myindex=cbuf->consumerIndex;
		//TODO STAIL DATA MAY OCCUR CAUSING ONLY LONGER SPINLOCK WAIT
//		rcvbaseindx=scoreboardRcv->rcvbase->pck_index;
		rcvbase_curr=__sync_add_and_fetch(&scoreboardRcv->rcvbase,0);	//ATOMIC RCVBASE VAL TAKE
		rcvbaseindx=rcvbase_curr->pck_index;

		////SPINLOCK CONSUMER
		while ((myindex+1)%cbSize==rcvbaseindx){  // consumable win "empty"
//            producerIndx=cbuf->producerIndex;//old...

//			rcvbaseindx=scoreboardRcv->rcvbase->pck_index; //TODO OLD VERSION
			rcvbase_curr=__sync_add_and_fetch(&scoreboardRcv->rcvbase,0);	//ATOMIC RCVBASE VAL TAKE
			rcvbaseindx=rcvbase_curr->pck_index;
#ifdef PRINT_DEBUG
			DEBUG_PROB_PRINT("CONSUMER SPINLOCK\n");
#endif
			sched_yield();
            usleep(consumer_sleeptime);
        }
		////    -CONSUMABLE AMMOUNT-    ////
		if(rcvbaseindx>myindex)
			consumable=rcvbaseindx-myindex-1;    //rcvbase next to me-> consumable all until first before him
		else 					//rcvbase behind me(or on me)->consumable until end of cbuf and continue untill first before him
			consumable=cbuf->dimension-1-myindex+rcvbaseindx;
		//todo debug 1 consumable
		numPckToWriteInFile = (unsigned int) min(consumable, scoreboardRcv->fileOut_BlockSize);
		//TODO DEBUG
//		fprintf(stdout, "CONSUMABLE %d consumer :%d \n",consumable,myindex);fflush(0);
		if(consumable<1){
		    fprintf(stderr,"cbufdimension %d \n",cbuf->dimension);
			errExitHandler_rcv();
		}
		//>
		firstPckIndexToConsume = (myindex + 1) % cbuf->dimension;            //start consuming from next
		//consume...
		writeRes=writeFileFromCircBuf(firstPckIndexToConsume,scoreboardRcv->fd,numPckToWriteInFile,scoreboardRcv->fileSize,cbuf);
		if(writeRes==RESULT_FAILURE){
			fprintf(stderr,"consuming pck data from cbuf into file err\n");
			errExitHandler_rcv();                 //all die
		}
		else if(writeRes==EOF_REATCHED){        //when has been written last byte of the received file -> exit :)
			//EOF CONDITION ACHIVED IF WRITTEN ON FILE FILE SIZE
			scoreboardRcv->eof_reached=true;                 //flag eof reached during write...

            writeWrap(scoreboardRcv->socket,SERIALIZED_ACK_SIZE,&fin);	///ECHO BACK FIN ACK
            pthread_cancel(scoreboardRcv->producer);         //cancel cosnumer thread
			pthread_exit((void *) &pthread_exit_success);
		}
		myindex=(myindex+numPckToWriteInFile)%cbuf->dimension;   //update consumer index..
//		cbuf->consumerIndex=myindex;
		__sync_val_compare_and_swap(&cbuf->consumerIndex,cbuf->consumerIndex,myindex);	//TODO ATOMIC UPDATE

	}
}

int pck_rcvd=0;	//TODO DEBUG COUNTER

short pckHandler(int seqN, struct pck *pckInCbuf, struct scoreboardReceiver *scoreboardl) {
	/*
	 * ani pck with seqN
	 * return MACROS if it's DUPLICATE,OUTOFRANGE,INWINDOW
	 * if respectivelly it's a duplicate pck, pck with seqN out of bounds..
	 * sendback ack in all case different to OUTOFRANGE
	 * or if it's a newly correctly received pck..
	 */
	bool seqNInDuplicateRange,seqInRcvWin;
	int rcvBaseSeqN,seqNInRangeHigh;										//in range of rcv win boundaries
	int lowestSqNDuplicate,highestSqNDuplicate;												//pck seqN duplicate boundaries
	////////     --duplicate pck detection--       ////////////////
	seqNInDuplicateRange = false,seqInRcvWin=false;							//pck in -windows- flags
	rcvBaseSeqN = scoreboardl->rcvbase->seqNum;                    //actual rcvbase seqN
	//duplicate pck boundaries [rcvbase-winsize,rcvbase-1]
	lowestSqNDuplicate = (rcvBaseSeqN - scoreboardl->winSize)
						 % (scoreboardl->maxseqN);
	if (lowestSqNDuplicate < 0)    //C module function for a=(a/b)(b)+a%b...
		lowestSqNDuplicate = scoreboardl->maxseqN + lowestSqNDuplicate;
	highestSqNDuplicate = (rcvBaseSeqN - 1) % (scoreboardl->maxseqN);
	if (highestSqNDuplicate < 0)
		highestSqNDuplicate = scoreboardl->maxseqN + highestSqNDuplicate;
//	printf("duplicate boundaries %d-%d\n", lowestSqNDuplicate, highestSqNDuplicate);fflush(0);
	//getting if seqN is in  duplicate range...
	N_IN_RANGE_MOD_N(lowestSqNDuplicate,highestSqNDuplicate,scoreboardl->maxseqN,seqN,seqNInDuplicateRange);
	//duplicate pck fall in receive window
	if (pckInCbuf->flag_internal > RECEIVED) {
		pckInCbuf->flag_internal = max(254, pckInCbuf->flag_internal + 1);   //increment duplicate counter in pck...
		fprintf(stderr, "duplicated pck,received %d times at least\n", pckInCbuf->flag_internal - RECEIVED);fflush(0);
		seqNInDuplicateRange=true;
	}
	//// seqN in rceive win [rcvbase,rcvbase+winsize)
	seqNInRangeHigh=(rcvBaseSeqN+scoreboardl->winSize-1)%scoreboardl->maxseqN;
	N_IN_RANGE_MOD_N(rcvBaseSeqN,seqNInRangeHigh,scoreboardl->maxseqN,seqN,seqInRcvWin)
//	printf("boundaries in range %d--%d duplicate %d-%d\n",rcvBaseSeqN,seqNInRangeHigh,lowestSqNDuplicate,highestSqNDuplicate);fflush(0);
	if(!seqInRcvWin && !seqNInDuplicateRange){				//pck out bounds...
		fprintf(stderr,"!!!!out of bounds seqN..:%d -> %d\n",seqN,pckInCbuf->pck_index);fflush(0);
		return OUTOFRANGE;
	}
	////	--ackback--		////
	int seqNSerialized=htonl(seqN);
	if (writeWrap(scoreboardl->socket, sizeof(int), &seqNSerialized) == RESULT_FAILURE) {
		fprintf(stderr, "ACK BACK ERR \n");
		errExitHandler_rcv();
	}        // sent back ack...

	if(seqNInDuplicateRange){		//ignore data of duplicate pcks...
#ifdef PRINT_DEBUG
		fprintf(stderr,"pck duplicate seqN %d -> %d ",seqN,pckInCbuf->pck_index);fflush(0);
#endif
		return DUPLICATE;
	}
	return INWINDOW;
}

//producer
void* pck_receiver(void* scoreboard) {
	/*
	 *PRODUCER WORK, receive pck from socket in unpredictible order
	 * in [rcvbase,rcvbase+winsize) in Range mod cbuf->size
	 * ack back server, and move rcvbase in case its seqN has been received
	 */
//    bool eof_reached=false;   TODO ???  return  EOF if received EOF MARKED PCK FROM SOCKET(SEQN<0 = EOF_REACHED MACRO)...
	struct scoreboardReceiver* scoreboardRcv=scoreboard;
	int cbufSize = scoreboardRcv->cbuf->dimension;
	int seqN,pckPositionInRingBuff;
	Pck *pckInCbuf;
	int receiveRes;
	int consZoneLow,consZoneHigh;                               //zone in consuming boundary boundaries

	int skippable;												//skippable pck from rcvwindow
	int rcvBaseSeqN;
	volatile Pck * futureRcvBase;								//next rcvbase in case of rcv win moving forward
	bool pck_in_consuming_zone;                                 //whill hold SPINLOCK bool CONDITION FOR PRODUCER
		/*
		 *                      -CONSUME ZONE-
		 * consumer may fall in receive window
		 * if pck_index is in (consumer_index,rcvbase) range mod (cbuf->dimension)
		 * so problematic zone is [consumer_index+1,rcvbase+winsize) range mod (cbuf->dimension)
		 * may be overwritten pck in consume( or next to be)
		 * so it's necessary to wait consumer move forward the window
		 */
	void *pck;											  //tmp buffer to hold 1 received pck from socket
	pck = malloc(SERIALIZED_SIZE_PCK);                    //TODO FREE WHERE?
	void* pcktmp=pck;
//	pck=&scoreboardClientGlbl->pckTmpBuffer;
	if(!pck){
		fprintf(stderr,"tmp pck err malloc\n");
		errExitHandler_rcv();
	}
	for (;;) {              //producer work
		/// receive 1 pck
		pcktmp = pck;
		receiveRes = readWrap(scoreboardRcv->socket, SERIALIZED_SIZE_PCK, pcktmp);	//TODO BOTTLENECK RECEIVER?
		//TODO DEBUG
		if (cbufSize != scoreboardRcv->cbuf->dimension) {
			fprintf(stderr, "mismatch ringbuef overwritten\n");
			errExitHandler_rcv();
		}
		if (receiveRes == RESULT_FAILURE) {
			fprintf(stderr, "pck reception  err\n");
			errExitHandler_rcv();
		}
		memcpy(&seqN, pcktmp, sizeof(seqN));                                //get pck seqN
		pcktmp += sizeof(seqN);                                            //move buff pntr copy to pck DATA
		seqN = ntohl(seqN);                                                 //deserilize seqN from data received
		if (seqN == FIN) {                                                    //rcvd FIN... echo back when received all data
//			fprintf(stderr, "received FIN from server,\n");
			scoreboardRcv->finRcvdFlg = true;
			continue;                        //may be necessary wait other pcks
		}
		pckPositionInRingBuff = (seqN) % cbufSize;                            ///indexing received pck in cbuf
		pckInCbuf = scoreboardRcv->cbuf->ringBuf + pckPositionInRingBuff;
#ifdef PRINT_DEBUG
		printf("received pck:%d #:%d \n", seqN, ++pck_rcvd);fflush(0);
#endif
		//todo debug print
		if (pckInCbuf->pck_index != pckPositionInRingBuff) {
			fprintf(stderr, "!!!!!pck index mismatch\n");
			errExitHandler_rcv();
		}//
		short pck_type = pckHandler(seqN, pckInCbuf, scoreboardRcv);    //ignore data of duplicate pcks or out of ranges...
		if (pck_type == DUPLICATE)
			continue;
		else if (pck_type == OUTOFRANGE)
			continue;
		//else INRANGEPCK received...
		////SPINLOCK WITH CONSUMER condition check
		pck_in_consuming_zone = false;            //in consuming zone = (consumer,rcvbase)
		int consIndex;
		consIndex = __sync_add_and_fetch(&scoreboardRcv->cbuf->consumerIndex, 0);//ATOMIC READ
//		consIndex = scoreboardRcv->cbuf->consumerIndex;			//TODO STAIL DATA MAY BE READED
		consZoneLow = 	(consIndex) % cbufSize; //consumer act work position
		consZoneHigh = 	(scoreboardRcv->rcvbase->pck_index - 1) % cbufSize;
		if (consZoneHigh < 0) {
			consZoneHigh = cbufSize - 1;
		}                                            //mod n carry back
		//pck in consuming win = [cosumer+1,rcvbase) low boundary move as fast as consumer...painfully slow
		N_IN_RANGE_MOD_N(consZoneLow, consZoneHigh, cbufSize, pckPositionInRingBuff, pck_in_consuming_zone);
//		printf("consumer %d consumer zone boundaries %d <-> %d bool (pck in consWin):%s \n",scoreboardRcv->cbuf->consumerIndex,consZoneLow,consZoneHigh,pck_in_consuming_zone?"true":"false");fflush(0);
		while (pck_in_consuming_zone) { //RING FULL
#ifdef PRINT_DEBUG
			DEBUG_PROB_PRINT("!SPINLOCK PRODUCER old pck in consuming....\n");
#endif
			usleep(producer_sleeptime);     //rest...
			sched_yield();

			consIndex = __sync_add_and_fetch(&scoreboardRcv->cbuf->consumerIndex, 0);//ATOMIC READ
//		consIndex = scoreboardRcv->cbuf->consumerIndex;			//TODO STAIL DATA MAY BE READED
			consZoneLow = 	(consIndex) % cbufSize; 		//update cons act work position
			N_IN_RANGE_MOD_N(consZoneLow, consZoneHigh, cbufSize, pckPositionInRingBuff, pck_in_consuming_zone);
		}//exit => safe writable in pck position (because consumer has moved || not there)
		///			data copy in c.buf
		pckInCbuf->seqNum = seqN;											   //4 future uses...
		pckInCbuf->flag_internal = RECEIVED;                                  //mark has single time received pck
		memcpy(&pckInCbuf->data, pcktmp, PCKPAYLOADSIZE);
		//////     - - - - -    	receiver window moving check		  - - - - -      /////
		if (pckPositionInRingBuff == scoreboardRcv->rcvbase->pck_index) {
			//calculate skip amount of producer in ring buf
			skippable = 0;                          //ammount of pck consecutive received ( # pck rcvbase move foreward)
			futureRcvBase = scoreboardRcv->rcvbase;
			rcvBaseSeqN = scoreboardRcv->rcvbase->seqNum;                          //take old rcvbase seqN
			//iterating among received(>=1times) pcks in client win
			while (futureRcvBase->flag_internal >= RECEIVED && futureRcvBase->flag_internal != FIN) {
				skippable++;
				//update next pck to check if will be rcvbase
				futureRcvBase = futureRcvBase->nextPck;
				if (futureRcvBase->pck_index == scoreboardRcv->cbuf->consumerIndex) {
					//TODO MUST BE NEXT RECV BASE, UNSAFE TO BE READED(MAY BE IN CONSUME...)
#ifdef PRINT_DEBUG
					fprintf(stderr, "!!!new rcvbase  in consuming...\n ");fflush(0);
#endif
					break;
				}
			}
			//todo debug
			if(skippable>scoreboardRcv->winSize){
				fprintf(stderr,"INVALID SKIPPABLE PROBABLY :%d \n",skippable);fflush(0);
				errExitHandler_rcv();
			}
			//update futurercvbase SeqN 4 next duplicate checks
			futureRcvBase->seqNum = (rcvBaseSeqN + skippable) % scoreboardRcv->maxseqN;
			__sync_val_compare_and_swap(&scoreboardRcv->rcvbase,scoreboardRcv->rcvbase,futureRcvBase);	//ATOMIC UPDATE
//			scoreboardRcv->rcvbase = futureRcvBase;
#ifdef PRINT_DEBUG
			printf("::>seqN->%d new rcvbase %d at %d ,skippable %d \n", seqN, futureRcvBase->seqNum, futureRcvBase->pck_index, skippable);fflush(0);
#endif
		}
	}
}

