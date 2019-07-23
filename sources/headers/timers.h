/*
 * this module using posix timers API wrap multiple timers op.s like: schedule, stop, check
 * also can be returned remaining time of first pck's timer scheduled in ringbuf
 * timers structures are binded inside pcks structure
 * ( only if entity require retrasmission if defined RETRASSMISION MACRO )
 * to avoid the use of signals in a multithread environment during posix timers
 * are set to not deliver signals. with poll are checked timers running inside sending
 * window looking the remaining time and handling retrassmission
 * a lock is used to let different thread to handle stop and start&poll timers
 *  by acker &  sender thread
 */
#ifndef TIMERS_H
#define TIMERS_H

#include <pthread.h>
#include <time.h>
#include <bits/time.h>
#include <sys/time.h>
#include <sys/queue.h>
///TIMERS FLAG TO DISNGUISH DISARMED TIMER vs EXPIRED TIMER
#define DISARMED    97
#define ARMDED      99

//running timers structure
struct timers_scheduled{
    pthread_mutex_t mutex;                  //mutex to separate (de)- schedule of timers ... on different thread
    int running;
    volatile Pck** firstScheduledPck;                // fist pck (timer) scheduled index in cbuf
    ///timeout
    struct itimerspec timeout_adaptive;         //accessed with mutex exclusione from server & ack_receiver threads...
    short time_adaption_counter;                //used in timeadaption euristic
    struct timeval TIMEOUT_MAX,TIMEOUT_MIN;     // adaptive timeout boundaries
    //statics
    int* totPckSent;
};



//start a pck timer return MACRO RESULT_SUCCES OR FAIL
int startTimer(struct timer_pck*timer,struct timers_scheduled* timersScheduled);

//stop a pck timer return MACRO RESULT_SUCCES OR FAIL
int stopTimer(struct timer_pck*timer,struct timers_scheduled*  timersScheduled);

///TRIGGER ADAPTION FROM THIS MACRO DEFINITION!!
//#define TIMEOUTFIX  1//IF NOT DEFINED TRIGGER TIMEOUT ADAPTION
///TIMEOUT ADAPTION HEURISTIC
/*
 * timer adaptation euristic, timer adaption will be done adding/subbing a costant from adaptive timer
 * adaptiveTimerCounter is incremented on an expired timer
 * and decremented for each stopped timer (in accord with remaining time in timer
 *                                         computing  the % of remaining time in the timer on current timeout value)
 *
 * ADAPTION UP->timer incremented on adaptiveTimerCounter >= ADAPTION_UP_THRESHOLD
 * ADAPTION DOWN-> timer decremented on adaptiveTimerCounter <= ADAPTION_DOWN_THRESHOLD
 *
 * called when stopping timer (stopTimer) or checking expired timers
 */
///ADAPTION COSTANTS
const struct timeval ADAPTIONUP={.tv_usec=2,.tv_sec=0};
const struct timeval ADAPTIONDOWN={.tv_usec=1,.tv_sec=0};
///TIMEOUTS CONFIGS
//adaptive timeout rail (min and MAX) with sensibility in microseconds
#define RAIL_MIN_TIMEOUT_USEC 1
#define RAIL_MIN_TIMEOUT_SEC 0
#define RAIL_MAX_TIMEOUT_USEC 10000                 //.01 sec
#define RAIL_MAX_TIMEOUT_SEC 0
//start timeout with sensibility in nanosecond
#ifndef TIMEOUTFIX
#define START_TIMEOUT_NANO 10000                    //10 usec
#else
#define START_TIMEOUT_NANO 1000                    //10 usec
#endif
#define START_TIMEOUT_SEC 0
//threshold to trigger timeout adaption
#define ADAPTATION_UP_THRESHOLD   ( 10)
#define ADAPTATION_DOWN_THRESHOLD (-10)
//adapte timeout in accord described heuristic
void timeout_adaptation(struct timers_scheduled* scheduledStruct,
                        struct itimerspec* remainingTimer);

//check expired timers and retrasmit return MACRO RESULT_SUCCES OR FAIL
int pollTimers(int socket,struct circularBuf* cbuf,struct timers_scheduled* timersScheduled,int lastPckIndexToCheck);

//return reamiain time of gived timer pointer and write result less an epsilon in nanosecs on remaining_dst
int reamaining_timers(struct timespec* remaining_dst,struct circularBuf*cbuf,struct timers_scheduled* timersScheduled);

//init timers structures return MACRO RESULT_SUCCES OR FAIL
int initTimers(struct circularBuf* cbuf,struct timers_scheduled* timersScheduled,volatile Pck** firstScheduledPck,int*totPckSent);
//relase allocated timers structures... return MACRO RESULT_SUCCES OR FAIL
int freeTimers(struct circularBuf* cbuf,struct timers_scheduled* timersScheduled);


#endif
