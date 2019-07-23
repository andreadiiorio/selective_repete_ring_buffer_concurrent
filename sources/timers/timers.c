/*
 * straighforeward implementation of A.VARGHESE IEEE paper
 * checking among timers navigating in a simple linked list of pcks...
 */
#include "timers.h"
#include "app.h"
#include "PckFunctions.h"
#include "../PcKFunctions.lc"
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>


//init timers structures propagated result...
int initTimers(struct circularBuf* cbuf,struct timers_scheduled* timersScheduled,volatile Pck** firstScheduledPck,int*totPckSent){
    /*
     * init timers structures from time setting obtained with a syscall relied from CALLING PROCESS!
     *          ONLY CALLING TASK CAN USE TIMERS INITIATED HERE !
     * return MACRO RESULT_SUCCESS OR FAIL
     */
    clockid_t clock_id = 0;
    //preset critical structures to 0
    memset(&timersScheduled->timeout_adaptive,0, sizeof(timersScheduled->timeout_adaptive));
    memset(&timersScheduled->timeout_adaptive,0, sizeof(timersScheduled->timeout_adaptive));
    int getclock_res;
    if ((getclock_res = clock_getcpuclockid(getpid(), &clock_id))!=0) {            //set clock id
        fprintf(stderr, "clock init fail ... %d\n",getclock_res);
        return RESULT_FAILURE;
    }
    pthread_mutex_init(&timersScheduled->mutex, NULL);              //init mutex for separation of timer (dis) arm
    struct sigevent segv;
    memset(&segv,0, sizeof(segv));                                                  //init 2 0
    segv.sigev_notify = SIGEV_NONE;
    //1shot initialize timers constant fields
    struct timer_pck *timerPck;
    Pck* pckRingBuff=cbuf->ringBuf;
    for (int x = 0; x < cbuf->dimension; x++) {
        timerPck = &pckRingBuff->timerPck;
        timerPck->flag=DISARMED;
        if (timer_create(clock_id, &segv,&timerPck->timer_id) == -1) {    //setup posix timer
            perror(" ttimer_create ");
            return RESULT_FAILURE;
        }
        pckRingBuff=pckRingBuff->nextPck;
    }
    //assign fistScheduledPck(timer) as pointer to pointer to sendbase
    (timersScheduled->firstScheduledPck)=firstScheduledPck;
    timersScheduled->totPckSent=totPckSent;                               //set statistics referement
    ///TIMEOUT CONFIGS
    //default 40 millisec
    timersScheduled->timeout_adaptive.it_value.tv_nsec=START_TIMEOUT_NANO;
    timersScheduled->timeout_adaptive.it_value.tv_sec=START_TIMEOUT_SEC;
    //default buondaries for timeout adaption
    //low
    timersScheduled->TIMEOUT_MIN.tv_sec=RAIL_MIN_TIMEOUT_SEC;
    timersScheduled->TIMEOUT_MIN.tv_usec=RAIL_MIN_TIMEOUT_USEC;
    //high
    timersScheduled->TIMEOUT_MAX.tv_usec=RAIL_MAX_TIMEOUT_USEC;
    timersScheduled->TIMEOUT_MAX.tv_sec=RAIL_MAX_TIMEOUT_SEC;
    timersScheduled->time_adaption_counter=0;

    return RESULT_SUCCESS;
}


//relase allocated timers structures...
int freeTimers(struct circularBuf* cbuf,struct timers_scheduled* timersScheduled){
//    struct timer_pck* timers=timersbean->timers;
    struct timer_pck* timer;
    for (int y=0;y<cbuf->dimension;++y){
        timer=&((cbuf->ringBuf+y)->timerPck);               //get related timer pointer
        timer_delete((timer)->timer_id);                    //delette posix timers related resources...
    }
    pthread_mutex_destroy(&timersScheduled->mutex);
}



double timerDivision(struct itimerspec *num, struct itimerspec *den) {
    //evalutate division of num/den of timers values of input parameters
    if(num->it_value.tv_sec==0 && den->it_value.tv_sec==0)
        return ((double) num->it_value.tv_nsec/den->it_value.tv_nsec);
    double n=num->it_value.tv_sec,d=den->it_value.tv_sec;   //reling var init with secs if non zeros
    const int BILION=1000000000;
    n+=((double) num->it_value.tv_nsec/BILION);
    d+=((double) den->it_value.tv_nsec/BILION);
//    printf("num :%ld :%ld -> %f\n",num->it_value.tv_sec,num->it_value.tv_nsec,n);
    return n/d;
}

int adaptionUpTot=0,adaptionDownTot=0;                  //TODO DEBUG STATISTICS
void timeout_adaptation(struct timers_scheduled* scheduledStruct,struct itimerspec* remainingTimer){
 //adaption timer heuristic, adaptive timer val may be changed;timers lock has to be taken
 //if remaining time is NOT NULL, will be executed adaption down heuristic else up adaption
 //TIMERS LOCK HAS TO BE TAKEN

#ifdef TIMEOUTFIX       ///no adaption => FIXED TIMEOUT
    return;
#endif
    //useful shorter pointers from timers scheduled structure
    struct timeval* TIMEOUT_MAX=&scheduledStruct->TIMEOUT_MAX;
    struct timeval* TIMEOUT_MIN=&scheduledStruct->TIMEOUT_MIN;
    ///make a tmp copy of current adaptive timeout on a tmp var compatible with timerspec MACROs
    struct timeval timeval1tmp;
    timeval1tmp.tv_sec=scheduledStruct->timeout_adaptive.it_value.tv_sec;
    timeval1tmp.tv_usec=lrint(floor(scheduledStruct->timeout_adaptive.it_value.tv_nsec/1000));

    ///recognizing adaption kind looking remaining timer parameter
    //gived reamining time => timer stopped => not expired timer => adaption down
    if(remainingTimer!=NULL)        ///ADAPTION DOWN
    {
        if(timercmp(&timeval1tmp,&scheduledStruct->TIMEOUT_MIN,==))
            return;                         ///ALREADY TIMER AT MIN FAST EXIT
        /*
         * adaption Down-> timer decrement as a f(remainingTime)  on stopped timer
         * considering remining time / current adaption timer value
         * will be decremented counter
         * 1 time if remaining time/ current timer <= .3
         * 2 time if remaining time/ current timer <= .6
         * else 3 time
         * in that way larger reamining time on stopped timer(indicating ack fast received)
         * will decrement more adaptive timer then smaller remaining time
         */
        scheduledStruct->time_adaption_counter--;       //one at least always
        double residueTimePercentual=timerDivision(remainingTimer,&scheduledStruct->timeout_adaptive);
        if(residueTimePercentual>0.6)
            scheduledStruct->time_adaption_counter--;
        if (residueTimePercentual>0.9)
            scheduledStruct->time_adaption_counter--;

    }
    else                            ///ADAPTION UP
        scheduledStruct->time_adaption_counter++;

    if(scheduledStruct->time_adaption_counter>=ADAPTATION_DOWN_THRESHOLD &&
            scheduledStruct->time_adaption_counter<=ADAPTATION_UP_THRESHOLD)
        return;     ///DO NOT UDAPTE TIMER IF NOT REACHED ANY THREASHOLD...

    if(scheduledStruct->time_adaption_counter<=ADAPTATION_DOWN_THRESHOLD)
    {    ///ADAPTION DOWN -> reduce timeout
        timersub(&timeval1tmp,&ADAPTIONDOWN,&timeval1tmp);
        ///ADJUST ADAPTED TIMEOUT WITH SETTED RAILs...
        if(timercmp(&timeval1tmp,TIMEOUT_MIN,<)) {
            timeval1tmp=*TIMEOUT_MIN;
        }
        scheduledStruct->time_adaption_counter=0;
        adaptionDownTot++;                                //statistics
    }
    else if (scheduledStruct->time_adaption_counter>=ADAPTATION_UP_THRESHOLD)
    {    ///ADAPTION UP -> increment timeout
        timeradd(&timeval1tmp,&ADAPTIONUP,&timeval1tmp);
        if(timercmp(&timeval1tmp,TIMEOUT_MAX,>))
            timeval1tmp=*TIMEOUT_MAX;                   //with MAX threshold
        scheduledStruct->time_adaption_counter=0;
        adaptionUpTot++;                              //statistics
    }
    /// SET ADAPTION RESULT ON SHARED ADAPTIVE TIMEOUT
    // reconverting timeout from timeval to timespec
    scheduledStruct->timeout_adaptive.it_value.tv_sec=timeval1tmp.tv_sec;
    scheduledStruct->timeout_adaptive.it_value.tv_nsec=timeval1tmp.tv_usec*1000;
    //if(adaptionUpTot %20==0 || adaptionDownTot%20==0)
    //    printf("adaption up and down tot : %d %d \n",adaptionUpTot,adaptionDownTot);

}



int startTimer(struct timer_pck*timer,struct timers_scheduled* timersScheduled){
    /*start a timer related to pck
     * will be used adaptive timeout inside timersScheduled
     * if started timer before sending => ack cannot arrive before => usable without mutex serializzation
     * called by server thread by sendPckThroughSocket to handle trasmission and retrassmission at the same way
     */

    //schedule pck timer
    errno=0;
    if(timer_settime(timer->timer_id, 0, &timersScheduled->timeout_adaptive, NULL)==-1){        //arming timer..
        perror("\n\n(re)starting timer ");
        return RESULT_FAILURE;
    }
    timer->flag=ARMDED;
    return RESULT_SUCCESS;
}

//stop a timer related to pck
int stopTimer(struct timer_pck*timer,struct timers_scheduled*  timersScheduled){
    /*
     * stop timer and mark it has disarmed
     * ackker_receiver thread will call this function, on ack received ,pck related timer has to be disarmed
     * will be done timeout adaption according to remain time...
     */
    struct itimerspec timerDisarm,remaining_time;      //disarmer value, and container for time still left in timer
    memset(&timerDisarm,0, sizeof(struct itimerspec));
    memset(&remaining_time,0, sizeof(struct itimerspec));
    timerDisarm.it_value.tv_nsec=0;
    timerDisarm.it_value.tv_sec=0;                          //set vars for disarming...

    int errCode;
    pthread_mutex_t* mutex=&timersScheduled->mutex;
    //lock to safe del and stop timer from scheduled ones
    if((errCode= pthread_mutex_lock(mutex))){
        fprintf(stderr,"mutex lock err\n err :%s\n",strerror(errCode));
        return RESULT_FAILURE;
    }//entered in critcal section


    if(timer->flag==DISARMED){
        fprintf(stderr,"!already disarmed timer \t .. DUPLICATE ACK?\n");
        goto stopTimerExit;
    }
    //disarm posix timer  related and get reamining time
    if(timer_settime(timer->timer_id,0,&timerDisarm,&remaining_time)==-1){
        perror("disarming timer err... ");fflush(0);
        pause();
        return RESULT_FAILURE;
    }
    timer->flag=DISARMED;
    timeout_adaptation(timersScheduled,&remaining_time);    ///ADAPTE TIMEOUT DOWN
    //finally unlock...
    stopTimerExit:
    if((errCode=pthread_mutex_unlock(mutex))){
        fprintf(stderr,"mutex lock err\n err :%s\n",strerror(errCode));
        return RESULT_FAILURE;
    }
    return  RESULT_SUCCESS;
}

int reamaining_timers(struct timespec* remaining_dst,struct circularBuf*cbuf,struct timers_scheduled* scheduled){
    //return into remaining_dst pointed buff the rest of time of first timer in scheduled linked list of timers :)
    //minus an epslin to wakeup right before timeout
    //usually one of the nearest to expire
    struct itimerspec remainingTimer1;
    //get first scheduled timer has indexconsumer timer related to pck == sendbase
    volatile struct timer_pck* head= &(*scheduled->firstScheduledPck)->timerPck;
    const short espilon_down=965;                           //nanosec subtracted to remain time to wakeup in time for expire

    errno=0;
    if(timer_gettime(head->timer_id,&remainingTimer1)==-1){  //getting timer value
        perror("accessing timer err..");
        return RESULT_FAILURE;
    }
    //copy remaining time in dest addr
    remaining_dst->tv_sec=remainingTimer1.it_value.tv_sec;
    remaining_dst->tv_nsec=max(0,remainingTimer1.it_value.tv_nsec-espilon_down);
    return RESULT_SUCCESS;
}

int expiredTimers=0;    //TODO COUNTER DEBUG
int pollTimers(int socket,struct circularBuf* cbuf,struct timers_scheduled* timersScheduled,int lastPckIndexToCheck){
    /* iterating among scheduled timers
     * resend and relink timers expired...
     * server thread will call this function
     */
    volatile struct timer_pck* timer_nodei;                                    //i_th timer scheduled
    pthread_mutex_t* mutex=&timersScheduled->mutex;
//    int schedulated,schedulableTimers_max=cbuf->dimension-EXTRABUFRINGLOGIC;    //S.R. gived window...
    int errCode,i=0;
    struct itimerspec currVal;                                                 //will contain remaining time for timers

    errno=0;
    //lock before read timer and evalutate resending
    if((errCode= pthread_mutex_lock(mutex))){
        fprintf(stderr,"mutex lock err\n err :%s\n",strerror(errCode));
        return RESULT_FAILURE;
    }   //critical section

    volatile Pck* pck=*timersScheduled->firstScheduledPck;                              //get first pck scheduled for rtx
    //init first scheduled timer pos=consumer=ackker=sndbase
    while(pck->pck_index!=lastPckIndexToCheck) {
        //iterate among timers between first scheduled and last gived, checking expired ones
        timer_nodei = &pck->timerPck;
        if (timer_nodei->flag != DISARMED && pck->flag_internal!=ACKKED){    //disarmed timer => NOT SCHEDULED
            //get pck and related timer
            if (timer_gettime(timer_nodei->timer_id, &currVal) == -1) {  //getting timer value
                perror("accessing timer err..");
                return RESULT_FAILURE;
            }
            if (currVal.it_value.tv_sec == 0 && currVal.it_value.tv_nsec == 0) {       ///expired timer...>resend..
                if (sendPckThroughSocket(pck->pck_index, 1, socket, cbuf,timersScheduled) == RESULT_FAILURE) {
                    fprintf(stderr, "retrasmit fail..\n");
                    return RESULT_FAILURE;
                }
#ifdef DEBUG_PRINT
                fprintf(stderr,"expired timer at %d , resending.. tot ammount:%d\n",pck->pck_index,++expiredTimers);
#endif
                timeout_adaptation(timersScheduled,NULL);    ///ADAPTING TIMEOUT UP
                (*timersScheduled->totPckSent++);                                       //update pck sent counter by ref
            }
        }
        pck=pck->nextPck;                                                       //check nexty
   }
    //unlock to let ackker thread to stop next timers
    if((errCode=pthread_mutex_unlock(mutex))){
        fprintf(stderr,"mutex lock err\n err :%s\n",strerror(errCode));
        return RESULT_FAILURE;
    }
    return RESULT_SUCCESS;
}

